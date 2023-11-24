#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../../include/http/http_server.h"
#include "../../include/http/uri.h"
#include "../../include/utils/serialize.h"

namespace httpserver {

    HttpServer::HttpServer(const std::string &host, std::uint16_t port) : 
        _host(host),
        _port(port),
        _socketFd(0),
        _running(false),
        _workerEpollFd(),
        _randomGenerator(std::chrono::steady_clock::now().time_since_epoch().count()),
        _sleepTimeRange(config::SLEEP_TIME_MIN, config::SLEEP_TIME_MAX) {
        CreateSocket();
    }

    void HttpServer::Start() {
        int opt = 1;
        sockaddr_in serverAddress;

        if (setsockopt(_socketFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error("Failed to set socket options");
        }

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        inet_pton(AF_INET, _host.c_str(), &(serverAddress.sin_addr.s_addr));
        serverAddress.sin_port = htons(_port);

        if (bind(_socketFd, (sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
            throw std::runtime_error("Failed to bind to socket");
        }

        if (listen(_socketFd, config::BACKLOG_SIZE) < 0) {
            std::ostringstream msg;
            msg << "Failed to listen on port " << _port;
            throw std::runtime_error(msg.str());
        }

        Initialize();
        _running = true;
        _listenerThread = std::thread(&HttpServer::Listen, this);
        for (int i = 0; i < config::WORKER_POOL_SIZE; ++i) {
            _workerThreads[i] = std::thread(&HttpServer::ProcessEvents, this, i);
        }
    }

    void HttpServer::Stop() {
        _running = false;
        
        if (_listenerThread.joinable()) {
            _listenerThread.join();
        }
        
        for (int i = 0; i < config::WORKER_POOL_SIZE; ++i) {
            if (_workerThreads[i].joinable()) {
                _workerThreads[i].join();
            }
        }
        
        for (int i = 0; i < config::WORKER_POOL_SIZE; ++i) {
            if (_workerEpollFd[i] >= 0) {
                close(_workerEpollFd[i]);
            }
        }
        
        if (_socketFd >= 0) {
            close(_socketFd);
        }
    }

    void HttpServer::CreateSocket() {
        if ((_socketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
            throw std::runtime_error("Failed to create a TCP socket");
        }
    }

    void HttpServer::Initialize() {
        for (int i = 0; i < config::WORKER_POOL_SIZE; ++i) {
            if ((_workerEpollFd[i] = epoll_create1(0)) < 0) {
                throw std::runtime_error(
                    "Failed to create epoll file descriptor for worker");
            }
        }
    }

    void HttpServer::Listen() {
        EventData *clientData;
        sockaddr_in clientAddress;
        socklen_t clientLen = sizeof(clientAddress);
        int clientFd;
        int currentWorker = 0;
        bool active = true;

        while (_running) {
            if (!active) {
                // Random sleep to prevent thundering herd problem
                std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeRange(_randomGenerator)));
            }
            
            clientFd = accept4(_socketFd, (sockaddr *)&clientAddress, &clientLen, SOCK_NONBLOCK);
            if (clientFd < 0) {
                active = false;
                continue;
            }

            active = true;
            clientData = new EventData();
            clientData->fd = clientFd;
            ControlEvent(_workerEpollFd[currentWorker], EPOLL_CTL_ADD, clientFd, EPOLLIN, clientData);
            if (++currentWorker == config::WORKER_POOL_SIZE) 
                currentWorker = 0;
        }
    }

    void HttpServer::ProcessEvents(int workerId) {
        EventData *data;
        int epollFd = _workerEpollFd[workerId];
        bool active = true;

        while (_running) {
            if (!active) {
                // Random sleep to prevent thundering herd problem
                std::this_thread::sleep_for(std::chrono::microseconds(_sleepTimeRange(_randomGenerator)));
            }
            int ec = epoll_wait(epollFd, _workerEvents[workerId], config::MAX_EVENTS, 0);
            if (ec <= 0) {
                active = false;
                continue;
            }

            active = true;
            for (int i = 0; i < ec; ++i) {
                const epoll_event &currentEvent = _workerEvents[workerId][i];
                data = reinterpret_cast<EventData*>(currentEvent.data.ptr);
                if ((currentEvent.events & EPOLLHUP) ||
                    (currentEvent.events & EPOLLERR)) {
                    ControlEvent(epollFd, EPOLL_CTL_DEL, data->fd);
                    close(data->fd);
                    delete data;
                } else if (currentEvent.events == EPOLLIN) {
                    Receive(epollFd, data);
                } else if (currentEvent.events == EPOLLOUT) {
                    Send(epollFd, data);
                } else {
                    ControlEvent(epollFd, EPOLL_CTL_DEL, data->fd);
                    close(data->fd);
                    delete data;
                }
            }
        }
    }

    void HttpServer::Receive(int epollFd, EventData* data) {
        int fd = data->fd;
        EventData* request = data;
        ssize_t byteCount = recv(fd, request->buffer, config::MAX_BUFFER_SIZE, 0);
        
        if (byteCount > 0) {
            request->length = byteCount;
            EventData* response = new EventData();
            response->fd = fd;
            ProcessData(*request, response);
            ControlEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
            delete request;
        } else if (byteCount == 0) {
            ControlEvent(epollFd, EPOLL_CTL_DEL, fd);
            close(fd);
            delete request;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                request->fd = fd;
                ControlEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
            } else {
                ControlEvent(epollFd, EPOLL_CTL_DEL, fd);
                close(fd);
                delete request;
            }
        }
    }

    void HttpServer::Send(int epollFd, EventData *data) {
        int fd = data->fd;
        EventData* response = data;
        ssize_t byteCount = send(fd, response->buffer + response->cursor, response->length, 0);
        
        if (byteCount >= 0) {
            if (byteCount < response->length) {
                response->cursor += byteCount;
                response->length -= byteCount;
                ControlEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
            } else {
                // HTTP keep-alive, reuse connection for next request
                EventData *request = new EventData();
                request->fd = fd;
                ControlEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
                delete response;
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ControlEvent(epollFd, EPOLL_CTL_ADD, fd, EPOLLOUT, response);
            } else {
                ControlEvent(epollFd, EPOLL_CTL_DEL, fd);
                close(fd);
                delete response;
            }
        }
    }

    void HttpServer::ControlEvent(int epollFd, int op, int fd,
                                       std::uint32_t events, void *data) {
        if (op == EPOLL_CTL_DEL) {
            if (epoll_ctl(epollFd, op, fd, nullptr) < 0) {
                throw std::runtime_error("Failed to remove file descriptor");
            }
        } else {
            epoll_event ev;
            ev.events = events;
            ev.data.ptr = data;
            if (epoll_ctl(epollFd, op, fd, &ev) < 0) {
                throw std::runtime_error("Failed to add file descriptor");
            }
        }
    }

    void HttpServer::ProcessData(const EventData &rawRequest,
                                    EventData *rawResponse) {
        std::string requestStr(rawRequest.buffer, rawRequest.length), responseStr;
        HttpRequest request;
        HttpResponse response;

        try {
            request = FromString<HttpRequest>(requestStr);
            response = HandleRequest(request);
        } 
        catch (const std::invalid_argument &e) {
            response = HttpResponse(HttpStatusCode::BadRequest);
            response.SetContent(e.what());  
        } 
        catch (const std::logic_error &e) {
            response = HttpResponse(HttpStatusCode::HttpVersionNotSupported);
            response.SetContent(e.what());
        } 
        catch (const std::exception &e) {
            response = HttpResponse(HttpStatusCode::InternalServerError);
            response.SetContent(e.what());
        }

        responseStr = ToString(response);
        size_t responseLength = std::min(responseStr.length(), config::MAX_BUFFER_SIZE - 1);
        memcpy(rawResponse->buffer, responseStr.c_str(), responseLength);
        rawResponse->length = responseLength;
    }

    HttpResponse HttpServer::HandleRequest(const HttpRequest &request) {
        auto it = _requestHandlers.find(request.GetURI().GetPath());
        if (it == _requestHandlers.end()) {
            return HttpResponse(HttpStatusCode::NotFound);
        }
        
        auto callbackIt = it->second.find(request.GetMethod());
        if (callbackIt == it->second.end()) {
            return HttpResponse(HttpStatusCode::MethodNotAllowed);
        }
        return callbackIt->second(request);
    }

    void HttpServer::RegisterRequestHandler(std::string path, HttpMethod method, const HttpRequestHandler callback) {
        if (path.empty() || path[0] != '/') {
            path = "/" + path;
        }
        _requestHandlers[path].insert(std::make_pair(method, std::move(callback)));
    }

    std::string HttpServer::GetHost() const { 
        return _host; 
    }

    std::uint16_t HttpServer::GetPort() const { 
        return _port; 
    }

    bool HttpServer::IsRunning() const { 
        return _running; 
    }
}