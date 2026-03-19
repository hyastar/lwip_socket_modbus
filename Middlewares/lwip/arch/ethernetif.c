/**
  ******************************************************************************
  * @file    LwIP/LwIP_HTTP_Server_Raw/Src/ethernetif.c
  * @author  MCD Application Team
  * @brief   This file implements Ethernet network interface drivers for lwIP
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip_comm.h"
#include "ethernetif.h"  
#include "string.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "./BSP/ETHERNET/ethernet_chip.h"
#include "./SYSTEM/delay/delay.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* The time to block waiting for input. */
#define TIME_WAITING_FOR_INPUT                  ( portMAX_DELAY )
/* Stack size of the interface thread */
#define INTERFACE_THREAD_STACK_SIZE             ( 512 )
#define NETIF_IN_TASK_PRIORITY                  ( 2 )

/* Define those to better describe your network interface. */
#define IFNAME0 's'
#define IFNAME1 't'

#define ETH_DMA_TRANSMIT_TIMEOUT                (20U)

#define ETH_RX_BUFFER_SIZE                      1524                        /* Packets of this app's primary service protocol are smaller
                                                                                * than this. Typical size is 1536. */
#define ETH_RX_BUFFER_CNT                       10                          /* This app buffers receive packets of its primary service
                                                                                * protocol for processing later. */
#define ETH_TX_BUFFER_MAX                       ((ETH_TX_DESC_CNT) * 2)     /* HAL_ETH_Transmit(_IT) may attach two
                                                                                * buffers per descriptor. */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/*
@Note: This interface is implemented to operate in zero-copy mode only:
        - Rx Buffers will be allocated from LwIP stack memory heap,
          then passed to ETH HAL driver.
        - Tx Buffers will be allocated from LwIP stack memory heap,
          then passed to ETH HAL driver.

@Notes:
  1.a. ETH DMA Rx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_RX_DESC_CNT in ETH GUI (Rx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h
  1.b. ETH DMA Tx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_TX_DESC_CNT in ETH GUI (Tx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h

  2.a. Rx Buffers number: ETH_RX_BUFFER_CNT must be greater than ETH_RX_DESC_CNT.
  2.b. Rx Buffers must have the same size: ETH_RX_BUFFER_SIZE, this value must
       passed to ETH DMA in the init field (heth.Init.RxBuffLen)
*/
typedef enum
{
    RX_ALLOC_OK       = 0x00,
    RX_ALLOC_ERROR    = 0x01
} RxAllocStatusTypeDef;

typedef struct
{
    struct pbuf_custom pbuf_custom;
    uint8_t buff[(ETH_RX_BUFFER_SIZE + 31) & ~31] __ALIGNED(32);
} RxBuff_t;


/* Memory Pool Declaration */
LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");

static uint8_t RxAllocStatus;


xSemaphoreHandle g_tx_semaphore = NULL;  /* 定义一个RX信号量 */
xSemaphoreHandle g_rx_semaphore = NULL;  /* 定义一个TX信号量 */

/* Global Ethernet handle*/
ETH_TxPacketConfig TxConfig;
eth_chip_object_t ETHCHIP;

/* Private function prototypes -----------------------------------------------*/
void ethernetif_input( void * argument );
void RMII_Thread( void * argument );
int32_t ETH_PHY_IO_Init(void);
int32_t ETH_PHY_IO_DeInit (void);
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
int32_t ETH_PHY_IO_GetTick(void);
void pbuf_free_custom(struct pbuf *p);

eth_chip_ioc_tx_t  ETH_CHIP_IOCtx = {   ETH_PHY_IO_Init,
                                        ETH_PHY_IO_DeInit,
                                        ETH_PHY_IO_WriteReg,
                                        ETH_PHY_IO_ReadReg,
                                        ETH_PHY_IO_GetTick};

/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH) 
*******************************************************************************/
/**
  * @brief In this function, the hardware should be initialized.
  * Called from ethernetif_init().
  *
  * @param netif the already initialized lwip network interface structure
  *        for this ethernetif
  */
