#pragma once

namespace httpserver {
    namespace config {
        // Buffer settings
        constexpr size_t MAX_BUFFER_SIZE = 4096;        // Max bytes per HTTP request/response
        
        // Server settings
        constexpr int BACKLOG_SIZE = 1000;              // Pending connections queue size
        constexpr int MAX_CONNECTIONS = 10000;          // Total concurrent connections
        constexpr int MAX_EVENTS = 2048;                // Max epoll events per worker
        constexpr int WORKER_POOL_SIZE = 8;             // Number of worker threads
        
        // Sleep time settings
        constexpr int SLEEP_TIME_MIN = 10;              // Min sleep time (us) 
        constexpr int SLEEP_TIME_MAX = 100;             // Max sleep time (us)
    }
}
