#include "log.h"

// 构造函数
Log::Log() {
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;
    lineCount_ = 0;
    toDay_ = 0;
    isAsync_ = false; //是否使用异步日志
}

// 析构函数
Log::~Log() {
    while(!deque_->empty()) {
        deque_->flush();
    }
    deque_->Close();
    // 等待写线程完成其任务
    writeThread_->join();
    if(fp_) {
        lock_guard<mutex> locker(mtx_); //lock_guard是一个模板类，接受一个互斥量作为参数，在构造时自动获取这个互斥量的所有权，生命周期结束时，在其析构函数中自动释放互斥量的所有权
        flush(); //清空缓冲区中的数据
        fclose(fp_);
    }
}

// 冲洗缓冲区，如果使用异步日志，会将日志刷新到deque_中，否则直接刷新到文件
void Log::flush() {
    // 判断是否异步，只有异步日志才会用到deque
    if(isAsync_) {
        deque_->flush();
    }
    fflush(fp_); // 清空输入缓冲区
}

// 懒汉模式
Log* Log::getInstance() {
    static Log log;
    return &log;
}

// 异步日志的写线程
void Log::FlushLogThread() {
    Log::getInstance()->AsyncWrite_();
}

// 写线程真正的执行函数
void Log::AsyncWrite_() {
    string str = "";
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

// 初始化日志实例
void Log::init(int level, const char* path, const char* suffix, int maxQueCapacity) {
    isOpen_ = true;
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    // 阻塞队列大小大于0时选择异步日志，等于0则选择同步日志
    if(maxQueCapacity) {
        isAsync_ = true;
        if(!deque_) {
            unique_ptr<BlockQueue<std::string>> newQue(new BlockQueue<std::string>); 
            // unique_ptr 不支持普通的拷贝和赋值操作，所以采用move
            // 将动态申请的内存权给deque，newQue被释放
            deque_ = move(newQue);

            unique_ptr<thread> newThread(new thread(FlushLogThread));
            writeThread_ = move(newThread);
        }
    }
    else {
        isAsync_ = false;
    }

    lineCount_ = 0;

    // 获取当前时间的时间戳
    // time(nullptr)返回当前事件距离1970年1月1日00：00的秒数
    time_t timer = time(nullptr);
    // localtime(&timer)将时间戳转换为本地时间，返回一个指向tm结构体的指针
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    // 使用snprintf函数创建日志文件名，最终格式“路径/年份_月份_日期后缀”
    // snprintf函数用于格式化字符串并将结果存储到一个字符数组中，同时确保不会超出指定的缓冲区大小。
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll(); //清空buff_，即一个用于存储日志信息的缓冲区
        if(fp_) {
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr) {
            mkdir(path_, 0777); // 创建日志文件的目录
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr); //使用assert断言确保文件指针fp_不为空，如果为空则程序终止
    }
}


void Log::write(int level, const char* format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec; // 提取秒数
    struct tm *sysTime = localtime(&tSec); //使用localtime函数将秒数转换为本地时间
    struct tm t = *sysTime;
    va_list vaList; //va_list是用于处理可变数量参数的数据类型

    // 判断是否需要切换到新的日志文件，成立的情况：当前日期与toDay_不一致，写入的日志行数lineCount_大于0并且达到了最大行限制
    if(toDay_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0))) {
        // unique_lock是一种用于管理互斥量mutex的RALL（资源获取即初始化）类型
        // locker将负责管理mtx_的锁定和解锁操作
        unique_lock<mutex> locker(mtx_); //确保在判断和执行之间没有其他线程干扰
        // 立即解锁与unique_lock相关联的互斥量mtx_
        locker.unlock(); //允许其他线程进入互斥区域

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        // 使用snprint函数将年、月、日信息格式化为字符串，并存储在tail中
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        // 时间不匹配，替换为最新的日志文件名，格式为"path_/年_月_日suffix_"
        if(toDay_ != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else { //时间匹配，但是行数达到了最大行数
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }
        locker.lock();  //生成新的日志文件名后，重新锁定互斥量
        flush(); //清空缓冲区中的数据
        fclose(fp_); // 关闭之前的文件
        fp_ = fopen(newFile, "a");  // 打开一个新文件，"a"表示以追加写入方式打开文件，如果文件不存在就创建它
        assert(fp_ != nullptr);
    }

    // 在buffer内生成一条对应的日志信息
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level); //添加日志等级

        va_start(vaList, format); //初始化vaList
        // vsnprintf函数用于格式化字符串并将结果存储到一个字符数组中，同时确保不会超出指定的缓冲区大小。与snprintf函数的不同之处在于vsnprintf使用一个va_list参数来处理可变数量的参数，而不是...形式的参数列表
        // 将vaList中的参数按照format格式化
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        // 异步方式，deque_不为空，deque_不满
        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        }
        // 同步方式
        else {
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

// 添加日志等级
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info]: ", 9);
        break;
    case 2:
        buff_.Append("[warn]: ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info]: ", 9);
        break;
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}