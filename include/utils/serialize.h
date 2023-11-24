#pragma once
#include <string>

namespace httpserver {
    // Forward declarations
    enum class HttpMethod;
    enum class HttpVersion;
    enum class HttpStatusCode;
    class HttpRequest;
    class HttpResponse;
    class URI;
}

// Template functions for serialization
template <typename T>
std::string ToString(T value);

template <typename T>
T FromString(const std::string& str);

