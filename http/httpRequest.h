#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <cerrno>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"

using std::string;



class HttpRequest
{

public:
    /* 主状态机的状态 */
    enum CHECK_STATE 
    {
        CHECK_REQUESTLINE = 0,   // 检查请求行
        CHECK_HEADER,            // 检查请求头
        CHECK_CONTENT,           // 检查消息体
        CHECK_FINISH,            // 结束状态
    };

    /* 处理HTTP的结果*/
    enum HTTP_CODE 
    {
        NO_REQUEST,         // 请求不完整，需要继续读取数据
        GET_REQUEST,        // 获得一个完整的请求
        BAD_REQUEST,        // 请求格式错误
        NO_RESOURCE,        // 没有这个资源
        FILE_REQUETS,       // 文件资源
        FORBIDDEN_REQUEST,  // 客户对资源没有权限
        INTERNAL_ERROR,     // 服务器内部错误
        CLOSED_CONNECTION   // 客户端已经关闭连接

    };

public:
    HttpRequest() { init();};
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer& buff);

    string path() const;
    string& path();
    string method() const;
    string version() const;
    string getPostByKey(const string& key) const;
    string getPostByKey(const char* key) const;

    bool isKeepAlive() const;



private:
    bool parseRequstLine(const string& text);
    bool parseHeader(const string& text);
    bool parseBody(const string& text);

    void parsePath();
    void parsePostReq();
    void parseFromUrlEncoded();         // 处理 URL 编码

    static bool userVerify(const string& name, const string& pwd, bool isLogin);
    static int converHex(char ch);      // 编码转换




private:
    CHECK_STATE m_curState;   // 记录当前状态

    string m_mthod;
    string m_path;
    string m_version;
    string m_body;

    std::unordered_map<string, string> m_header;    // 存储请求头键值对
    std::unordered_map<string, string> m_userInfo;    // 存在用户名和密码键值对

    static const std::unordered_set<string> DEFAULT_HTML;       // 存储默认的HTML路径
    static const std::unordered_map<string, int> DEFAULT_HTML_TAG;      // 存储路径与标签映射关系

};

#endif