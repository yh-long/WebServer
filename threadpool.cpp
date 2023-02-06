// #include <threadpool.h>


// template <typename T>
// threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests),
//                                                                 m_stop(false), m_threads(NULL)
// {
//     m_stop=false;
    
//     //判断要创建的线程、任务的数量
//     if ((thread_number <= 0) || (max_requests <= 0))
//     {
//         throw std::exception();
//     }

//     //创建线程数组
//     m_threads = new pthread_t[m_thread_number];
//     if (!m_threads)
//     {
//         throw std::exception();
//     }

//     //创建线程，并设置为线程分离
//     for (int i = 0; i < thread_number; i++)
//     {
//         printf("create the %dth thread\n", i);
//         //线程创建
//         if (pthread_create(m_threads + i, NULL, worker, this) != 0)
//         { /*线程、线程属性、回调函数、传递参数*/
//             delete[] m_threads;
//             throw std::exception();
//         }
        
//         //线程分离
//         if (pthread_detach(m_threads[i]) != 0)
//         {
//             delete[] m_threads;
//             throw std::exception();
//         }
//     }
// }

// template <typename T>
// threadpool<T>::~threadpool()
// {
//     delete[] m_threads;
//     m_stop = true;
// }

// //线程加入队列
// template <typename T>
// bool threadpool<T>::append(T *request)
// {
//     m_queuelocker.lock();
//     //判断请求队列是否可添加
//     if (m_workqueue.size() > m_max_requests)
//     {
//         m_queuelocker.unlock();
//         return false;
//     }
//     //将任务加入队列
//     m_workqueue.push_back(request);
//     m_queuelocker.unlock();
//     //信号量加一
//     m_queuestat.post();
    
//     return true;
// }

// template <typename T>
// void *threadpool<T>::worker(void *arg)
// {   
//     threadpool *pool = (threadpool *)arg;
//     pool->run(); 
//     return pool;
// }

// template <typename T>
// void threadpool<T>::run()
// {
//     while (!m_stop)
//     {
//         //printf("thread:%ld\n",pthread_self());
//         //线程执行
//         m_queuestat.wait();
//         //数据同步
//         m_queuelocker.lock();
//         //是否存在任务
//         if(m_workqueue.empty()){
//             m_queuelocker.unlock();
//             continue;
//         }
//         T *request = m_workqueue.front();
//         m_workqueue.pop_front();
//         m_queuelocker.unlock();

//         if (!request)
//         {
//             continue;
//         }
//         request->process();  
//     }
// }