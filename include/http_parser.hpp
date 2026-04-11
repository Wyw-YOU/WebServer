#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
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
        if(headers.count("Connection"))
        {
            return headers.at("Connection") == "keep-alive";
        }
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

        PARSE_STATE state = REQUEST_LINE;

        size_t pos = 0;

        size_t lineEnd;

        while((lineEnd = request.find("\r\n",pos)) != std::string::npos)
        {

            std::string line = request.substr(pos,lineEnd-pos);

            pos = lineEnd + 2;

            // 空行说明header结束
            if(line.empty())
            {
                state = BODY;
                break;
            }

            if(state == REQUEST_LINE)
            {

                parseRequestLine(line,req);

                state = HEADERS;

            }
            else if(state == HEADERS)
            {

                parseHeader(line,req);

            }

        }

        // body解析
        if(pos < request.size())
        {

            req.body = request.substr(pos);

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