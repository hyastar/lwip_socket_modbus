/**
 ****************************************************************************************************
 * @file        ethernet.h
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
 * V1.0 20211202
 * 第一次发布
 *
 ****************************************************************************************************
 */
 
#ifndef __ETHERNET_H
#define __ETHERNET_H
#include "./SYSTEM/sys/sys.h"
#include "stm32f4xx_hal_conf.h"


/******************************************************************************************/
/* 引脚 定义 */

#define ETH_CLK_GPIO_PORT               GPIOA
#define ETH_CLK_GPIO_PIN                GPIO_PIN_1
#define ETH_CLK_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOA_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */

#define ETH_MDIO_GPIO_PORT              GPIOA
#define ETH_MDIO_GPIO_PIN               GPIO_PIN_2
#define ETH_MDIO_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOA_CLK_ENABLE();}while(0)                 /* 所在IO口时钟使能 */

#define ETH_CRS_GPIO_PORT               GPIOA
#define ETH_CRS_GPIO_PIN                GPIO_PIN_7
#define ETH_CRS_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOA_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */

#define ETH_MDC_GPIO_PORT               GPIOC
#define ETH_MDC_GPIO_PIN                GPIO_PIN_1
#define ETH_MDC_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOC_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */

#define ETH_RXD0_GPIO_PORT              GPIOC
#define ETH_RXD0_GPIO_PIN               GPIO_PIN_4
#define ETH_RXD0_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOC_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */

#define ETH_RXD1_GPIO_PORT              GPIOC
#define ETH_RXD1_GPIO_PIN               GPIO_PIN_5
#define ETH_RXD1_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOC_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */

#define ETH_TX_EN_GPIO_PORT             GPIOB
#define ETH_TX_EN_GPIO_PIN              GPIO_PIN_11
#define ETH_TX_EN_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOG_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */

#define ETH_TXD0_GPIO_PORT              GPIOG
#define ETH_TXD0_GPIO_PIN               GPIO_PIN_13
#define ETH_TXD0_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOG_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */

#define ETH_TXD1_GPIO_PORT              GPIOG
#define ETH_TXD1_GPIO_PIN               GPIO_PIN_14
#define ETH_TXD1_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOG_CLK_ENABLE();}while(0)                  /* 所在IO口时钟使能 */


/******************************************************************************************/

extern ETH_HandleTypeDef    g_eth_handler;                                      /* 以太网句柄 */
extern ETH_DMADescTypeDef   g_eth_dma_rx_dscr_tab[ETH_RX_DESC_CNT];                      /* Ethernet Rx DMA Descriptors */
extern ETH_DMADescTypeDef   g_eth_dma_tx_dscr_tab[ETH_TX_DESC_CNT];                      /* Ethernet Tx DMA Descriptors */

uint8_t     ethernet_init(void);                                                /* 以太网芯片初始化 */
uint32_t    ethernet_read_phy(uint16_t reg);                                    /* 读取以太网芯片寄存器值 */
void        ethernet_write_phy(uint16_t reg, uint16_t value);                   /* 向以太网芯片指定地址写入寄存器值 */
uint8_t     ethernet_chip_get_speed(void);                                      /* 获得以太网芯片的速度模式 */
#endif

