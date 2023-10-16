#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"
/*
进行读写数据并调用httprequest 来解析数据以及httpresponse来生成响应
*/
class HttpConn {
public:
    HttpConn();
    ~HttpConn();
    
    void init(int fd, const sockaddr_in& addr);
    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);
    void Close();
    int GetFd() const;  //获取套接字文件描述符
    int GetPort() const;    //获取连接端口号
    const char* GetIP() const;  //获取连接的IP地址
    sockaddr_in GetAddr() const;    //获取连接的地址信息
    bool process(); //处理HTTP请求，包括解析请求和生成相应

    // 写的总长度
    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;  // 原子，支持锁
    
private:
   
    int fd_;
    struct  sockaddr_in addr_;

    bool isClose_;
    
    int iovCnt_;
    struct iovec iov_[2]; // 多个缓冲区的I/O操作，允许将多个缓冲区的数据一次性写入或读取到文件描述符
    // 包括iov_base和iov_len，一个指向缓冲区的指针，一个表示缓冲区的长度
    // 用于将响应报文的数据从缓冲区writeBuff_以及文件数据写入到客户端连接的文件描述符fd_中
    //iov_[0] 包含了响应报文的头部和部分正文内容。iov_[0].iov_base 是指向缓冲区 writeBuff_ 中待发送数据的指针，iov_[0].iov_len 是待发送数据的长度。
    //iov_[1] 包含了文件数据，当需要发送文件时才会用到。iov_[1].iov_base 是指向文件数据的指针，iov_[1].iov_len 是文件数据的长度。

    Buffer readBuff_; // 读缓冲区
    Buffer writeBuff_; // 写缓冲区

    HttpRequest request_;
    HttpResponse response_;
};


#endif