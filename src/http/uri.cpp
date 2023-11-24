#include "../../include/http/uri.h"
#include <algorithm>
#include <cctype>

namespace httpserver {

    URI::URI(const std::string& uri) {
        Parse(uri);
    }

    bool URI::operator==(const URI& other) const {
        return _path == other._path && _query == other._query;
    }

    bool URI::operator<(const URI& other) const {
        if (_path != other._path) return _path < other._path;
        return _query < other._query;
    }

    void URI::Parse(std::string uri) {
        std::transform(uri.begin(), uri.end(), uri.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        size_t queryPos = uri.find('?');
        if (queryPos != std::string::npos) {
            _path = uri.substr(0, queryPos);
            _query = uri.substr(queryPos + 1);
        } else {
            _path = uri;
        }

        if (_path.empty() || _path[0] != '/') {
            _path = "/" + _path;
        }
    }

    std::string URI::GetPath() const {
        return _path;
    }

    std::string URI::GetQuery() const {
        return _query;
    }

    bool URI::IsValid() const {
        return !_path.empty();
    }

}
