#include "httprequest.h"
using namespace std;

// 存储默认的HTML内容
const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

// 存储默认的HTML标签，登录/注册
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/login.html", 1}, {"/register.html", 0}};

// 初始化操作，一些清零操作
void HttpRequest::Init() {
    state_ = REQUEST_LINE;
    method_ = path_ = version_ = body_ = "";
    header_.clear();
    post_.clear();
}

// 解析处理
bool HttpRequest::parse(Buffer& buff) {
    const char END[] = "\r\n";
    if(buff.ReadableBytes() == 0)   // 没有可读的字节
        return false;
    // 读取数据开始
    while(buff.ReadableBytes() && state_ != FINISH) { //只要buff中还有可读的字节，并且HTTP请求不是FINISH，就继续循环
        const char* lineend = search(buff.Peek(), buff.BeginWriteConst(), END, END+2); //从写指针的位置到读指针的位置搜索"\r\n", 返回指向缓冲区第一个\r\n的指针
        string line(buff.Peek(), lineend); // 将找到的一行存储在字符串中
        switch (state_)
        {
        case REQUEST_LINE:  //解析请求行
            // 解析错误
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();   // 解析路径
            break;
        case HEADERS:   //解析请求头
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) {  // 说明是get请求，后面为\r\n
                state_ = FINISH;   // 提前结束
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineend == buff.BeginWrite()) {  // 读完了
            buff.RetrieveAll();
            break;
        }
        buff.RetrieveUntil(lineend + 2);        // 跳过回车换行
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

// 解析HTTP请求行
bool HttpRequest::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); //正则表达式 ^表示行的开始，([^ ]*)匹配任何不含空格的字符序列，空格 匹配一个空格字符，HTTP/ 匹配一个HTTP/，$表示行的结束
    smatch Match;   // 定义一个std::match对象，用来存储正则表达式匹配的结果，如果字符串成功匹配正则表达式，那么Match对象会被填充为匹配的子序列
    // 在匹配规则中，以括号()的方式来划分组别 一共三个括号 [0]表示整体
    if(regex_match(line, Match, patten)) {  // 匹配指定字符串整体是否符合
        method_ = Match[1];
        path_ = Match[2];
        version_ = Match[3];
        state_ = HEADERS; //下一步解析HTTP请求头
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

// 解析路径，统一一下path名称，方便后面解析资源
void HttpRequest::ParsePath_() {
    if(path_ == "/") {
        path_ = "/index.html"; //如果请求路径为/ ，则设置路径为"/index.html"
    } else {
        if(DEFAULT_HTML.find(path_) != DEFAULT_HTML.end()) {
            path_ += ".html";
        } //如果请求的路径不是/，则检查路径是否在DEFAULT_HTML集合中，在的话，末尾添上".html"
    }
}


void HttpRequest::ParseHeader_(const string& line) {
    // ^字符串的开始 ([^:]*)匹配任何不包含冒号的字符序列 :匹配冒号 空格?匹配零个或一个空格 (.*)匹配任何字符除了换行符 $字符串的结束
    regex patten("^([^:]*): ?(.*)$");
    smatch Match;
    if(regex_match(line, Match, patten)) {
        header_[Match[1]] = Match[2];
    } else {    
        state_ = BODY;
    }
}


void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();  //处理POST请求
    state_ = FINISH;    // 状态转换为下一个状态
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}


// 16进制转化为10进制
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') 
        return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') 
        return ch -'a' + 10;
    return ch;
}

// 处理post请求，解析HTTP POST请求的主体内容
void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();     // POST请求体示例
        if(DEFAULT_HTML_TAG.count(path_)) { // 如果是登录/注册的path
            int tag = DEFAULT_HTML_TAG.find(path_)->second; 
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);  // 为1则是登录
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

// 从url中解析编码
void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

// 用户验证
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());

    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::getInstance());
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };        //存储sql查询或插入命令
    MYSQL_FIELD *fields = nullptr;  //数据库查询后存储字段信息
    MYSQL_RES *res = nullptr;       //数据库查询后查询信息
    
    // 不是登录
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_INFO("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_INFO("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    // SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}
