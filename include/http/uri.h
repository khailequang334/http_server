#pragma once
#include <string>

namespace httpserver {
    // Handle only path and query, ignore scheme, authority, and fragment
    class URI {
    public:
        URI() = default;
        explicit URI(const std::string& uri);
        ~URI() = default;

        bool operator==(const URI& other) const;
        bool operator<(const URI& other) const;

        void Parse(std::string uri);

        std::string GetPath() const;
        std::string GetQuery() const;

        bool IsValid() const;

    private:
        std::string _path;
        std::string _query;
    };
}