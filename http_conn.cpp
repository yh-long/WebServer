#include "http_conn.h"

http_conn::http_conn()
{
}

http_conn::~http_conn()
{
    exit(0);
}

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;
const char *error_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "";
const char *error_403_title = "Forbidden";
const char *error_403_form = "";
const char *error_404_title = "Not Found";
const char *error_404_form = "";
const char *error_500_title = "Internal Error";
const char *error_500_form = "";
const char *doc_root = "/home/lenovo/WebServer/resource";

//设置文件描述符非阻塞
void setnonblocking(int fd)
{
    int old_flg = fcntl(fd, F_GETFL);   //获取文件描述符当前属性
    int new_flg = old_flg | O_NONBLOCK; ////文件描述符新属性
    fcntl(fd, F_SETFL, new_flg);        //设置文件描述符属性
}

//添加文件描述符到epoll
int addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    // event.events = EPOLLIN | EPOLLRDHUP;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    if (one_shot)
    {
        event.events | EPOLLONESHOT;
    }
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) != 0)
    {
        return -1;
    }

    //设置文件描述符非阻塞
    setnonblocking(fd);
    return 0;
}

//从epoll删除文件描述符
int removefd(int epollfd, int fd)
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0) != 0)
    {
        return -1;
    }
    close(fd);
    return 0;
}

//从epoll修改文件描述符
int modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    printf("%d", event.data.fd);
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) != 0)
    {
        return -1;
    }
    return 0;
}

