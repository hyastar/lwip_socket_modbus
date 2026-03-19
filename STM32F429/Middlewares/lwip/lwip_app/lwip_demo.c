/**
 ****************************************************************************************************
 * @file        lwip_demo
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-08-01
 * @brief       lwIP SOCKET CPServer多连接实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 阿波罗 F429开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */
 
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "lwip_demo.h"
#include "lwip/netdb.h"
#include <lwip/sockets.h>
#include <string.h>
#include <stdlib.h>


static const char *send_data = "link Success\n";

/* 客户端的信息 */
struct client_info
{
    int socket_num;                 /* socket号的数量 */
    struct sockaddr_in ip_addr;     /* socket客户端的IP地址 */
    int sockaddr_len;               /* socketaddr的长度 */
};

/* 客户端的任务信息 */
struct client_task_info
{
    UBaseType_t client_task_pro;    /* 客户端任务优先级 */
    uint16_t client_task_stk;       /* 客户端任务优先级 */
    TaskHandle_t * client_handler;  /* 客户端任务控制块 */
    char *client_name;              /* 客户端任务名称 */
    char *client_num;               /* 客户端任务数量 */
};

/* socket信息 */
struct link_socjet_info
{
    int sock_listen;                /* 监听 */
    int sock_connect;               /* 连接 */
    struct sockaddr_in listen_addr; /* 监听地址 */
    struct sockaddr_in connect_addr;/* 连接地址 */
};

/**
 * @brief       客户端的任务函数
 * @param       pvParameters : 传入链接客户端的信息
 * @retval      无
 */
void lwip_client_thread_entry(void *param)
{
    struct client_info* client = param;
    /* 某个客户端连接 */
    printf("Client[%d]%s:%d is connect server\r\n", client->socket_num, inet_ntoa(client->ip_addr.sin_addr),ntohs(client->ip_addr.sin_port));
    /* 向客户端发送连接成功信息 */
    send(client->socket_num, (const void* )send_data, strlen(send_data), 0);
    
    while (1)
    {
        char str[100];
        memset(str, 0, sizeof(str));
        int bytes = recv(client->socket_num, str, sizeof(str), 0);
        
        /* 获取关闭连接的请求 */
        if (bytes <= 0)
        {
            mem_free(client);
            closesocket(client->socket_num);
            break;
        }
        
        printf("[%d]%s:%d=>%s...\r\n", client->socket_num, inet_ntoa(client->ip_addr.sin_addr),ntohs(client->ip_addr.sin_port), str);
        
        send((int )client->socket_num, (const void * )str, (size_t )strlen(str), 0);
    }
    
    printf("[%d]%s:%d is disconnect...\r\n", client->socket_num, inet_ntoa(client->ip_addr.sin_addr),ntohs(client->ip_addr.sin_port));
    
    vTaskDelete(NULL); /* 删除该任务 */
}

/**
 * @brief       lwip_demo实验入口
 * @param       无
 * @retval      无
 */
void lwip_demo(void)
{
    struct client_info *client_fo;
    struct client_task_info *client_task_fo;
    struct link_socjet_info *socket_link_info;
    int sin_size = sizeof(struct sockaddr_in);
    char client_name[10] = "cli";
    char client_num[10];
    
    /* socket连接结构体申请内存 */
    socket_link_info = mem_malloc(sizeof(struct link_socjet_info));
    
    /* 设置客户端任务信息 */
    client_task_fo = mem_malloc(sizeof(struct client_task_info));
    client_task_fo->client_handler = NULL;
    client_task_fo->client_task_pro = 5;
    client_task_fo->client_task_stk = 512;
    
    /* 创建socket连接 */
    if ((socket_link_info->sock_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Socket error\r\n");
        return;
    }

    /* 初始化连接的服务端地址 */
    socket_link_info->listen_addr.sin_family = AF_INET;
    socket_link_info->listen_addr.sin_port = htons(8088);
    socket_link_info->listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(socket_link_info->listen_addr.sin_zero), 0, sizeof(socket_link_info->listen_addr.sin_zero));

    /* 绑定socket和连接的服务端地址信息 */
    if (bind(socket_link_info->sock_listen, (struct sockaddr * )&socket_link_info->listen_addr, sizeof(struct sockaddr)) < 0)
    {
        printf("Bind fail!\r\n");
        goto __exit;
    }

    /* 监听客户端的数量 */
    listen(socket_link_info->sock_listen, 4);
    printf("begin listing...\r\n");

    while (1)
    {
        /* 请求客户端连接 */
        socket_link_info->sock_connect = accept(socket_link_info->sock_listen, (struct sockaddr* )&socket_link_info->connect_addr, (socklen_t* )&sin_size);
        
        if (socket_link_info->sock_connect == -1)
        {
            printf("no socket,waitting others socket disconnect.\r\n");
            continue;
        }
        
        lwip_itoa((char *)socket_link_info->sock_connect, (size_t)client_num, 10);
        strcat(client_name, client_num);
        client_task_fo->client_name = client_name;
        client_task_fo->client_num = client_num;
        
        /* 初始化连接客户端信息 */
        client_fo = mem_malloc(sizeof(struct client_info));
        client_fo->socket_num = socket_link_info->sock_connect;
        memcpy(&client_fo->ip_addr, &socket_link_info->connect_addr, sizeof(struct sockaddr_in));
        client_fo->sockaddr_len = sin_size;
        
        /* 创建连接的客户端任务 */
        xTaskCreate((TaskFunction_t )lwip_client_thread_entry,
                    (const char *   )client_task_fo->client_name,
                    (uint16_t       )client_task_fo->client_task_stk,
                    (void *         )(void*) client_fo,
                    (UBaseType_t    )client_task_fo->client_task_pro ++ ,
                    (TaskHandle_t * )&client_task_fo->client_handler);
        
        if (client_task_fo->client_handler == NULL)
        {

            printf("no memery for thread %s startup failed!\r\n",client_task_fo->client_name);
            mem_free(client_fo);
            continue;
        }
        else
        {
            printf("thread %s success!\r\n", client_task_fo->client_name);
        }
    }
    
__exit: 
    printf("listener failed\r\n");
    /* 关闭这个socket */
    closesocket(socket_link_info->sock_listen);
    vTaskDelete(NULL); /* 删除本任务 */
}
