#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "../include/http/http_server.h"
#include "../include/http/http_message.h"
#include "../include/http/uri.h"

using httpserver::HttpMethod;
using httpserver::HttpRequest;
using httpserver::HttpResponse;
using httpserver::HttpServer;
using httpserver::HttpStatusCode;

// Handle Ctrl+C and kill signals
std::atomic<bool> gRunning{true};
void terminateHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
        gRunning = false;
    }
}

int main() {
    signal(SIGINT, terminateHandler);
    signal(SIGTERM, terminateHandler);
    
    HttpServer server("0.0.0.0", 8080);

    auto test = [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response(HttpStatusCode::OK);
        response.SetHeader("Content-Type", "text/plain");
        response.SetContent("test\n");
        return response;
    };

    server.RegisterRequestHandler("/", HttpMethod::HEAD, test);
    server.RegisterRequestHandler("/", HttpMethod::GET, test);

    try {
        server.Start();
        std::cout << "Server listening on 0.0.0.0:8080" << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        
        while (gRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        server.Stop();
    } 
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
