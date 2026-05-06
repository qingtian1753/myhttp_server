#include "http_response.h"
#include <fstream>
#include <sstream>

namespace HttpResponse{

std::string buildResponse(  int statusCode,
                            const std::string& statusText,
                            const std::string& contentType,
                            const std::string& body,
                            bool keepAlive)
{
    std::string response;
    response += "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: " + std::string(keepAlive ? "keep-alive" : "close") + "\r\n";
    response += "\r\n";
    response += body;
    return response;
}

std::string buildJsonResponse(  int statusCode,
                                const std::string& statusText,
                                const std::string& body,
                                bool keepAlive)
{
    return buildResponse(statusCode, statusText, "application/json", body, keepAlive);
}

std::string buildFileResponse(  int statusCode,
                                const std::string& statusText,
                                const std::string& filePath,
                                const std::string& body,
                                bool keepAlive)
{
    return buildResponse(statusCode, statusText, getContentType(filePath), body, keepAlive);
}

//返回错误页面
std::string buildErrorPage( int statusCode,
                            const std::string& statusText,
                            const std::string& pagePath,
                            bool keepAlive)
{
    std::ifstream file(pagePath, std::ios::binary);
    std::ostringstream oss;
    if (file) {
        oss << file.rdbuf();
    } else {
        oss << "<html><body><h1>" << statusCode << " " << statusText << "</h1></body></html>";
    }
    return buildFileResponse(statusCode, statusText, ".html", oss.str(), keepAlive);
}

std::string getContentType(const std::string& path)
{
    if (path.find(".html") != std::string::npos) return "text/html";
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".json") != std::string::npos) return "application/json";
    if (path.find(".js") != std::string::npos) return "application/javascript";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (path.find(".gif") != std::string::npos) return "image/gif";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".ico") != std::string::npos) return "image/x-icon";
    return "text/plain";
}

std::string buildJsonResponseWithCookie(
    int statusCode,
    const std::string& statusText,
    const std::string& jsonBody,
    const std::string& cookie,
    bool keepAlive)
{
    std::ostringstream oss;
    oss<<"HTTP/1.1 "<<statusCode<<" "<<statusText<<"\r\n";
    oss<<"Content-Type: application/json\r\n";
    oss<<"Content-Length: "<<jsonBody.size() << "\r\n";
    if(!cookie.empty())
    {
        oss<<"Set-Cookie: "<<cookie<<"\r\n";
    }
    oss << "Connection: "<<(keepAlive?"keep-alive":"close")<<"\r\n";
    oss << "\r\n";
    oss << jsonBody;
    return oss.str();

}
}
