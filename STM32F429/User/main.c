/**
 ****************************************************************************************************
 * @file        main.c
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

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./USMART/usmart.h"
#include "./BSP/SDRAM/sdram.h"
#include "./MALLOC/malloc.h"
#include "./BSP/PCF8574/pcf8574.h"
#include "lwip_comm.h"
#include "lwipopts.h"
#include "freertos_demo.h"


int main(void)
{
    HAL_Init();                                 /* 初始化HAL库 */
    sys_stm32_clock_init(360, 25, 2, 8);        /* 设置时钟,180Mhz */
    delay_init(180);                            /* 延时初始化 */
    usart_init(115200);                         /* 串口初始化为115200 */
    usmart_dev.init(90);                        /* 初始化USMART */
    led_init();                                 /* 初始化LED */
    key_init();                                 /* 初始化按键 */
    sdram_init();                               /* SRAM初始化 */
    lcd_init();                                 /* 初始化LCD */
    
    while (pcf8574_init())                      /* 检测不到PCF8574 */
    {
        lcd_show_string(30, 170, 200, 16, 16, "PCF8574 Check Failed!", RED);
        delay_ms(500);
        lcd_show_string(30, 170, 200, 16, 16, "Please Check!      ", RED);
        delay_ms(500);
        LED0_TOGGLE();     /* 红灯闪烁 */
    }
    
    my_mem_init(SRAMIN);                        /* 初始化内部内存池 */
    my_mem_init(SRAMEX);                        /* 初始化外部内存池 */
    my_mem_init(SRAMCCM);                       /* 初始化CCM内存池 */

    freertos_demo();
}
