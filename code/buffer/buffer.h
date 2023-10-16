#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    // 可写的数量：buffer大小 - 写下标
    size_t WritableBytes() const;
    // 可读的数量：写下标 - 读下标    
    size_t ReadableBytes() const;
    // 可预留的空间：已读过的就没用了，等于读下标
    size_t PrependableBytes() const;

    // 读下标的位置
    const char* Peek() const;
    // 确保缓冲区中有足够的可写空间
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll();
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();  // buffer开头
    // 返回缓冲区的起始地址
    const char* BeginPtr_() const;
    // 缓冲区空间不足时进行扩展
    void MakeSpace_(size_t len);
    //! 用什么方式分配足够的内存来保存数据
    //MakeSpace()扩容，创建一个更大的vector<char>，将旧数据移动到新容器中，并释放旧的vector

    std::vector<char> buffer_;  
    std::atomic<std::size_t> readPos_;  // 读的下标
    std::atomic<std::size_t> writePos_; // 写的下标
};

#endif //BUFFER_H