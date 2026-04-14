#pragma once
#include <string>
#include "http_request.h"

struct HttpParser {
    enum class ParseStatus {
        Complete,
        Incomplete,
        BadRequest,
    };

    static ParseStatus tryParseOne(std::string& readBuffer, HttpRequest& req);

private:
    static bool parseRequestHeader(const std::string& rawHeader, HttpRequest& req);
    static bool parseRequestLine(const std::string& line, HttpRequest& req);
    static bool parseHeaderLine(const std::string& line, HttpRequest& req);
    static std::string trim(const std::string& s);
};