static void
low_level_init(struct netif *netif)
{
    int32_t phy_link_state = 0;
    uint32_t duplex = 0;
    uint32_t speed = 0;
    ETH_MACConfigTypeDef g_eth_macconfig_handler = {0};
    
    ethernet_init();                        /* 初始化以太网IO */
    
    
    netif->hwaddr_len = ETHARP_HWADDR_LEN;  /* 设置MAC地址长度,为6个字节 */
    /* 初始化MAC地址,设置什么地址由用户自己设置,但是不能与网络中其他设备MAC地址重复 */
    netif->hwaddr[0] = g_lwipdev.mac[0]; 
    netif->hwaddr[1] = g_lwipdev.mac[1]; 
    netif->hwaddr[2] = g_lwipdev.mac[2];
    netif->hwaddr[3] = g_lwipdev.mac[3];   
    netif->hwaddr[4] = g_lwipdev.mac[4];
    netif->hwaddr[5] = g_lwipdev.mac[5];
    
    netif->mtu = ETH_MAX_PAYLOAD; /* 最大允许传输单元,允许该网卡广播和ARP功能 */

    /* 网卡状态信息标志位，是很重要的控制字段，它包括网卡功能使能、广播 */
    /* 使能、 ARP 使能等等重要控制位 */
    netif->flags |= NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP;   /* 广播 ARP协议 链接检测 */
    
    /* Initialize the RX POOL */
    LWIP_MEMPOOL_INIT(RX_POOL);

    /* 配置发送报文配置常用参数 */
    memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
    TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    /* create a binary semaphore used for informing ethernetif of frame reception */
    g_rx_semaphore = xSemaphoreCreateBinary();

    /* create a binary semaphore used for informing ethernetif of frame transmission */
    g_tx_semaphore = xSemaphoreCreateBinary();

    /* create the task that handles the ETH_MAC */
    sys_thread_new("eth_thread",
                   ethernetif_input,            /* 任务入口函数 */
                   netif,                       /* 任务入口函数参数 */
                   INTERFACE_THREAD_STACK_SIZE, /* 任务栈大小 */
                   NETIF_IN_TASK_PRIORITY);     /* 任务的优先级 */

    /* 设置PHY IO功能 */
    eth_chip_regster_bus_io(&ETHCHIP, &ETH_CHIP_IOCtx);

    /* 初始化ETH PHY */
    eth_chip_init(&ETHCHIP);
    
    /* 必须开启自动协商功能 */
    eth_chip_start_auto_nego(&ETHCHIP);
    
    delay_ms(2000);        /* 必须等待初始化 */
    phy_link_state = eth_chip_get_link_state(&ETHCHIP);
    
    if (phy_link_state == ETH_CHIP_STATUS_READ_ERROR)
    {
        netif_set_link_down(netif);
        netif_set_down(netif);
    }
    else
    {
        switch (phy_link_state)
        {
            case ETH_CHIP_STATUS_100MBITS_FULLDUPLEX:
                duplex = ETH_FULLDUPLEX_MODE;
                speed = ETH_SPEED_100M;
                break;
            case ETH_CHIP_STATUS_100MBITS_HALFDUPLEX:
                duplex = ETH_HALFDUPLEX_MODE;
                speed = ETH_SPEED_100M;
                break;
            case ETH_CHIP_STATUS_10MBITS_FULLDUPLEX:
                duplex = ETH_FULLDUPLEX_MODE;
                speed = ETH_SPEED_10M;
                break;
            case ETH_CHIP_STATUS_10MBITS_HALFDUPLEX:
                duplex = ETH_HALFDUPLEX_MODE;
                speed = ETH_SPEED_10M;
                break;
            default:
                duplex = ETH_FULLDUPLEX_MODE;
                speed = ETH_SPEED_100M;
                break;
        }
    }
    
    /* 配置MAC */
    HAL_ETH_GetMACConfig(&g_eth_handler, &g_eth_macconfig_handler);
    g_eth_macconfig_handler.DuplexMode = duplex;
    g_eth_macconfig_handler.Speed = speed;
    HAL_ETH_SetMACConfig(&g_eth_handler,&g_eth_macconfig_handler);    
    HAL_ETH_Start_IT(&g_eth_handler);

    while (!ethernet_read_phy(ETH_CHIP_PHYSCSR))  /* 检查MCU与PHY芯片是否通信成功 */
    {
        printf("MCU与PHY芯片通信失败，请检查电路或者源码！！！！\r\n");
    }
    
    if (HAL_GetREVID() == 0x1000)
    { 
        /* 
        This thread will keep resetting the RMII interface until good frames are received
        */
        sys_thread_new( "rmii_thread",
                       RMII_Thread,                     /* 任务入口函数 */
                       NULL,                            /* 任务入口函数参数 */
                       configMINIMAL_STACK_SIZE,        /* 任务栈大小 */
                       NETIF_IN_TASK_PRIORITY + 1);     /* 任务的优先级 */
    }

}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet was sent, or ERR_IF if the packet was unable to be sent
 *
 * @note ERR_OK means the packet was sent (but not necessarily transmit complete),
 * and ERR_IF means the packet has more chained buffers than what the interface supports.
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    uint32_t i = 0U;
    struct pbuf *q = NULL;
    err_t errval = ERR_OK;
    ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT] = {0};

    memset(Txbuffer, 0 , ETH_TX_DESC_CNT*sizeof(ETH_BufferTypeDef));

    for (q = p; q != NULL; q = q->next)
    {
        if (i >= ETH_TX_DESC_CNT)
        return ERR_IF;

        Txbuffer[i].buffer = q->payload;
        Txbuffer[i].len = q->len;

        if (i > 0)
        {
            Txbuffer[i-1].next = &Txbuffer[i];
        }

        if (q->next == NULL)
        {
            Txbuffer[i].next = NULL;
        }

        i++;
    }

    TxConfig.Length = p->tot_len;
    TxConfig.TxBuffer = Txbuffer;
    TxConfig.pData = p;

    pbuf_ref(p);

    HAL_ETH_Transmit_IT(&g_eth_handler, &TxConfig);

    while (xSemaphoreTake(g_tx_semaphore, TIME_WAITING_FOR_INPUT) != pdTRUE)
    {
    }

    HAL_ETH_ReleaseTxPacket(&g_eth_handler);

    return errval;
}

