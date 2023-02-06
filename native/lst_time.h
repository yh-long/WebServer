#ifndef LST_TIME_H
#define LST_TIME_H

#include <stdio.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 64;
class until_timer; //前向声明

//用户信息记录
struct client_data
{
    sockaddr_in address;   //用户地址信息
    int scokfd;            //通信文件描述符
    char buf[BUFFER_SIZE]; //内容缓存区
    until_timer *timer;    //定时器
};

//定时器
class until_timer
{
public:
    until_timer() : prev(NULL), next(NULL);
    ~until_timer();

private:
    time_t expire;                  //等待时间
    void (*cb_func)(client_data *); //回调函数
    client_data *user_data;         //用户信息
    until_timer *prev;              //上个节点
    until_timer *next;              //下个节点
};

//定时器链表
class sort_time_list
{
public:
    sort_time_list() : head(NULL), tail(NULL);
    ~sort_time_list()
    {
        until_timer *temp = head;
        while (temp)
        {
            head = temp->next;
            delete temp;
            temp = head;
        }
    };
    // 将目标定时器timer添加到链表中
    void add_timer(until_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if (!head)
        {
            head = tail = timer;
            return;
        }
        /**/
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }
    /* 当某个定时任务发生变化时，调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的
    超时时间延长的情况，即该定时器需要往链表的尾部移动。*/
    void adjust_timer(until_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        until_timer *temp = timer->next;
        if (!temp || (timer->expire < temp->expire))
        {
            return;
        }
        if (timer == head)
        {
            head->next = head;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, timer);
        }
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->nexr);
        }
    }
    /*将目标定时器从链表删除*/
    void del_timer(until_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if ((timer == head) && (timer == tail))
        {
            delete timer;
            timer->next = NULL;
            timer->prev = NILL;
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete tail;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
    }
    /* SIGALARM 信号每次被触发就在其信号处理函数中执行一次 tick() 函数，以处理链表上到期任务。*/
    void tick()
    {
        if (!head)
        {
            return;
        }
        printf("tick\n");
        time_t cur = time(NULL);
        until_timer *temp = head;
        while (temp)
        {
            if (cur < temp->expire)
            {
                break;
            }

            temp->cb_func(temp->user_data);
            head = temp->next;
            if (head)
            {
                head->prev = NULL;
            }
            delete temp;
            temp = head;
        }
    }

private:
    /*重载的辅助函数，它被公有的 add_timer 函数和 adjust_timer 函数调用
    该函数表示将目标定时器 timer 添加到节点 lst_head 之后的部分链表中 */
    void add_timer(until_timer *timer, until_timer *lst_head)
    {
        until_timer *prev = lst_head;
        until_timer *temp = prev->next;
        /* 遍历 list_head 节点之后的部分链表，直到找到一个超时时间大于目标定时器的超时时间节点
        并将目标定时器插入该节点之前 */
        while (temp)
        {
            if (timer->expire < temp->expire)
            {
                prev->next = timer;
                timer->next = temp;
                temp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = temp;
            temp = temp->next;
        }
        /* 如果遍历完 lst_head 节点之后的部分链表，仍未找到超时时间大于目标定时器的超时时间的节点，
        则将目标定时器插入链表尾部，并把它设置为链表新的尾节点。*/
        if (!temp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    until_timer *head; //头节点
    until_timer *tail; //尾节点
}

#endif