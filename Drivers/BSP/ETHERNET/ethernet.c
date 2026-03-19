/**
 ****************************************************************************************************
 * @file        ethernet.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-08-01
 * @brief       ETHERNET 驱动代码
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
 * 修改说明
 * V1.0 20211014
 * 第一次发布
 *
 ****************************************************************************************************
 */

#include "./BSP/ETHERNET/ethernet.h"
#include "./BSP/PCF8574/pcf8574.h"
#include "./BSP/ETHERNET/ethernet_chip.h"
#include "lwip_comm.h"
#include "./SYSTEM/delay/delay.h"
#include "./MALLOC/malloc.h"


ETH_HandleTypeDef   g_eth_handler;                               /* 以太网句柄 */
ETH_DMADescTypeDef  g_eth_dma_rx_dscr_tab[ETH_RX_DESC_CNT];      /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef  g_eth_dma_tx_dscr_tab[ETH_TX_DESC_CNT];      /* Ethernet Tx DMA Descriptors */

ETH_MACFilterConfigTypeDef g_eth_macfilterconfig_handler;
/**
 * @brief       以太网芯片初始化
 * @param       无
 * @retval      0,成功
 *              1,失败
 */
uint8_t ethernet_init(void)
{
    uint8_t macaddress[6];
    
    macaddress[0] = g_lwipdev.mac[0];
    macaddress[1] = g_lwipdev.mac[1];
    macaddress[2] = g_lwipdev.mac[2];
    macaddress[3] = g_lwipdev.mac[3];
    macaddress[4] = g_lwipdev.mac[4];
    macaddress[5] = g_lwipdev.mac[5];

    g_eth_handler.Instance = ETH;
    g_eth_handler.Init.MACAddr = macaddress;
    g_eth_handler.Init.MediaInterface = HAL_ETH_RMII_MODE;
    g_eth_handler.Init.RxDesc = g_eth_dma_rx_dscr_tab;
    g_eth_handler.Init.TxDesc = g_eth_dma_tx_dscr_tab;
    g_eth_handler.Init.RxBuffLen = 1000;
    
    if (HAL_ETH_Init(&g_eth_handler) == HAL_OK)
    {
        return 0;   /* 成功 */
    }
    else
    {
        return 1;  /* 失败 */
    }
}

