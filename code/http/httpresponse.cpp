#include "httpresponse.h"

using namespace std;

// 后缀类型
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
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

// 状态码
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

//状态路径
const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};


HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}

//初始化
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    if(mmFile_) { UnmapFile(); }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

// 用于生成HTTP响应
void HttpResponse::MakeResponse(Buffer& buff) {
    /* 判断请求的资源文件 */
    // srcDir_+path_ 表示文件的完整路径，data()获取路径的C字符串表示，S_ISDIR是一个宏函数，检查文件的类型是否是目录
    // 使用stat函数获取文件的元文件信息，填充到mmFileStat_中
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;    //文件不存在或者是一个目录
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;    //没有读权限
    }
    else if(code_ == -1) { 
        code_ = 200; 
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

// 返回文件内容的指针
char* HttpResponse::File() {
    return mmFile_;
}

// 返回文件内容的长度
size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

// 根据状态码重定向路径，重新获取文件信息
void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) { //状态码存在
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else { //如果没找到的话，设置为400
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: "); //连接状态
    if(isKeepAlive_) {  //如果是长连接，设置最大请求数和超时时间
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n"); //添加文件类型
}

// 将文件内容映射到内存中以提高文件的访问速度，并向HTTP响应中添加内容的相关信息
void HttpResponse::AddContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    //将文件映射到内存提高文件的访问速度  MAP_PRIVATE 建立一个写入时拷贝的私有映射
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    // mmap函数将文件映射到内存中，0表示映射到任何可用地址，mmFileStat_.st_size表示文件长度，PROT_READ允许读取映射区的内容，MAP_PRIVATE建立一个写入时拷贝的私有映射，srcFd要映射的文件描述符，0表示映射文件的起始位置
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    mmFile_ = (char*)mmRet;  //指向映射区域的指针
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}


void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size); //释放之前映射的内存区域
        mmFile_ = nullptr;
    }
}

// 判断文件类型 根据文件的后缀来确定文件的内容类型
string HttpResponse::GetFileType_() {
    string::size_type idx = path_.find_last_of('.'); //找到最后一个点号的位置
    if(idx == string::npos) {   // 最大值 find函数在找不到指定值得情况下会返回string::npos
        return "text/plain"; //纯文本类型
    }
    string suffix = path_.substr(idx); //后缀
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }  //如果后缀存在，返回相对应的内容类型
    return "text/plain";
}

// 生成响应中的错误信息内容
void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;  // 错误页面的内容
    string status;  //存储HTTP状态的文本描述
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
