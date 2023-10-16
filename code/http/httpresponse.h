#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer& buff);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(Buffer& buff, std::string message);
    int Code() const { return code_; }

private:
    void AddStateLine_(Buffer &buff);   //添加行
    void AddHeader_(Buffer &buff);      //添加头
    void AddContent_(Buffer &buff);     //添加消息体

    void ErrorHtml_();
    std::string GetFileType_();

    int code_;
    bool isKeepAlive_;

    std::string path_;     //响应路径
    std::string srcDir_;   //相应目录
    
    char* mmFile_; 
    struct stat mmFileStat_;    //文件的元数据信息，包括文件的类型和访问权限st_mode、文件的大小 st_size

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 后缀类型集
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 编码状态集
    static const std::unordered_map<int, std::string> CODE_PATH;            // 编码路径集
};


#endif //HTTP_RESPONSE_H
