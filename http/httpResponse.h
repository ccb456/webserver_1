#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string>

#include "../buffer/buffer.h"

using std::string;

class HttpResponse
{

public:
    HttpResponse();
    ~HttpResponse();

    void init(const string& srcDir, string& path, bool isKeepAlive = false, int code = -1);
    void makeResponse(Buffer& buff);
    void unmap();
    char* fileAddr() const;
    size_t fileLen() const;
    void errorContent(Buffer& buff, string message);
    int code() const { return m_code;}


private:
    static const std::unordered_map<string, string> SUFFIX_TYPE;
    static const std::unordered_map<int, string> CODE_STATUS;
    static const std::unordered_map<int, string> CODE_PATH;
    
    void addStateLine(Buffer& buff);
    void addHeader(Buffer& buff);
    void addContent(Buffer& buff);
    
    void errorHtml();
    string getFileType();


private:
    int m_code;                 // HTTP响应状态码
    bool m_isKeepAlive;         // 是否保持连接

    string m_path;              // 待响应的文件路径
    string m_strDir;            // 静态资源地址

    char* m_fileAddr;           // map映射文件地址
    struct stat m_fileStat;     // 映射文件状态信息
};

#endif