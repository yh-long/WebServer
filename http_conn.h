#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include "locker.h"

class http_conn
{
public:
    // HTTP请求方法
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT
    };

    /*解析客服端信息时，主状态机的状态    */
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, // CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER,          // CHECK_STATE_HEADER:当前正在分析请求头
        CHECK_STATE_CONTENT          // CHECK_STATE_CONTENT:当前正在分析请求体
    };
    /*服务器处理HTTP请求的可能结果，报文解析结果*/
    enum HTTP_CODE
    {
        NO_REQUEST,        // NO_REQUEST:请求不完整，需要继续读取客服端信息
        GET_REQUEST,       // GET_REQUEST:获得一个完整的客户请求
        BAD_REQUEST,       // BAD_REQUEST:客户请求语法错误
        NO_RESOURCE,       // NO_RESOURCE:服务器没有资源
        FORBIDDEN_REQUEST, // FORBIDDEN_REQUEST:客户没有足够权限访问服务器资源
        FILE_REQUEST,      // FILE_REQUEST:文件请求，获取文件成功
        INTERNAL_ERROR,    // INTERNAL_ERROR:服务器内部错误
        CLOSE_CONNECTION,  // CLOSE_CONNECTION:客户端断开连接
    };

    /*从状态机的三种可能状态*/
    enum LINE_STATUS
    {
        LINE_OK = 0, //读取到一个完整行
        LINE_BAD,    //行错误
        LINE_OPEN    //行数据不完整
    };

    http_conn();
    ~http_conn();

    static int m_epollfd;    //所有socket加入同一个epoll对象
    static int m_user_count; //所有的用户
    static const int READE_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 2048;
    static const int FILENAME_LEN =255 ;

    void process();                                 //处理客户端的请求
    void init(int sockfd, const sockaddr_in &addr); //初始化新连接客服端信息
    void close_conn();                              //关闭连接
    bool read();                                    //非阻塞读
    bool write();                                   //非阻塞写

    HTTP_CODE precess_read();                 //解析HTTP请求
    HTTP_CODE parse_request_line(char *text); //解析请求行
    HTTP_CODE parse_headers(char *text);      //解析请求头
    HTTP_CODE parse_content(char *text);      //解析请求体
    bool process_write(HTTP_CODE read_ret);
    bool add_response(const char *format, ...);
    bool add_status_line(int status, const char *str); //添加响应行
    bool add_headers(int content_len);                 //响应头
    bool add_content_length(int content_len);
    bool add_linger();
    bool add_blank_line();
    bool add_content(const char *content); //响应体
    bool add_content_type();
    void unmap(); //释放地址映射

    LINE_STATUS parse_line();
    HTTP_CODE do_request();

private:
    int m_sockfd;                        // HTTP通信连接
    struct sockaddr_in m_address;        //通信socket地址
    char m_read_buf[READE_BUFFER_SIZE];  //读缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE]; //写缓冲区
    int m_read_index = 0;                //读取标志
    int m_write_index = 0;               //写入标志

    int m_checked_index; //当前正在分析的字符在读缓冲区的位置
    int m_start_line;
    char *m_url;           //请求地址
    char *m_version;       // HTTP版本
    METHOD m_method;       //请求方法
    char *m_host;          //主机
    char *m_agent;         //
    bool m_linger = false; //判断HTTP请求是否保持连接
    int m_content_length = 0;
    char *m_accept;
    char *m_accept_Encoding;
    char *m_accept_Language;
    int m_upgrade_insecure_requests;
    char m_real_file[200];
    char *doc_root;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    char *m_file_address;
    int m_iv_count;

    CHECK_STATE m_check_state;

    void init(); //初始化解析
    char *get_line() { return m_read_buf + m_start_line; };
};
#endif