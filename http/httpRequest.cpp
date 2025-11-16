#include "httpRequest.h"

const std::unordered_set<string> HttpRequest::DEFAULT_HTML
{
    "/index", "/register", "/login", "/welcome", "/video",
    "/picture",
};

const std::unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG
{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::init()
{
    m_mthod = m_path = m_version = m_body = "";
    m_curState = CHECK_REQUESTLINE;
    m_header.clear();
    m_userInfo.clear();
}

bool HttpRequest::parse(Buffer& buff)
{
    const char CRLF[] = "\r\n";
    if(buff.writeableBytes() <= 0) return false;

    while(buff.readableBytes() && m_curState != CHECK_FINISH)
    {
        // 查找当前行的结束符
        const char* lineEnd = std::search(buff.readBegin(), buff.writeBeginConst(), CRLF, CRLF + 2);

        // 提取当前行
        string line = string(buff.readBegin(), lineEnd);
        switch(m_curState)
        {
            case CHECK_REQUESTLINE:
            {
                if(!parseRequstLine(line))
                {
                    return false;
                }

                // 处理请求路径（如补全 .html 后缀）
                parsePath();
                break;
            }

            case CHECK_HEADER:
            {
                if(!parseHeader(line))
                {
                    return false;
                }

                // 若缓冲区剩余数据仅够 CRLF，说明请求头结束，对应GET请求
                if(buff.writeableBytes() <= 2)
                {
                    m_curState = CHECK_FINISH;
                }

                break;
            }

            case CHECK_CONTENT:
            {
                if(!parseBody(line))
                {
                    return false;
                }
                break;
            }

            default:
                break;
        }

        // 说明数据已经处理完了
        if(lineEnd == buff.writeBegin())
        {
            break;
        }

        // 移动读指针,+2的含义是每行后面的 /r/n 需要跳过
        buff.advance(lineEnd + 2);
    }
#ifdef DEBUG
        std::cout << "[ "<< m_mthod << "  " << m_path << "  " << m_version  << "]" << std::endl;
#endif 

    return true;
}

void HttpRequest::parsePath()
{
    if(m_path == "/")
    {
        m_path = "/index.html";
    }
    else
    {
        for(auto& i : DEFAULT_HTML)
        {
            if(i == m_path)
            {
                m_path += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::parseRequstLine(const string& line)
{
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMath;
    if(std::regex_match(line, subMath, patten))
    {
        m_mthod = subMath[1];
        m_path = subMath[2];
        m_version = subMath[3];
        m_curState = CHECK_HEADER;
        return true;
    }

#ifdef DEBUG
        std::cout << "requestLine error!" << std::endl;
#endif 

    return false;
}

bool HttpRequest::parseHeader(const string& line)
{
    // 匹配 "Key: Value"
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMath;

    if(std::regex_match(line, subMath, patten))
    {
        m_header[subMath[1]] = subMath[2];
    }
    else
    {
        // 空行 → 请求头结束，切换到解析 body
        m_curState = CHECK_CONTENT;
    }

    return true;
}

bool HttpRequest::parseBody(const string& line)
{
#ifdef DEBUG
    std::cout << "[post content] " << line << std::endl;
#endif
    m_body = line;
    // 处理post提交的数据
    parsePostReq();
    m_curState = CHECK_FINISH;
    return true;
}

int HttpRequest::converHex(char ch)
{   
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

void HttpRequest::parsePostReq()
{
    if(m_mthod == "POST" && m_header["Content-Type"] == "application/x-www-form-urlencoded")
    {
        // 解析 URL 编码的表单数据
        parseFromUrlEncoded();

        if(DEFAULT_HTML_TAG.count(m_path))
        {
            int flag = DEFAULT_HTML_TAG.find(m_path)->second;
            if(flag == 0 || flag == 1)
            {
                bool isLogin = (flag == 1);
                if(userVerify(m_userInfo["username"], m_userInfo["password"], isLogin))
                {
                    m_path = "/welcome.html";
                }
                else
                {
                    m_path = "/error.html";
                }
            }
        }

    }
}

void HttpRequest::parseFromUrlEncoded()
{
    if(m_body.size() == 0) return;

    string key ,value;

    int num = 0;
    int n = m_body.size();
    int i = 0, j = 0;

    // 处理表单数据中的特殊字符（如 + 转空格、%XX 转字符）
    for(; i < n; ++i)
    {
        char ch = m_body[i];
        switch(ch)
        {
            case '=':
            {
                key = m_body.substr(j, i - j);
                j = i + 1;
                break;
            }

            case '+' :
            {
                m_body[i] = ' ';
                break;
            }

            case '%' :
            {
                num = converHex(m_body[i + 1]) * 16 + converHex(m_body[i + 2]);
                m_body[i + 2] = num % 10 + '0';
                m_body[i + 1] = num / 10 + '0';

                i += 2;
                break;
            }

            case '&' :
            {
                value = m_body.substr(j, i - j);
                j = i + 1;
                m_userInfo[key] = value;
                break;
            }
            default :
                break;
        }
    }

    assert(j <= i);
    // 处理最后一组键值对
    if(m_userInfo.count(key) == 0 && j < i)
    {
        value = m_body.substr(j, i - j);
        m_userInfo[key] = value;
    }
}

bool HttpRequest::userVerify(const string& name, const string& pwd, bool isLogin)
{
    if(name.empty() || pwd.empty() || name == " " || pwd == " " )
    {
#ifdef DEBUG
        std::cout << "name or password error " << std::endl;
#endif 
        return false;
    } 
#ifdef DEBUG
        std::cout << "name: " << name << " password: " << pwd << std::endl;
#endif 

    std::shared_ptr<MysqlConnection> mysql = DbConnsPool::getInstance()->getConnection();

    assert(mysql);

    bool flag = !isLogin ? true : false;
    char sql[256] = { 0 };

    snprintf(sql, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    
    MYSQL_RES* res = mysql->query(sql);
    if(res == nullptr)
    {
#ifdef DEBUG
        std::cout << "mysql query error!" << std::endl;
#endif 
        return false;
    }
    
    while(MYSQL_ROW row = mysql_fetch_row(res))
    {
        string passwd(row[1]);

        if(isLogin)
        {
            if(pwd == passwd) { flag = true; }
            else
            {
                flag = false;
            }

        }
    }

    if(!isLogin && flag)
    {
        bzero(sql, 256);
        snprintf(sql, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        if(!mysql->update(sql))
        {
#ifdef DEBUG
        std::cout << "insert into error!" << std::endl;
#endif 
            return false;
        }
    }
    return flag;
}

string HttpRequest::path() const
{
    return m_path;
}

string& HttpRequest::path()
{
    return m_path;
}

string HttpRequest::method() const
{
    return m_mthod;
}

string HttpRequest::version() const
{
    return m_version;
}

string HttpRequest::getPostByKey(const string& key) const
{
    assert(key != "");

    if(m_userInfo.count(key))
    {
        return m_userInfo.find(key)->second;
    }

    return "";
}

string HttpRequest::getPostByKey(const char* key) const
{
    assert(key != nullptr);
    
    if(m_userInfo.count(key))
    {
        return m_userInfo.find(key)->second;
    }

    return "";
}

bool HttpRequest::isKeepAlive() const
{
    if(m_header.count("Connection"))
    {
        return m_header.find("Connection")->second == "keep-alive" && m_version == "1.1";
    }

    return false;
}