/**
  * @brief Should allocate a pbuf and transfer the bytes of the incoming
  * packet from the interface into the pbuf.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return a pbuf filled with the received packet (including MAC header)
  *         NULL on memory error
  */
static struct pbuf * low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;

    if (RxAllocStatus == RX_ALLOC_OK)
    {
        HAL_ETH_ReadData(&g_eth_handler, (void **)&p);
    }

    return p;
}

/**
 * This task should be signaled when a receive packet is ready to be read
 * from the interface.
 *
 * @param argument the lwip network interface structure for this ethernetif
 */
void ethernetif_input( void * argument )
{
    struct pbuf *p = NULL;
    struct netif *netif = (struct netif *) argument;

    for( ;; )
    {
        if (xSemaphoreTake( g_rx_semaphore, TIME_WAITING_FOR_INPUT) == pdTRUE)
        {
            do
            {
                p = low_level_input( netif );
                if (p != NULL)
                {
                    if (netif->input( p, netif) != ERR_OK )
                    {
                        pbuf_free(p);
                    }
                }

            }while(p!=NULL);
        }
    }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */\
    
    /*
    * Initialize the snmp variables and counters inside the struct netif.
    * The last argument should be replaced with your link speed, in units
    * of bits per second.
    */
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
    * You can instead declare your own function an call etharp_output()
    * from it if you have to do some checks before sending (e.g. if link
    * is available...) */
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}

/**
  * @brief  Custom Rx pbuf free callback
  * @param  pbuf: pbuf to be freed
  * @retval None
  */
void pbuf_free_custom(struct pbuf *p)
{
    struct pbuf_custom* custom_pbuf = (struct pbuf_custom*)p;
    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
    /* If the Rx Buffer Pool was exhausted, signal the ethernetif_input task to
    * call HAL_ETH_GetRxDataBuffer to rebuild the Rx descriptors. */
    if (RxAllocStatus == RX_ALLOC_ERROR)
    {
        RxAllocStatus = RX_ALLOC_OK;
        portBASE_TYPE taskWoken = pdFALSE;

        if (xSemaphoreGiveFromISR(g_rx_semaphore, &taskWoken) == pdTRUE)
        {
            portEND_SWITCHING_ISR(taskWoken);
        }
    }
}

/**
  * @brief  Returns the current time in milliseconds
  *         when LWIP_TIMERS == 1 and NO_SYS == 1
  * @param  None
  * @retval Current Time value
  */
