/**************************************************************************************************
  Filename:       bsp_i2c.c
  Revised:        $Date: 2025-10-30 $

  Description:    CC2530 软件模拟 I2C 驱动层 (BSP) 实现文件。
                  (SDA: P0.5, SCL: P0.6)
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <ioCC2530.h>
#include "bsp_i2c.h"

/*********************************************************************
 * MACROS
 */

// --- 软件 I2C 引脚定义 ---
#define SCL P0_6
#define SDA P0_5

// --- 软件 I2C 引脚方向宏 ---
#define SDA_OUT() (P0DIR |= 0x20)  // P0.5 (SDA) 设置为输出
#define SDA_IN()  (P0DIR &= ~0x20) // P0.5 (SDA) 设置为输入
#define SCL_OUT() (P0DIR |= 0x40)  // P0.6 (SCL) 设置为输出

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/**
 * @brief   I2C 使用的微秒级短延时 (基于 32MHz 时钟)。
 */
static void bsp_I2C_Delay(void)
{
    // 几个 NOP 指令在 32MHz 时钟下提供足够的延时 (约 0.15us)
    __asm("NOP");
    __asm("NOP");
    __asm("NOP");
    __asm("NOP");
    __asm("NOP");
}
 
/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief   初始化软件 I2C 总线的 GPIO 引脚 (P0.5, P0.6)。
 */
void bsp_I2C_Init(void)
{
    // 1. 将 P0.5 (SDA) 和 P0.6 (SCL) 设置为通用 GPIO
    P0SEL &= ~0x60;
    
    // 2. 启用 P0 端口的上拉电阻 (通过 P2INP.0 = 0)
    //    (注意: 原示例 P2INP &= ~0x02; 是 P1 端口, 这里修正为 P0)
    P2INP &= ~0x01; 
    
    // 3. 将 P0.5 (SDA) 设置为上拉/下拉模式 (由 P2INP 控制)
    //    (SDA 在 IN 模式时需要上拉)
    P0INP &= ~0x20; 
    
    // 4. SCL 和 SDA 均设置为输出
    SCL_OUT();
    SDA_OUT();
    
    // 5. 设置总线空闲状态 (SCL=1, SDA=1)
    SCL = 1;
    SDA = 1;
}

/**
 * @brief   发送 I2C 起始信号。
 * (SCL 高电平期间, SDA 下降沿)
 */
void bsp_I2C_Start(void)
{
    SDA_OUT(); // 确保 SDA 是输出
    SDA = 1;
    SCL = 1;
    bsp_I2C_Delay();
    SDA = 0; // SCL高电平期间, SDA下降沿
    bsp_I2C_Delay();
    SCL = 0; // 钳住I2C总线, 准备发送数据
}

/**
 * @brief   发送 I2C 停止信号。
 * (SCL 高电平期间, SDA 上升沿)
 */
void bsp_I2C_Stop(void)
{
    SDA_OUT(); // 确保 SDA 是输出
    SCL = 0;
    SDA = 0;
    bsp_I2C_Delay();
    SCL = 1;
    bsp_I2C_Delay();
    SDA = 1; // SCL高电平期间, SDA上升沿
    bsp_I2C_Delay();
}

/**
 * @brief   等待 I2C 应答信号。
 * @return  0=收到应答 (ACK), 1=未收到应答 (NACK)
 */
uint8 bsp_I2C_GetAck(void)
{
    uint8 ack;
    SCL = 0;
    SDA_IN(); // SDA设置为输入, 准备接收ACK
    bsp_I2C_Delay();
    SCL = 1;
    bsp_I2C_Delay();
    
    ack = SDA; // 读取ACK信号
    
    SCL = 0;
    return ack;
}

/**
 * @brief   发送 I2C 应答信号。
 * @param   ack - 0=发送ACK, 1=发送NACK
 */
void bsp_I2C_SendAck(uint8 ack)
{
    SCL = 0;
    SDA_OUT(); // SDA设置为输出
    if(ack)
        SDA = 1; // NACK
    else
        SDA = 0; // ACK
    bsp_I2C_Delay();
    SCL = 1;
    bsp_I2C_Delay();
    SCL = 0;
}

/**
 * @brief   通过 I2C 发送一个字节。
 * (MSB first)
 */
void bsp_I2C_SendByte(uint8 dat)
{
    uint8 i;
    SDA_OUT();
    for(i=0; i<8; i++)
    {
        SCL = 0;
        bsp_I2C_Delay();
        if(dat & 0x80) // 发送最高位
            SDA = 1;
        else
            SDA = 0;
        dat <<= 1;
        bsp_I2C_Delay();
        SCL = 1; // 拉高SCL, 触发从机读取
        bsp_I2C_Delay();
    }
    SCL = 0;
}

/**
 * @brief   从 I2C 读取一个字节。
 * (MSB first)
 */
uint8 bsp_I2C_ReadByte(void)
{
    uint8 i, dat = 0;
    SDA_IN(); // SDA设置为输入
    for(i=0; i<8; i++)
    {
        SCL = 0;
        bsp_I2C_Delay();
        SCL = 1; // 拉高SCL, 触发从机发送
        bsp_I2C_Delay();
        dat <<= 1;
        if(SDA)
            dat |= 0x01;
    }
    SCL = 0;
    return dat;
}
