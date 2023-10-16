#include "sqlconnpool.h"

SqlConnPool* SqlConnPool::getInstance() {
    static SqlConnPool pool;
    return &pool;
}

//初始化，六个参数：MySQL服务器的主机名、端口号、用户名、密码、数据库名、连接池的大小
void SqlConnPool::Init(const char* host, uint16_t port, const char* user, const char* pwd, const char* dbName, int connSize = 10) {
    assert(connSize > 0);   //断言连接池的大小必须大于0
    for (int i = 0; i < connSize; i++) {
        MYSQL *conn = nullptr;
        conn = mysql_init(conn); // 创建一个新的MySQL连接，并进行了初始化
        if(!conn) { //如果初始化失败，记录错误日志，并断言连接非空
            LOG_ERROR("MySql init error!");
            assert(conn);
        }
        conn = mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0); //尝试用指定的参数连接到MySQL服务器
        if(!conn) { //如果连接失败，记录错误日志
            LOG_ERROR("Mysql Connect error!");
        }
        connQue_.emplace(conn); //将创建的连接添加到连接队列中
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_); //初始化一个信号量，用于控制可用的连接数
}

// 获得一个MYSQL连接
MYSQL* SqlConnPool::GetConn() {
    MYSQL *conn = nullptr;
    if(connQue_.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);
    std::lock_guard<std::mutex> locker(mtx_);
    conn = connQue_.front();
    connQue_.pop();
    return conn;
}

// 释放一个MYSQL连接，相当于存入内存池
void SqlConnPool::FreeConn(MYSQL *conn) {
    assert(conn);
    std::lock_guard<std::mutex> locker(mtx_);
    connQue_.push(conn);
    sem_post(&semId_);
}

// 关闭线程池
void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto conn = connQue_.front();
        connQue_.pop();
        mysql_close(conn);
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);
    return connQue_.size();
}