//主状态机，解析请求
http_conn::HTTP_CODE http_conn::precess_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char *text = 0;
    text = get_line();

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        //获取一行数据
        text = get_line();

        m_start_line = m_checked_index;
        // printf("m_start_line:%d\n", m_start_line);
        // printf("got 1 http line:\n%s\n", text);

        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            ret = parse_request_line(text);
            // printf("parse_request_line: %d\n", ret);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            ret = parse_headers(text);
            // printf("parse_headers: %d\n", ret);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST)
            {
                printf("do_request\n");
                return do_request();
            }
            break;
        case CHECK_STATE_CONTENT:
            ret = parse_content(text);
            printf("parse_content: %d\n", ret);
            if (ret == GET_REQUEST)
            {
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

//解析HTTP请求行，获取请求方法、目标URL、HTTP版本
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    // printf("第一个匹配的字符是： %s\n", m_url);

    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else
    {
        printf("error");
        return BAD_REQUEST;
    }

    m_version = strpbrk(m_url, " \t");
    // printf("第一个匹配的字符是： %s\n", m_version);
    if (!m_version)
    {
        printf("m_version !\n");
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        printf("m_version error\n");
        return BAD_REQUEST;
    }
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;                 // 192.168.1.1:10000/index.html
        m_url = strchr(m_url, '/'); // /index.html
    }
    if (!m_url || m_url[0] != '/')
    {
        printf("url error\n");
        return BAD_REQUEST;
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    //遇到空行，表示头部解析完成
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
            // printf("keep-alive\n");
        }
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else if (strncasecmp(text, "User-Agent:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        m_agent = text;
    }
    else if (strncasecmp(text, "Accept:", 7) == 0)
    {
        text += 7;
        text += strspn(text, " \t");
        m_accept = text;
    }
    else if (strncasecmp(text, "Accept-Language:", 16) == 0)
    {
        text += 16;
        text += strspn(text, " \t");
        m_accept_Language = text;
    }
    else if (strncasecmp(text, "Accept-Encoding:", 16) == 0)
    {
        text += 16;
        text += strspn(text, " \t");
        m_accept_Encoding = text;
    }
    else if (strncasecmp(text, "Upgrade-Insecure-Requests:", 26) == 0)
    {
        m_upgrade_insecure_requests = 1;
    }
    else
    {
        printf("oop! unknow header %s\n", text);
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_index >= (m_content_length + m_checked_index))
    {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
//解析一行，判断/r/n
http_conn::LINE_STATUS http_conn::parse_line()
{

    char temp;
    for (; m_checked_index < m_read_index; ++m_checked_index)
    {
        temp = m_read_buf[m_checked_index];
        // printf("m_read_index:%d\n", m_read_index);
        // printf("m_checked_index:%d,temp:%s\n", m_checked_index,&temp);
        if (temp == '\r')
        {
            if ((m_checked_index + 1) == m_read_index)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_index + 1] == '\n')
            {
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            printf("error:1\n");
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if ((m_checked_index > 1) && (m_read_buf[m_checked_index - 1] == '\r'))
            {
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            printf("error:2\n");
            return LINE_BAD;
        }
    }
    return LINE_OK;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    // home/lenovo/webser/resource
    const char *doc_root = "/home/lenovo/WebServer/resource";
    printf("%s\n",doc_root);
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    // printf("len: %d\n", len);
    // printf("m_real_file:%s\n", m_real_file);
    // printf("m_url:%s\n", m_url);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    printf("m_real_file:%s\n", m_real_file);
    //获取文件状态信息，-1失败，0成功
    if (stat(m_real_file, &m_file_stat) < 0)
    {
        perror("stat");
        return NO_REQUEST;
    }

    //判断访问权限
    if (!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }
    //判断是否是目录
    if (S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }
    //以只读方式打开文件
    int fd = open(m_real_file, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
    }
    //创建内存映射
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);  

    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_address)
    {

        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::process_write(HTTP_CODE read_ret)
{
    switch (read_ret)
    {
    case INTERNAL_ERROR:
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
        {
            return false;
        }
        break;
    case BAD_REQUEST:
        add_status_line(400, error_400_title);
        add_headers(strlen(error_400_form));
        if (!add_content(error_400_form))
        {
            return false;
        }
        break;
    case NO_RESOURCE:
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
        {
            return false;
        }
        break;
    case FORBIDDEN_REQUEST:
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
        {
            return false;
        }
        break;
    case FILE_REQUEST:
        add_status_line(200, error_200_title);
        add_headers(m_file_stat.st_size);
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_index;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        m_iv_count = 2;
        return true;
    default:
        return false;
    }
    return false;
}

void http_conn::process()
{

    //解析HTTP请求
    // precess_read()

    HTTP_CODE read_ret = precess_read();
    printf("read_ret: %d\n", read_ret);
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    // precess_write()
    bool write_ret = process_write(read_ret);
    printf("write_ret: %d\n", write_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
    // printf("parse request,create response\n");
    //生成响应
}

void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;

    //设置端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //添加到epoll对象
    addfd(m_epollfd, m_sockfd, true);
    m_user_count++;
    init();
}

void http_conn::init()
{
    m_check_state = CHECK_STATE_REQUESTLINE; //初始化解析首行
    m_checked_index = 0;
    m_start_line = 0;
    m_read_index = 0;
    m_method = GET; //请求方法
    m_url = 0;      //请求地址
    m_version = 0;  // HTTP版本
    m_host = 0;
    bzero(m_read_buf, READE_BUFFER_SIZE);
}

void http_conn::close_conn()
{
    if (m_sockfd != -1)
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--; //客户数量减一
    }
}

bool http_conn::read()
{
    if (m_read_index >= READE_BUFFER_SIZE)
    {
        return false;
    }
    //读取字节
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READE_BUFFER_SIZE - m_read_index, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                //没有数据
                break;
            }
            return false;
        }
        else if (bytes_read == 0)
        {
            //断开连接
            return false;
        }
        m_read_index += bytes_read;
    }
    // printf("m_read_index:%d\n", m_read_index);
    // printf("recv data:\n%s\n", m_read_buf);
    return true;
}

bool http_conn::write()
{
    int temp;
    int bytes_have_send = 0;           //已经发送的字节
    int bytes_to_send = m_write_index; //将要发送的字节
    printf("bytes_to_send:%d\n",bytes_to_send);
    if (bytes_to_send == 0)
    {
        //将要发送的字节为0，结束响应
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while (1)
    {
        //分散写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        printf("write buf:%s\n",m_write_buf);
        printf("real file:%s\n",m_file_address);
        if (temp <= -1)
        {
            perror("writecv");
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        
        if (bytes_to_send <= bytes_have_send)
        {
            unmap();
            if (m_linger)
            {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else
            {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
        
    }
    return false;
}

//向缓冲区写入数据
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_index >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_index, WRITE_BUFFER_SIZE - 1 - m_write_index, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_index))
    {
        return false;
    }
    m_write_index += len;

    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char *str)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, str);
}

bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
    return true;
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-live" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content)
{
    return true;
}

bool http_conn::add_content_type()
{
    return add_response("Content-Type: %s\r\n", "text/html");
}