/**
 * @brief       ETH底层驱动，时钟使能，引脚配置
 *    @note     此函数会被HAL_ETH_Init()调用
 * @param       heth:以太网句柄
 * @retval      无
 */
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
    GPIO_InitTypeDef gpio_init_struct;
    
    ETH_CLK_GPIO_CLK_ENABLE();          /* 开启ETH_CLK时钟 */
    ETH_MDIO_GPIO_CLK_ENABLE();         /* 开启ETH_MDIO时钟 */
    ETH_CRS_GPIO_CLK_ENABLE();          /* 开启ETH_CRS时钟 */
    ETH_MDC_GPIO_CLK_ENABLE();          /* 开启ETH_MDC时钟 */
    ETH_RXD0_GPIO_CLK_ENABLE();         /* 开启ETH_RXD0时钟 */
    ETH_RXD1_GPIO_CLK_ENABLE();         /* 开启ETH_RXD1时钟 */
    ETH_TX_EN_GPIO_CLK_ENABLE();        /* 开启ETH_TX_EN时钟 */
    ETH_TXD0_GPIO_CLK_ENABLE();         /* 开启ETH_TXD0时钟 */
    ETH_TXD1_GPIO_CLK_ENABLE();         /* 开启ETH_TXD1时钟 */
    __HAL_RCC_ETH_CLK_ENABLE();         /* 开启ETH时钟 */

    /* 网络引脚设置 RMII接口
     * ETH_MDIO -------------------------> PA2
     * ETH_MDC --------------------------> PC1
     * ETH_RMII_REF_CLK------------------> PA1
     * ETH_RMII_CRS_DV ------------------> PA7
     * ETH_RMII_RXD0 --------------------> PC4
     * ETH_RMII_RXD1 --------------------> PC5
     * ETH_RMII_TX_EN -------------------> PB11
     * ETH_RMII_TXD0 --------------------> PG13
     * ETH_RMII_TXD1 --------------------> PG14
     */

    /* PA1,2,7 */
    gpio_init_struct.Pin = ETH_CLK_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* 推挽复用 */
    gpio_init_struct.Pull = GPIO_NOPULL;                    /* 不带上下拉 */
    gpio_init_struct.Speed = GPIO_SPEED_HIGH;               /* 高速 */
    gpio_init_struct.Alternate = GPIO_AF11_ETH;             /* 复用为ETH功能 */
    HAL_GPIO_Init(ETH_CLK_GPIO_PORT, &gpio_init_struct);    /* ETH_CLK引脚模式设置 */
    
    gpio_init_struct.Pin = ETH_MDIO_GPIO_PIN;
    HAL_GPIO_Init(ETH_MDIO_GPIO_PORT, &gpio_init_struct);   /* ETH_MDIO引脚模式设置 */
    
    gpio_init_struct.Pin = ETH_CRS_GPIO_PIN;    
    HAL_GPIO_Init(ETH_CRS_GPIO_PORT, &gpio_init_struct);    /* ETH_CRS引脚模式设置 */

    /* PC1 */
    gpio_init_struct.Pin = ETH_MDC_GPIO_PIN;
    HAL_GPIO_Init(ETH_MDC_GPIO_PORT, &gpio_init_struct);    /* ETH_MDC初始化 */

    /* PC4 */
    gpio_init_struct.Pin = ETH_RXD0_GPIO_PIN;
    HAL_GPIO_Init(ETH_RXD0_GPIO_PORT, &gpio_init_struct);   /* ETH_RXD0初始化 */
    
    /* PC5 */
    gpio_init_struct.Pin = ETH_RXD1_GPIO_PIN;
    HAL_GPIO_Init(ETH_RXD1_GPIO_PORT, &gpio_init_struct);   /* ETH_RXD1初始化 */
    
    
    /* PB11,13,14 */
    gpio_init_struct.Pin = ETH_TX_EN_GPIO_PIN; 
    HAL_GPIO_Init(ETH_TX_EN_GPIO_PORT, &gpio_init_struct);  /* ETH_TX_EN初始化 */

    gpio_init_struct.Pin = ETH_TXD0_GPIO_PIN; 
    HAL_GPIO_Init(ETH_TXD0_GPIO_PORT, &gpio_init_struct);   /* ETH_TXD0初始化 */
    
    gpio_init_struct.Pin = ETH_TXD1_GPIO_PIN; 
    HAL_GPIO_Init(ETH_TXD1_GPIO_PORT, &gpio_init_struct);   /* ETH_TXD1初始化 */
    
    uint32_t regval;

    sys_intx_disable();                                     /* 关闭所有中断，复位过程不能被打断！ */
    /* 判断开发板是否是旧版本(老板卡板载的是LAN8720A，而新板卡板载的是YT8512C) */
    HAL_ETH_ReadPHYRegister(&g_eth_handler, ETH_CHIP_ADDR,2, &regval);
    
    if (regval && 0xFFF == 0xFFF)   /* 旧板卡（LAN8720A）引脚复位 */
    {
        pcf8574_write_bit(ETH_RESET_IO,1);                  /* 硬件复位 */
        delay_ms(100);
        pcf8574_write_bit(ETH_RESET_IO,0);                  /* 复位结束 */
        delay_ms(100);
    }
    else                            /* 新板卡（YT8512C）引脚复位 */
    {
        pcf8574_write_bit(ETH_RESET_IO,0);                  /* 硬件复位 */
        delay_ms(100);
        pcf8574_write_bit(ETH_RESET_IO,1);                  /* 复位结束 */
        delay_ms(100);
    }
    
    sys_intx_enable();                                      /* 开启所有中断 */
    /* Enable the Ethernet global Interrupt */
    HAL_NVIC_SetPriority(ETH_IRQn, 0x07, 0);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
}

/**
 * @breif       中断服务函数
 * @param       无
 * @retval      无
 */
void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&g_eth_handler);
}

/**
 * @breif       读取以太网芯片寄存器值
 * @param       reg：读取的寄存器地址
 * @retval      regval：返回读取的寄存器值
 */
uint32_t ethernet_read_phy(uint16_t reg)
{
    uint32_t regval;

    HAL_ETH_ReadPHYRegister(&g_eth_handler, ETH_CHIP_ADDR,reg, &regval);
    return regval;
}

/**
 * @breif       向以太网芯片指定地址写入寄存器值
 * @param       reg   : 要写入的寄存器
 * @param       value : 要写入的寄存器
 * @retval      无
 */
void ethernet_write_phy(uint16_t reg, uint16_t value)
{
    uint32_t temp = value;
    
    HAL_ETH_ReadPHYRegister(&g_eth_handler, ETH_CHIP_ADDR,reg, &temp);
}

/**
 * @breif       获得网络芯片的速度模式
 * @param       无
 * @retval      1:100M
                0:10M
 */
uint8_t ethernet_chip_get_speed(void)
{
    uint8_t speed;
    if(PHY_TYPE == LAN8720) 
    speed = ~((ethernet_read_phy(ETH_CHIP_PHYSCSR) & ETH_CHIP_SPEED_STATUS));         /* 从LAN8720的31号寄存器中读取网络速度和双工模式 */
    else if(PHY_TYPE == SR8201F)
    speed = ((ethernet_read_phy(ETH_CHIP_PHYSCSR) & ETH_CHIP_SPEED_STATUS) >> 13);    /* 从SR8201F的0号寄存器中读取网络速度和双工模式 */
    else if(PHY_TYPE == YT8512C)
    speed = ((ethernet_read_phy(ETH_CHIP_PHYSCSR) & ETH_CHIP_SPEED_STATUS) >> 14);    /* 从YT8512C的17号寄存器中读取网络速度和双工模式 */
    else if(PHY_TYPE == RTL8201)
    speed = ((ethernet_read_phy(ETH_CHIP_PHYSCSR) & ETH_CHIP_SPEED_STATUS) >> 1);     /* 从RTL8201的16号寄存器中读取网络速度和双工模式 */
    return speed;
}
