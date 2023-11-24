#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <chrono>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <thread>

#include "http_message.h"
#include "uri.h"
#include "http_server_config.h"

namespace httpserver {

    struct EventData {
        int fd;
        size_t length;
        size_t cursor;
        char buffer[config::MAX_BUFFER_SIZE];
        EventData() : fd(0), length(0), cursor(0), buffer() {}
    };

    using HttpRequestHandler = std::function<HttpResponse(const HttpRequest&)>;

    class HttpServer {
    public:
        explicit HttpServer(const std::string& host, std::uint16_t port);
        HttpServer(HttpServer&&) noexcept;
        HttpServer& operator=(HttpServer&&) noexcept;
        HttpServer() = default;
        ~HttpServer() = default;

        void Start();
        void Stop();
        void RegisterRequestHandler(std::string path, HttpMethod method, const HttpRequestHandler callback);

        std::string GetHost() const;
        std::uint16_t GetPort() const;
        bool IsRunning() const;

    private:
        std::string _host;
        std::uint16_t _port;
        int _socketFd;
        bool _running;

        std::thread _listenerThread;
        std::thread _workerThreads[config::WORKER_POOL_SIZE];
        
        int _workerEpollFd[config::WORKER_POOL_SIZE];
        epoll_event _workerEvents[config::WORKER_POOL_SIZE][config::MAX_EVENTS];
        std::map<std::string, std::map<HttpMethod, HttpRequestHandler>> _requestHandlers;
        std::mt19937 _randomGenerator;
        std::uniform_int_distribution<int> _sleepTimeRange;

        void CreateSocket();
        void Initialize();
        void Listen();
        void ProcessEvents(int workerId);
        void Receive(int epollFd, EventData* data);
        void Send(int epollFd, EventData* data);
        void ControlEvent(int epollFd, int op, int fd, std::uint32_t events = 0, void* data = nullptr);
        void ProcessData(const EventData& request, EventData* response);
        HttpResponse HandleRequest(const HttpRequest& request);
    };
}