//u32_t sys_now(void)
//{
//    return HAL_GetTick();
//}
/**
  * @brief  Ethernet Rx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    portBASE_TYPE taskWoken = pdFALSE;
    
    if (xSemaphoreGiveFromISR(g_rx_semaphore, &taskWoken) == pdTRUE)
    {
        portEND_SWITCHING_ISR(taskWoken);
    }
}

/**
  * @brief  Ethernet Tx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
    portBASE_TYPE taskWoken = pdFALSE;
    
    if (xSemaphoreGiveFromISR(g_tx_semaphore, &taskWoken) == pdTRUE)
    {
        portEND_SWITCHING_ISR(taskWoken);
    }
}

/**
  * @brief  Ethernet DMA transfer error callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
    if ((HAL_ETH_GetDMAError(heth) & ETH_DMASR_RBUS) == ETH_DMASR_RBUS)
    {
        portBASE_TYPE taskWoken = pdFALSE;
        
        if (xSemaphoreGiveFromISR(g_rx_semaphore, &taskWoken) == pdTRUE)
        {
            portEND_SWITCHING_ISR(taskWoken);
        }
    }
}

/*******************************************************************************
                       PHI IO Functions
*******************************************************************************/
/**
  * @brief  Initializes the MDIO interface GPIO and clocks.
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */
int32_t ETH_PHY_IO_Init(void)
{
    /* We assume that MDIO GPIO configuration is already done
     in the ETH_MspInit() else it should be done here
    */

    /* Configure the MDIO Clock */
    HAL_ETH_SetMDIOClockRange(&g_eth_handler);

    return 0;
}

/**
  * @brief  De-Initializes the MDIO interface .
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */
int32_t ETH_PHY_IO_DeInit (void)
{
    return 0;
}

/**
  * @brief  Read a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  pRegVal: pointer to hold the register value
  * @retval 0 if OK -1 if Error
  */
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
    if (HAL_ETH_ReadPHYRegister(&g_eth_handler, DevAddr, RegAddr, pRegVal) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

/**
  * @brief  Write a value to a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  RegVal: Value to be written
  * @retval 0 if OK -1 if Error
  */
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
    if (HAL_ETH_WritePHYRegister(&g_eth_handler, DevAddr, RegAddr, RegVal) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

/**
  * @brief  Get the time in millisecons used for internal PHY driver process.
  * @retval Time value
  */
int32_t ETH_PHY_IO_GetTick(void)
{
    return HAL_GetTick();
}

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
    struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
    
    if (p)
    {
        /* Get the buff from the struct pbuf address. */
        *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
        p->custom_free_function = pbuf_free_custom;
        /* Initialize the struct pbuf.
        * This must be performed whenever a buffer's allocated because it may be
        * changed by lwIP or the app, e.g., pbuf_free decrements ref. */
        pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
    }
    else
    {
        RxAllocStatus = RX_ALLOC_ERROR;
        *buff = NULL;
    }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
    struct pbuf **ppStart = (struct pbuf **)pStart;
    struct pbuf **ppEnd = (struct pbuf **)pEnd;
    struct pbuf *p = NULL;

    /* Get the struct pbuf from the buff address. */
    p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
    p->next = NULL;
    p->tot_len = 0;
    p->len = Length;

    /* Chain the buffer. */
    if (!*ppStart)
    {
        /* The first buffer of the packet. */
        *ppStart = p;
    }
    else
    {
        /* Chain the buffer to the end of the packet. */
        (*ppEnd)->next = p;
    }
    *ppEnd  = p;

    /* Update the total length of all the buffers of the chain. Each pbuf in the chain should have its tot_len
    * set to its own length, plus the length of all the following pbufs in the chain. */
    for (p = *ppStart; p != NULL; p = p->next)
    {
        p->tot_len += Length;
    }
}

void HAL_ETH_TxFreeCallback(uint32_t * buff)
{
    pbuf_free((struct pbuf *)buff);
}

/**
  * @brief  RMII interface watchdog thread
  * @param  argument
  * @retval None
  */
void RMII_Thread( void * argument )
{
    (void) argument; 

    while (1)
    {
        /* some unicast good packets are received */
        if (g_eth_handler.Instance->MMCRGUFCR > 0U)
        {
            /* RMII Init is OK: Delete the Thread */ 
            vTaskDelete(NULL);
        }    
        else if (g_eth_handler.Instance->MMCRFCECR > 10U) 
        {
            /* ETH received too many packets with CRC errors, resetting RMII */
            SYSCFG->PMC &= ~SYSCFG_PMC_MII_RMII_SEL;
            SYSCFG->PMC |= SYSCFG_PMC_MII_RMII_SEL;

            g_eth_handler.Instance->MMCCR |= ETH_MMCCR_CR;
        }
        else
        {
            /* Delay 200 ms */
            vTaskDelay(200);
        }
    }
}
