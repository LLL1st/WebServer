

#### 1 单例模式

在整个系统生命周期内，保证一个类只能产生一个实例

特点：

构造函数和析构函数为私有类型，禁止外部构造和析构

拷贝构造函数和赋值构造函数为私有类型，禁止外部拷贝和赋值，保证唯一性

类中有一个获取实例的静态方法，可以全局访问



- 懒汉模式

  不用时不去初始化，第一次被使用时初始化

  **加锁的懒汉模式**

  ```c++
  class Single {
  public:
      //获取单例对象
      static Single* getInstance();
      //释放单例对象，进程退出时调用
      static void deleteInstance();
  private:
      //私有化构造函数和析构函数
      Single() {}
      ~Single() {}
      
      //私有化拷贝构造函数和赋值构造函数
      Single(const Single &signal) = delete;
      const Single &operator=(const Single &signal) = delete;
      
      //唯一单例对象指针
      static Single *m_single;
      static std::mutex m_mutex;
  };
  // 初始化静态成员变量
  Single* Single::m_single = nullptr;
  std::mutex Single::m_mutex;
  
  Single* Single::getInstance() {
      if(m_single == nullptr) {
          std::unique_lock<std::mutex> lock(m_mutex);
          if(m_single == nullptr) {
              m_single = new Single();
          }
      }
      return m_single;
  }
  void Single::deleteInstance() {
      std::unique_lock<std::mutex> lock(m_mutex);
      if(m_single) {
          delete m_single;
          m_single = nullptr;
      }
  }
  ```

  > **为什么要双检测**
  >
  > 1.第一次 `if (NULL == p)`：在没有锁定互斥锁的情况下检查 `p` 是否为空。如果为空，才进入下一层的临界区（即 `pthread_mutex_lock` 和 `pthread_mutex_unlock` 之间的代码块）。
  >
  > 2.第二次 `if (NULL == p)`：在临界区内，再次检查 `p` 是否为空。这是因为在多线程环境下，两个线程可能同时通过了第一次检查，然后一个线程获得了锁，创建了实例，而另一个线程在获取锁之前等待。如果不进行第二次检查，那么等待的线程在获取锁后也会创建一个实例，破坏了单例模式的要求。

  **静态局部变量的懒汉模式**

  ```c++
  class single{
  public:
  	static single* getinstance();
  private:
      single(){}
      ~single(){}
  };
  
  single* single::getinstance() {
      static single obj;
      return &obj;
  }
  ```

  

- 饿汉模式

  饿汉模式不需要用锁，就可以实现线程安全。原因在于，在程序运行时就定义了对象，并对其初始化。之后，不管哪个线程调用成员函数getinstance()，都只不过是返回一个对象的指针而已。所以是线程安全的，不需要在获取实例的成员函数中加锁。

  ```c++
  class single{
  public:
      static single* getinstance();
  private:
      static single* p;
      single(){}
      ~single(){}
  };
  
  single* single::p = new single();
  single* single::getinstance() {
      return p;
  }
  ```

  饿汉模式虽好，但其存在隐藏的问题，在于非静态对象（函数外的static对象）在不同编译单元中的初始化顺序是未定义的。如果在初始化完成之前调用 getInstance() 方法会返回一个未定义的实例。

  

#### 2 同步和异步日志
- 同步日志：日志写入函数与工作线程串行执行，由于涉及到I/O操作，当单条日志比较大的时候，同步模式会阻塞整个处理流程，服务器所能处理的并发能力将有所下降，尤其是在峰值的时候，写日志可能成为系统的瓶颈。

- 异步日志：将所写的日志内容先存入阻塞队列中，写线程从阻塞队列中取出内容，写入日志。

考虑到文件IO操作是非常慢的，所以采用异步日志就是先将内容存放在内存里，然后日志线程有空的时候再写到文件里。

日志队列是什么呢？他的需求就是时不时会有一段日志放到队列里面，时不时又会被取出来一段，这不就是经典的生产者消费者模型吗？所以还需要加锁、条件变量等来帮助这个队列实现。



#### 3 日志的运行流程
1、使用单例模式（局部静态变量方法）获取实例Log::getInstance()。

2、通过实例调用Log::getInstance()->init()函数完成初始化，若设置阻塞队列大小大于0则选择异步日志，等于0则选择同步日志，更新isAysnc变量。

3、通过实例调用write_log()函数写日志，首先根据当前时刻创建日志（前缀为时间，后缀为".log"，并更新日期today和当前行数lineCount。

4、在write_log()函数内部，通过isAsync变量判断写日志的方法：如果是异步，工作线程将要写的内容放进阻塞队列中，由写线程在阻塞队列中取出数据，然后写入日志；如果是同步，直接写入日志文件中。



#### 4 日志的分级与分文件
**分级情况：**

- Debug，调试代码时的输出，在系统实际运行时，一般不使用。
Warn，这种警告与调试时终端的
- warning类似，同样是调试代码时使用。
- Info，报告系统当前的状态，当前执行的流程或接收的信息等。
- Erro，输出系统的错误信息


**分文件情况：**

按天分，日志写入前会判断当前today是否为创建日志的时间，若为创建日志时间，则写入日志，否则按当前时间创建新的log文件，更新创建时间和行数。

按行分，日志写入前会判断行数是否超过最大行限制，若超过，则在当前日志的末尾加lineCount / MAX_LOG_LINES为后缀创建新的log文件。