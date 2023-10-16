### Buffer类的设计：

三个空间：预留空间，写空间，读空间，既可以作为写缓冲区，又可以作为读缓冲区

两个接口：

ReadFd  将数据读到缓冲区，在栈上准备了一个空间，用到iovec结构体，利用readv()来读取数据，第一块指向buffer中的writeable，第二块指向栈上空间，写区写满了再利用vector扩容机制扩容写进缓冲区。这样的目的是利用临时栈上空间，避免buffer内存浪费和read()的系统开销。

WriteFd 将缓冲区的数据写到fd



prependable：预留空间，腾空间给writable

下面的read和write针对服务端：

readale：客户端发来的HTTP请求报文写到readale，服务端读readale

writable：服务端回复的HTTP响应报文写到writable，客户端读writable