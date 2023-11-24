#include <algorithm>
#include <cctype>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "../../include/http/http_message.h"

namespace httpserver {

    HttpMessage::HttpMessage() : _version(HttpVersion::HTTP_11) {}  // version is HTTP/1.1

    void HttpMessage::SetHeader(const std::string& key, const std::string& value) {
        if (!key.empty()) {
            _headers[key] = value;
        }
    }

    void HttpMessage::RemoveHeader(const std::string& key) {
        _headers.erase(key);
    }

    void HttpMessage::ClearHeaders() {
        _headers.clear();
    }

    void HttpMessage::SetContent(const std::string& content) {
        _content = content;
        SetHeader("Content-Length", std::to_string(_content.length()));
    }

    void HttpMessage::ClearContent() {
        _content.clear();
        SetHeader("Content-Length", "0");
    }

    HttpVersion HttpMessage::GetVersion() const {
        return _version;
    }

    std::string HttpMessage::GetHeader(const std::string& key) const {
        auto it = _headers.find(key);
        return it != _headers.end() ? it->second : std::string();
    }

    const std::map<std::string, std::string>& HttpMessage::GetHeaders() const {
        return _headers;
    }

    const std::string& HttpMessage::GetContent() const {
        return _content;
    }

    size_t HttpMessage::GetContentLength() const {
        return _content.length();
    }

    void HttpRequest::SetMethod(HttpMethod method) {
        _method = method;
    }

    void HttpRequest::SetURI(const URI& uri) {
        _uri = uri;
    }

    HttpMethod HttpRequest::GetMethod() const {
        return _method;
    }

    const URI& HttpRequest::GetURI() const {
        return _uri;
    }

    HttpResponse::HttpResponse(HttpStatusCode statusCode) : _statusCode(statusCode) {
        if (statusCode == HttpStatusCode::NoContent) {
            ClearContent();
        }
    }

    void HttpResponse::SetStatusCode(HttpStatusCode statusCode) {
        _statusCode = statusCode;
        if (statusCode == HttpStatusCode::NoContent) {
            ClearContent();
        }
    }

    HttpStatusCode HttpResponse::GetStatusCode() const {
        return _statusCode;
    }

}

