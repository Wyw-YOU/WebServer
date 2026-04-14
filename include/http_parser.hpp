#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <sstream>
#include <unordered_map>

/*
    HTTP请求结构体
*/

struct HttpRequest
{
    std::string method;   // GET / POST
    std::string url;      // 请求路径
    std::string version;  // HTTP版本

    std::unordered_map<std::string,std::string> headers; // 请求头

    std::string body;

    /*
    判断是否是长连接
    */
    bool isKeepAlive() const
    {
        auto it = headers.find("Connection");
    
        // 显式 close
        if(it != headers.end())
        {
            std::string v = it->second;
            for(auto &c : v) c = tolower(c);
    
            if(v == "close") return false;
            if(v == "keep-alive") return true;
        }
    
        // ===== 默认规则（关键！）=====
        if(version == "HTTP/1.1")
            return true;
    
        return false;
    }
};

/*
    HTTP解析状态
*/

enum PARSE_STATE
{
    REQUEST_LINE,
    HEADERS,
    BODY,
    FINISH
};

/*
    HTTP解析器
*/

class HttpParser
{

public:

    /*
        解析HTTP请求
    */
    static HttpRequest parse(const std::string& request)
    {
        HttpRequest req;

        size_t pos = request.find("\r\n\r\n");

        // ===== 1. Header 可能还不完整 =====
        if(pos == std::string::npos)
        {
            return req; // 返回空（调用方判断）
        }

        size_t headerLen = pos + 4;

        std::string headerPart = request.substr(0, headerLen);

        std::stringstream ss(headerPart);
        std::string line;

        // ===== 2. 请求行 =====
        if(std::getline(ss, line))
        {
            if(line.back() == '\r') line.pop_back();

            std::stringstream ls(line);
            ls >> req.method >> req.url >> req.version;
        }

        // ===== 3. Header =====
        while(std::getline(ss, line))
        {
            if(line == "\r" || line.empty()) break;

            if(line.back() == '\r') line.pop_back();

            size_t pos = line.find(':');
            if(pos != std::string::npos)
            {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 2);
                req.headers[key] = value;
            }
        }

        // ===== 4. Body（可能不存在）=====
        if(request.size() > headerLen)
        {
            req.body = request.substr(headerLen);
        }

        return req;
    }

private:

    /*
        解析请求行
        GET /index.html HTTP/1.1
    */
    static void parseRequestLine(const std::string& line,HttpRequest& req)
    {

        size_t pos1 = line.find(" ");

        size_t pos2 = line.find(" ",pos1+1);

        req.method = line.substr(0,pos1);

        req.url = line.substr(pos1+1,pos2-pos1-1);

        req.version = line.substr(pos2+1);

    }

    /*
        解析Header
        Host: xxx
    */
    static void parseHeader(const std::string& line,HttpRequest& req)
    {

        size_t pos = line.find(":");

        if(pos == std::string::npos)
            return;

        std::string key = line.substr(0,pos);

        std::string value = line.substr(pos+2);

        req.headers[key] = value;

    }

};

#endif