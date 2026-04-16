#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <sstream>
#include <unordered_map>

/**
 * @brief HTTP 请求结构体
 */
struct HttpRequest
{
    std::string method;   ///< 请求方法 (GET/POST)
    std::string url;      ///< 请求路径
    std::string version;  ///< HTTP 版本

    std::unordered_map<std::string, std::string> headers; ///< 请求头字段

    std::string body;     ///< 请求体

    /**
     * @brief 判断是否保持长连接
     * @return true 保持连接，false 关闭连接
     */
    bool isKeepAlive() const
    {
        auto it = headers.find("Connection");

        // 显式指定 close 或 keep-alive
        if (it != headers.end())
        {
            std::string v = it->second;
            for (auto &c : v) c = tolower(c);

            if (v == "close") return false;
            if (v == "keep-alive") return true;
        }

        // 默认规则：HTTP/1.1 默认长连接
        if (version == "HTTP/1.1")
            return true;

        return false;
    }
};

/**
 * @brief HTTP 解析状态枚举
 */
enum PARSE_STATE
{
    REQUEST_LINE,   ///< 解析请求行
    HEADERS,        ///< 解析头部
    BODY,           ///< 解析主体
    FINISH          ///< 解析完成
};

/**
 * @brief HTTP 请求解析器（静态方法类）
 */
class HttpParser
{
public:
    /**
     * @brief 解析 HTTP 请求报文
     * @param request 原始请求字符串
     * @return 解析后的 HttpRequest 对象（若头部不完整则返回空对象）
     */
    static HttpRequest parse(const std::string& request)
    {
        HttpRequest req;

        size_t pos = request.find("\r\n\r\n");

        // 1. 检查请求头是否完整
        if (pos == std::string::npos)
        {
            return req; // 返回空对象，调用方自行判断
        }

        size_t headerLen = pos + 4;
        std::string headerPart = request.substr(0, headerLen);
        std::stringstream ss(headerPart);
        std::string line;

        // 2. 解析请求行
        if (std::getline(ss, line))
        {
            if (line.back() == '\r') line.pop_back();

            std::stringstream ls(line);
            ls >> req.method >> req.url >> req.version;
        }

        // 3. 解析头部字段
        while (std::getline(ss, line))
        {
            if (line == "\r" || line.empty()) break;

            if (line.back() == '\r') line.pop_back();

            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos)
            {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 2); // 跳过 ": "
                req.headers[key] = value;
            }
        }

        // 4. 解析请求体（可能不存在）
        if (request.size() > headerLen)
        {
            req.body = request.substr(headerLen);
        }

        return req;
    }

private:
    /**
     * @brief 解析请求行（例如：GET /index.html HTTP/1.1）
     * @param line 请求行字符串
     * @param req 输出参数，填充 method、url、version
     */
    static void parseRequestLine(const std::string& line, HttpRequest& req)
    {
        size_t pos1 = line.find(" ");
        size_t pos2 = line.find(" ", pos1 + 1);

        req.method = line.substr(0, pos1);
        req.url = line.substr(pos1 + 1, pos2 - pos1 - 1);
        req.version = line.substr(pos2 + 1);
    }

    /**
     * @brief 解析单个头部字段（例如：Host: localhost）
     * @param line 头部行字符串
     * @param req 输出参数，填充 headers 映射
     */
    static void parseHeader(const std::string& line, HttpRequest& req)
    {
        size_t pos = line.find(":");
        if (pos == std::string::npos)
            return;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 2); // 跳过 ": "
        req.headers[key] = value;
    }
};

#endif // HTTP_PARSER_HPP