#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

    //新增：获取线程池状态
    void get_stats(int& queue_size){
        m_queuelocker.lock();
        queue_size = m_workqueue.size();
        m_queuelocker.unlock();
    }

    //新增：打印状态日志
    void log_stats(){
        int queue_size = 0;
        get_stats(queue_size);

        //获取时间戳
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S", t);
        printf("[%s] [THREADPOOL] queue=%d/%d threads=%d\n",
        buf, queue_size, m_max_requests, m_thread_number);
    }

    //新增：获取配置信息（用于日志）
    int get_thread_num() {return m_thread_number;}
    int get_max_requests() {return m_max_requests;}

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);  //worker 是静态函数，通过 this 传入对象指针调用成员函数
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列  任务队列（生产者-消费者缓冲区）
    locker m_queuelocker;       //保护请求队列的互斥锁 （保护队列）
    sem m_queuestat;            //是否有任务需要处理  信号量（队列是否有任务）
    connection_pool *m_connPool;  //数据库  数据库连接池（给任务用）
    int m_actor_model;          //模型切换 0=Proactor, 1=Reactor
};

template <typename T>
threadpool<T>::threadpool( int actor_model, connection_pool *connPool, int thread_number, int max_requests) : m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    
    //循环创建线程
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)  //worker 是静态函数，通过 this 传入对象指针调用成员函数
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))  // 分离线程，自动回收
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}
template <typename T>
bool threadpool<T>::append(T *request, int state)  //主线程添加任务（生产者）
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;  // 队列满，添加失败
    }
    request->m_state = state;  // 0=读, 1=写
    m_workqueue.push_back(request);
    m_queuelocker.unlock(); 
    m_queuestat.post();  // V操作：唤醒工作线程 S++; 唤醒等待的进程;
    return true;
}
template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();  // 调用成员函数
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        m_queuestat.wait();  // P操作：等任务（阻塞） 等待/申请资源  while (S <= 0) 阻塞; S--;
        m_queuelocker.lock();  // 加锁
        if (m_workqueue.empty())  // 双重检查
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();  // 取任务
        m_workqueue.pop_front();
        m_queuelocker.unlock();  // 解锁
        if (!request)
            continue;
        
        // 执行任务（Reactor/Proactor区分）
        if (1 == m_actor_model)  // Reactor
        {
            if (0 == request->m_state)  // 读事件
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else  // 写事件
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else  // Proactor
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();  // 直接处理
        }
    }
}
#endif
