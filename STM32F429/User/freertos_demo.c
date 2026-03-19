
 
 #include "freertos_demo.h"
 #include "./BSP/LED/led.h"
 #include "./BSP/LCD/lcd.h"
 #include "./SYSTEM/usart/usart.h"
 #include "./SYSTEM/delay/delay.h"
 #include "./MALLOC/malloc.h"
 #include "./BSP/KEY/key.h"
 #include "lwip_comm.h"
 #include "lwip_demo.h"
 #include "lwipopts.h"
 #include "stdio.h"
 #include "string.h"
 #include "FreeRTOS.h"
 #include "task.h"
 #include "queue.h"
 #include "modbus_tcp_client.h"
 
 
 /******************************************************************************************************/
 /*FreeRTOS配置及任务函数声明*/
 
 /* START_TASK 任务配置
  * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
  */
 #define START_TASK_PRIO         5           /* 任务优先级 */
 #define START_STK_SIZE          512         /* 任务堆栈大小 */
 TaskHandle_t StartTask_Handler;             /* 任务句柄 */
 void start_task(void *pvParameters);        /* 任务函数声明 */
 
 /* LWIP_DEMO 任务配置
  * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
  */
 #define LWIP_DMEO_TASK_PRIO     11          /* 任务优先级 */
 #define LWIP_DMEO_STK_SIZE      1024        /* 任务堆栈大小 */
 TaskHandle_t LWIP_Task_Handler;             /* 任务句柄 */
 void lwip_demo_task(void *pvParameters);    /* 任务函数声明 */
 
 /* LED_TASK 任务配置
  * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
  */
 #define LED_TASK_PRIO           10          /* 任务优先级 */
 #define LED_STK_SIZE            128         /* 任务堆栈大小 */
 TaskHandle_t LEDTask_Handler;               /* 任务句柄 */
 void led_task(void *pvParameters);          /* 任务函数声明 */
 TaskHandle_t ModbusTCPTask_Handler;         /* Modbus TCP任务句柄 */
 
 /* MODBUS_TCP 任务配置 */
 #define MODBUS_TCP_TASK_PRIO      9           /* 任务优先级 */
 #define MODBUS_TCP_STK_SIZE       512         /* 任务堆栈大小 */
 void modbus_tcp_task(void *pvParameters);
 
 /* 以下为传感器数据采集接口声明，实际由外部文件实现 */
 uint16_t adc_get_mq1(void);
 uint16_t adc_get_mq2(void);
 int16_t  sensor_get_temp_x100(void);
 int16_t  sensor_get_humi_x100(void);    /* 温湿度传感器接口 */
 
 
 /* 显示消息队列，用于任务间传递LCD显示内容 */
 #define DISPLAYMSG_Q_NUM    20              /* 消息队列的消息数量 */
 QueueHandle_t g_display_queue;              /* 显示消息队列句柄 */
 
 /******************************************************************************************************/
 
 
 /**
  * @breif       显示测试UI界面
  * @param       mode :  bit0:0,不更新;1,更新静态部分UI
  *                      bit1:0,不更新;1,更新动态部分UI
  * @retval      无
  */
 void lwip_test_ui(uint8_t mode)
 {
     uint8_t speed;
     uint8_t buf[30];
     
     if (mode & 1<< 0)
     {
         lcd_fill(5, 30, lcddev.width,110, WHITE);
         lcd_show_string(5, 30, 200, 16, 16, "STM32", RED);
         lcd_show_string(5, 50, 200, 16, 16, "lwIP TCPServer MUTLienk Test", RED);
         lcd_show_string(5, 70, 200, 16, 16, "ATOM@ALIENTEK", RED);
     }
     
     if (mode & 1 << 1)
     {
         lcd_fill(5, 110, lcddev.width,lcddev.height, WHITE);
         lcd_show_string(5, 110, 200, 16, 16, "lwIP Init Successed", BLUE);
         
         if (g_lwipdev.dhcpstatus == 2)
         {
             sprintf((char*)buf,"DHCP IP:%d.%d.%d.%d",g_lwipdev.ip[0],g_lwipdev.ip[1],g_lwipdev.ip[2],g_lwipdev.ip[3]);     /* 显示DHCP获取的IP地址 */
         }
         else
         {
             sprintf((char*)buf,"Static IP:%d.%d.%d.%d",g_lwipdev.ip[0],g_lwipdev.ip[1],g_lwipdev.ip[2],g_lwipdev.ip[3]);    /* 显示静态配置的IP地址 */
         }
         
         lcd_show_string(5, 130, 200, 16, 16, (char*)buf, BLUE);
         
         speed = ethernet_chip_get_speed();      /* 获取网络速度 */
         
         if (speed)
         {
             lcd_show_string(5, 170, 200, 16, 16, "Ethernet Speed:100M", BLUE);
         }
         else
         {
             lcd_show_string(5, 170, 200, 16, 16, "Ethernet Speed:10M", BLUE);
         }
         
         lcd_show_string(5, 190, 200, 16, 16, "KEY0:Send data", RED);
         lcd_show_string(5, 210, lcddev.width - 30, lcddev.height - 190, 16, "Receive Data:", BLUE); /* 显示接收区标签 */
     }
 }
 
 /**
  * @breif       freertos_demo 入口函数
  * @param       无
  * @retval      无
  */
 void freertos_demo(void)
 {
     /* 创建start_task任务 */
     xTaskCreate((TaskFunction_t )start_task,
                 (const char *   )"start_task",
                 (uint16_t       )START_STK_SIZE,
                 (void *         )NULL,
                 (UBaseType_t    )START_TASK_PRIO,
                 (TaskHandle_t * )&StartTask_Handler);
 
     vTaskStartScheduler(); /* 开启任务调度器 */
 }
 
 /**
  * @brief       start_task 启动任务函数
  * @param       pvParameters : 传入参数(未用到)
  * @retval      无
  */
 void start_task(void *pvParameters)
 {
     pvParameters = pvParameters;
     
     g_lwipdev.lwip_display_fn = lwip_test_ui;
     
     lwip_test_ui(1);    /* 初始化并显示静态UI界面 */
     
     while(lwip_comm_init() != 0)
     {
         lcd_show_string(30, 110, 200, 16, 16, "lwIP Init failed!!", RED);
         delay_ms(500);
         lcd_fill(30, 50, 200 + 30, 50 + 16, WHITE);
         lcd_show_string(30, 110, 200, 16, 16, "Retrying...       ", RED);
         delay_ms(500);
         LED1_TOGGLE();
     }
     
     while (g_lwipdev.dhcpstatus != 2 && g_lwipdev.dhcpstatus != 0xff)/* 等待DHCP获取IP完成，或回退到静态IP */
     {
         vTaskDelay(500);
     }
     
     taskENTER_CRITICAL();           /* 进入临界区 */
     
     g_display_queue = xQueueCreate(DISPLAYMSG_Q_NUM,200);      /* 创建消息队列，队列长度为20，每条消息最大200字节 */
     
     /* 创建lwIP演示任务 */
     xTaskCreate((TaskFunction_t )lwip_demo_task,
                 (const char*    )"lwip_demo_task",
                 (uint16_t       )LWIP_DMEO_STK_SIZE, 
                 (void*          )NULL,
                 (UBaseType_t    )LWIP_DMEO_TASK_PRIO,
                 (TaskHandle_t*  )&LWIP_Task_Handler);
 
     /* 创建LED任务 */
     xTaskCreate((TaskFunction_t )led_task,
                 (const char*    )"led_task",
                 (uint16_t       )LED_STK_SIZE,
                 (void*          )NULL,
                 (UBaseType_t    )LED_TASK_PRIO,
                 (TaskHandle_t*  )&LEDTask_Handler);
 
     /* 创建Modbus TCP任务 */
     xTaskCreate((TaskFunction_t )modbus_tcp_task,
                 (const char*    )"modbus_tcp_task",
                 (uint16_t       )MODBUS_TCP_STK_SIZE,
                 (void*          )NULL,
                 (UBaseType_t    )MODBUS_TCP_TASK_PRIO,
                 (TaskHandle_t*  )&ModbusTCPTask_Handler);
 
     vTaskDelete(StartTask_Handler); /* 删除开始任务 */
     taskEXIT_CRITICAL();            /* 退出临界区 */
     
 }
 
 /**
  * @brief       lwIP演示任务函数
  * @param       pvParameters : 传入参数(未用到)
  * @retval      无
  */
 void lwip_demo_task(void *pvParameters)
 {
     pvParameters = pvParameters;
 
     lwip_demo();            /* 运行lwip演示程序 */
     
     while (1)
     {
         vTaskDelay(5);
     }
 }
 
 /**
  * @brief       LED闪烁任务函数
  * @param       pvParameters : 传入参数(未用到)
  * @retval      无
  */
 void led_task(void *pvParameters)
 {
     pvParameters = pvParameters;
 
     while(1)
     {
         LED1_TOGGLE();
         vTaskDelay(100);
     }
 }
 
 /**
  * @brief       Modbus TCP 任务函数，负责采集传感器数据并通过Modbus TCP上报
  * @param       pvParameters : 传入参数(未使用)
  * @retval      无
  */
 void modbus_tcp_task(void *pvParameters)
 {
     pvParameters = pvParameters;
 
     /* 等待 LwIP 初始化完成 (DHCP 获取到 IP) */
     while (g_lwipdev.dhcpstatus != 2) {
         if (g_lwipdev.dhcpstatus == 0xFF) {
             printf("[modbus_tcp] DHCP失败，使用静态IP: %d.%d.%d.%d\r\n",
                    g_lwipdev.ip[0], g_lwipdev.ip[1],
                    g_lwipdev.ip[2], g_lwipdev.ip[3]);
             break;
         }
         vTaskDelay(pdMS_TO_TICKS(500));
     }
 
     if (g_lwipdev.dhcpstatus == 2) {
         printf("[modbus_tcp] DHCP成功，IP: %d.%d.%d.%d\r\n",
                g_lwipdev.ip[0], g_lwipdev.ip[1],
                g_lwipdev.ip[2], g_lwipdev.ip[3]);
     }
 
     /* 初始化 Modbus TCP 客户端并连接服务器 */
     if (!ModbusTCP_Init(MODBUS_TCP_SERVER_IP, MODBUS_TCP_SERVER_PORT)) {
         printf("[modbus_tcp] 连接Modbus服务器失败，任务退出\r\n");
         vTaskDelete(NULL);
         return;
     }
 
     /* 周期性采集传感器数据并上报 */
     while (1) {
         uint16_t regs[REG_COUNT];
         regs[0] = adc_get_mq1();                     /* MQ1 ADC 值 */
         regs[1] = adc_get_mq2();                     /* MQ2 ADC 值 */
         regs[2] = (uint16_t)sensor_get_temp_x100();  /* 温度值 */
         regs[3] = (uint16_t)sensor_get_humi_x100();  /* 湿度值 */
         regs[4] = 0x0001;                            /* 设备在线状态标志 */
 
         if (ModbusTCP_WriteRegisters(regs)) {
             printf("[modbus_tcp] 数据上报成功 MQ1=%u MQ2=%u T=%.2f H=%.2f\r\n",
                    regs[0], regs[1],
                    regs[2] / 100.0f, regs[3] / 100.0f);
         } else {
             printf("[modbus_tcp] 数据上报失败，正在重连...\r\n");
             ModbusTCP_Reconnect();
         }
 
         vTaskDelay(pdMS_TO_TICKS(1000));
     }
 }
 
 /* 以下为占位弱函数默认实现，实际传感器驱动请在外部文件中实现并覆盖 */
 /* TODO: 需要替换为真实 ADC / 传感器驱动实现 */
 
 __attribute__((weak)) uint16_t adc_get_mq1(void)
 {
     return 500;
 }
 
 __attribute__((weak)) uint16_t adc_get_mq2(void)
 {
     return 300;
 }
 
 __attribute__((weak)) int16_t sensor_get_temp_x100(void)
 {
     return 2536;
 }
 
 __attribute__((weak)) int16_t sensor_get_humi_x100(void)
 {
     return 6012;
 }

 