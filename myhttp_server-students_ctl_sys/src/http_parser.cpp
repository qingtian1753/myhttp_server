#include "http_parser.h"
#include <sstream>
#include <stdexcept>

//匿名命名空间，只在此文件用，类似于static
namespace {
constexpr size_t MAX_BODY_SIZE = 1024 * 1024;
}

HttpParser::ParseStatus HttpParser::tryParseOne(std::string& readBuffer, HttpRequest& req)
{
    size_t headerEnd = readBuffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return ParseStatus::Incomplete;
    }

    size_t headerLength = headerEnd + 4;
    std::string rawHeader = readBuffer.substr(0, headerLength);

    HttpRequest temp;
    if (!parseRequestHeader(rawHeader, temp)) {
        return ParseStatus::BadRequest;
    }

    size_t bodyLength = 0;
    //判断是否有内容长度这一条，如果有说明是post请求
    auto it = temp.headers.find("Content-Length");
    if (it != temp.headers.end()) {
        try {
            //用较大的longlong来接收长度，如果是负数或者超出了最大值那么直接返回
            long long len = std::stoll(it->second);
            if (len < 0 || static_cast<size_t>(len) > MAX_BODY_SIZE) {
                return ParseStatus::BadRequest;
            }
            bodyLength = static_cast<size_t>(len);
        } catch (...) {
            //如果捕获到了异常那么直接返回坏请求
            return ParseStatus::BadRequest;
        }
    }

    //数据没来全那么返回，等待下一次接收数据
    if (readBuffer.size() < headerLength + bodyLength) {
        return ParseStatus::Incomplete;
    }

    temp.body = readBuffer.substr(headerLength, bodyLength);
    readBuffer.erase(0, headerLength + bodyLength);
    req = std::move(temp);
    return ParseStatus::Complete;
}

bool HttpParser::parseRequestHeader(const std::string& rawHeader, HttpRequest& req)
{
    std::istringstream iss(rawHeader);
    std::string line;

    if (!std::getline(iss, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (!parseRequestLine(line, req)) {
        return false;
    }

    while (std::getline(iss, line)) {
        //不出意外每一句都是以\r\n结尾的,所以要pop掉'\r'
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        if (!parseHeaderLine(line, req)) {
            return false;
        }
    }
    return true;
}

bool HttpParser::parseRequestLine(const std::string& line, HttpRequest& req)
{
    std::istringstream iss(line);
    if (!(iss >> req.method >> req.path >> req.version)) {
        return false;
    }
    //只允许版本是1.1和1.0的访问
    return req.version == "HTTP/1.1" || req.version == "HTTP/1.0";
}

//除了请求头和body外的消息都是 ... : ... 所以以:为分隔
bool HttpParser::parseHeaderLine(const std::string& line, HttpRequest& req)
{
    size_t pos = line.find(':');
    if (pos == std::string::npos) {
        return false;
    }
    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));
    req.headers[key] = value;
    return true;
}

//去掉开头或者结尾的空格或制表符
std::string HttpParser::trim(const std::string& s)
{
    size_t left = 0;
    while (left < s.size() && (s[left] == ' ' || s[left] == '\t')) {
        ++left;
    }
    size_t right = s.size();
    while (right > left && (s[right - 1] == ' ' || s[right - 1] == '\t')) {
        --right;
    }
    return s.substr(left, right - left);
}
