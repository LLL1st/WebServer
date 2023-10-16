#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>    // 正则表达式
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();    // 初始化方法
    bool parse(Buffer& buff);   //解析HTTP请求

    std::string path() const;   //获取HTTP请求的路径
    std::string& path();        //设置HTTP请求的路径
    std::string method() const; //获取HTTP请求的方法（GET、POST）
    std::string version() const;    //获取HTTP请求的版本
    std::string GetPost(const std::string& key) const;  //获取POST请求中的参数
    std::string GetPost(const char* key) const; //获取POST请求中的参数

    bool IsKeepAlive() const;   //检查HTTP请求是否要求保持连接

private:
    bool ParseRequestLine_(const std::string& line);    // 处理请求行
    void ParseHeader_(const std::string& line);         // 处理请求头
    void ParseBody_(const std::string& line);           // 处理请求体

    void ParsePath_();                                  // 处理请求路径
    void ParsePost_();                                  // 处理Post事件
    void ParseFromUrlencoded_();                        // 从url种解析编码

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);  // 验证用户名和密码


    //类的私有成员变量，存储HTTP请求的状态、方法、路径、版本、主体、头部、POST参数
    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML; //静态常量无序集合，存储默认的HTML内容
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; //静态常量无序映射，存储默认的HTML标签以及对应的整数值
    static int ConverHex(char ch);  // 16进制转换为10进制，处理URL编码时需要这种转换
};

#endif