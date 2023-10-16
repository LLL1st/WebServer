#include "buffer.h"

// 读写下标初始化，vector<char>初始化
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0)  {}  

// 可写的数量：buffer大小 - 写下标
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 可读的数量：写下标 - 读下标
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

// 可预留空间：已经读过的就没用了，等于读下标
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

// 返回一个指向缓冲区中可读数据的指针，即读下标 readPos_ 的位置。
const char* Buffer::Peek() const {
    
    return &buffer_[readPos_];
}

// 确保缓冲区中有足够的写空间存储len字节的数据。若不足则调用MakeSpace_函数扩展缓冲区大小
void Buffer::EnsureWriteable(size_t len) {
    if(len > WritableBytes()) {
        MakeSpace_(len);
    }
    assert(len <= WritableBytes());
}

// 移动写下标，表示已经写入了len字节的数据。在Append中使用
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
}

// 移动读下标，表示已经读取了len字节的数据
void Buffer::Retrieve(size_t len) {
    readPos_ += len;
}

// 从缓冲区中一直读数据，读到end指针的位置
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek()); // end指针 - 读指针 长度
}

// 取出所有数据，buffer归零，读写下标归零,在别的函数中会用到
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size()); // 覆盖原本数据
    readPos_ = writePos_ = 0;
}

// 取出剩余可读的str，再将缓冲区清空
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 写指针的位置
const char* Buffer::BeginWriteConst() const {
    return &buffer_[writePos_];
}

char* Buffer::BeginWrite() {
    return &buffer_[writePos_];
}

// 添加str到缓冲区
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);   // 确保可写的长度
    std::copy(str, str + len, BeginWrite());    // 将str放到写下标开始的地方
    HasWritten(len);    // 移动写下标
}

void Buffer::Append(const std::string& str) {
    Append(str.c_str(), str.size());
}

// 将二进制数据data中的len字节追加到缓冲区中
void Append(const void* data, size_t len) {
    Append(static_cast<const char*>(data), len);
}

// 将buffer中的读下标的地方放到该buffer中的写下标位置
void Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

// 将fd的内容读到缓冲区，即writable的位置，并返回读取的字节数
ssize_t Buffer::ReadFd(int fd, int* Errno) {
    char buff[65535];   // 栈区
    struct iovec iov[2];
    size_t writeable = WritableBytes(); // 先记录能写多少
    // 分散读， 保证数据全部读完
    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *Errno = errno;
    } else if(static_cast<size_t>(len) <= writeable) {   // 若len小于writable，说明写区可以容纳len
        writePos_ += len;   // 直接移动写下标
    } else {    
        writePos_ = buffer_.size(); // 写区写满了,下标移到最后
        Append(buff, static_cast<size_t>(len - writeable)); // 剩余的长度
    }
    return len;
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::WriteFd(int fd, int* Errno) {
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if(len < 0) {
        *Errno = errno;
        return len;
    } 
    Retrieve(len);
    return len;
}

// 返回指向缓冲区起始地址的指针
char* Buffer::BeginPtr_() {
    return &buffer_[0];
}

const char* Buffer::BeginPtr_() const{
    return &buffer_[0];
}

// 扩展空间
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    } else {
        size_t readable = ReadableBytes();
        // 将从读下标readPos_到写下标writePos_之间的数据复制到缓冲区的起始位置
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readable;
        assert(readable == ReadableBytes());
    }
}

