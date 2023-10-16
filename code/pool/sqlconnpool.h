#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();//返回类的唯一实例

    MYSQL *GetConn();//从连接池中获取一个连接
    void FreeConn(MYSQL * conn);//把一个连接归还给连接池
    int GetFreeConnCount();//获取当前连接池中空闲连接的数量

    void Init(const char* host, uint16_t port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);//初始化连接池
    void ClosePool();//关闭连接池

private:
    SqlConnPool() = default;
    ~SqlConnPool() { ClosePool(); }

    int MAX_CONN_;

    std::queue<MYSQL *> connQue_;//存储MySQL连接，需要一个连接时可以从队列的前端获取，完成后放入队列的后端
    std::mutex mtx_;
    sem_t semId_;
};

// 资源在对象构造初始化，资源在对象析构时释放
// 提供了一种自动管理从SqlConnPool获取的MySql连接的机制：当创建一个SqlConnRAII对象时，会自动从连接池中获取一个连接；当对象被销毁时，会自动将连接归还给连接池。这就是资源获取即初始化（RAII）原则的体现。
class SqlConnRAII {
public:
    // 指向MQL指针的指针，指向SqlConnPool的指针
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); }
    }
    
private:
    MYSQL *sql_;    //从连接池中获取的MySql连接
    SqlConnPool* connpool_; //连接池的指针
};

#endif // SQLCONNPOOL_H