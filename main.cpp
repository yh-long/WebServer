#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <libgen.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535           //最大的文件描述符
#define MAX_EVENT_NUMBER 10000 //最大监听

//添加信号捕捉
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

//添加文件描述符到epoll
extern int addfd(int epollfd, int fd, bool one_shot);
//从epoll删除文件描述符
extern int removefd(int epollfd, int fd);
//从epoll修改文件描述符
extern int modfd(int epollfd, int fd, int ev);

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        printf("按照如下格式运行：%s port_number\n", basename(argv[0]));
        exit(-1);
    }

    //获取端口号
    int port = atoi(argv[1]);
    printf("argc:%d,argv[1]:%d\n", argc, port);
    //对SIGPIE信号进行处理
    addsig(SIGPIPE, SIG_IGN);
    //创建线程池、初始化线程池
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        exit(-1);
    }

    //创建一个数组用于保存所有的客服端信息
    http_conn *users = new http_conn[MAX_FD];

    //创建socket套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    if (listenfd == -1)
    {
        perror("socket");
        exit(-1);
    }
    //设置端口复用
    int reuse = 1;
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret == -1)
    {
        perror("setsockopt");
        exit(-1);
    }

    //绑定IP地址、端口
    struct sockaddr_in ser_addr;
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    ser_addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(listenfd, (struct sockaddr *)&ser_addr, sizeof(ser_addr));
    if (ret == -1)
    {
        perror("bind");
        exit(-1);
    }
    //监听socket
    ret = listen(listenfd, 5);
    if (ret == -1)
    {
        perror("listen");
        exit(-1);
    }

    //创建epoll对象、事件数组、添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    // printf("epollfd:%d\n", epollfd);
    // printf("listenfd:%d\n", listenfd);
    //将监听文件描述符添加到epoll对象
    ret = addfd(epollfd, listenfd, false);
    if (ret == -1)
    {
        exit(-1);
    }
    http_conn::m_epollfd = epollfd;
    // printf("http_conn::epollfd:%d\n", http_conn::m_epollfd);

    while (1)
    {
        // printf("调试监听事件\n");
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        printf("num:%d\n", num);
        if ((num < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        //遍历文件描述符数组
        for (int i = 0; i < num; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                //客服端连接服务器生成文件描述符
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);

                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlen);
                if (connfd == -1)
                {
                    perror("accept");
                    exit(-1);
                }

                if (http_conn::m_user_count >= MAX_FD)
                {
                    //目前连接数
                    //给客服端一个信息，服务器正忙
                    close(connfd);
                    continue;
                }

                //客服端数据初始化，保存到数组
                users[connfd].init(connfd, client_address);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //客服端异常断开或者错误等事件
                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                if (users[sockfd].read())
                {
                    //读取数据
                    pool->append(users + sockfd);
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {

                if (!users[sockfd].write())
                { //一次写入全部数据
                    printf("write...\n");
                    users[sockfd].close_conn();
                }
            }
        }
        sleep(1);
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}