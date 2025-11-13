#include "httpResponse.h"

const std::unordered_map<string, string> HttpResponse::SUFFIX_TYPE = 
{
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, string> HttpResponse::CODE_STATUS = 
{
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, string> HttpResponse::CODE_PATH = 
{
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse()
:m_code(-1), m_isKeepAlive(false), m_path(""), m_strDir(""), m_fileAddr(nullptr)
{

}

HttpResponse::~HttpResponse()
{
    unmap();
}

void HttpResponse::init(const string& srcDir, string& path, bool isKeepAlive, int code)
{
    assert(srcDir != "");
    if(m_fileAddr)
    {
        unmap();
    }

    m_code = code;
    m_isKeepAlive = isKeepAlive;
    m_path = path;
    m_strDir = srcDir;
    m_fileAddr = nullptr;
    m_fileStat = { 0 };
}

void HttpResponse::makeResponse(Buffer& buff)
{
    // 1.检查请求的文件是否存在，是否为目录，是否有权限读
    if(stat((m_strDir + m_path).data(), &m_fileStat) < 0 || S_ISDIR(m_fileStat.st_mode))
    {
        m_code = 404;   // 文件不存在或者为目录
    }
    else if(!(m_fileStat.st_mode & S_IROTH))
    {
        m_code = 403;   // 没有读权限
    }
    else if(m_code == -1)
    {
        m_code = 200;   // 初始状态码，默认为成功
    }

    // 2.处理错误页面(若状态码为错误码，映射到相应的错误页面)
    errorHtml();

    // 3.拼接相应行，响应头、响应体到缓冲区
    addStateLine(buff);
    addHeader(buff);
    addContent(buff);
}

char* HttpResponse::fileAddr() const
{
    return m_fileAddr;
}

size_t HttpResponse::fileLen() const
{
    return m_fileStat.st_size;
}

void HttpResponse::errorHtml()
{
    if(CODE_PATH.count(m_code))
    {
        m_path = CODE_PATH.find(m_code)->second;   // 映射错误页面路径
        stat((m_strDir + m_path).data(), &m_fileStat);  // 更新文件状态
    }
}

void HttpResponse::addStateLine(Buffer& buff)
{
    string status;
    if(CODE_STATUS.count(m_code))
    {
        status = CODE_STATUS.find(m_code)->second;
    }
    else
    {
        m_code = 400;   // 未知状态-> 400
        status = CODE_STATUS.find(m_code)->second;
    }

    string line = "HTTP/1.1 " + std::to_string(m_code) + " " + status + "\r\n";
    buff.insert(line);
}

void HttpResponse::addHeader(Buffer& buff)
{
    buff.insert("Connection: ");
    if(m_isKeepAlive)
    {
        buff.insert("Keep-Alive\r\n");
        buff.insert("Keep-Alive: max=6, timeout=120\r\n");  // 长连接参数
    }
    else
    {
        buff.insert("close\r\n");
    }

    buff.insert("Content-type: " + getFileType() + "\r\n");
}

void HttpResponse::addContent(Buffer& buff)
{
    int fd = open((m_strDir + m_path).data(), O_RDONLY);
    if(fd < 0)
    {
        errorContent(buff, "File NotFound!");
        return;
    }
    
    int* ret = (int*) mmap(0, m_fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(*ret == -1)
    {
        errorContent(buff, "file NotFound!");
        return;
    } 
    m_fileAddr = (char*)ret;
    close(fd);

    buff.insert("Content-length: " + std::to_string(m_fileStat.st_size) + "\r\n\r\n");
}

void HttpResponse::unmap()
{
    if(m_fileAddr)
    {
        munmap(m_fileAddr, m_fileStat.st_size);
        m_fileAddr = nullptr;
    }
}

string HttpResponse::getFileType()
{
    string::size_type idx = m_path.find_last_of(".");
    if(idx == string::npos)
    {
        return "text/plain";
    }

    string suffix = m_path.substr(idx);
    if(SUFFIX_TYPE.count(suffix))
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }

    return "text/plain";
}

void HttpResponse::errorContent(Buffer& buff, string message)
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.find(m_code)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(m_code) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.insert("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.insert(body);
}
