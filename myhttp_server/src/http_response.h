#pragma once
#include <string>

namespace HttpResponse {
    std::string buildResponse(  int statusCode,
                                const std::string& statusText,
                                const std::string& contentType,
                                const std::string& body,
                                bool keepAlive);

    std::string buildJsonResponse(  int statusCode,
                                    const std::string& statusText,
                                    const std::string& body,
                                    bool keepAlive);

    std::string buildFileResponse(  int statusCode,
                                    const std::string& statusText,
                                    const std::string& filePath,
                                    const std::string& body,
                                    bool keepAlive);

    std::string buildErrorPage( int statusCode,
                                const std::string& statusText,
                                const std::string& pagePath,
                                bool keepAlive);

    std::string getContentType(const std::string& path);
};
