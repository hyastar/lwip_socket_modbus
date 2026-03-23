#include "bsp_ads1115.h"
#include "FreeRTOS.h"
#include "task.h"

static const uint16_t MUX_TABLE[4] = {
    ADS1115_MUX_AIN0,
    ADS1115_MUX_AIN1,
    ADS1115_MUX_AIN2,
    ADS1115_MUX_AIN3,
};

HAL_StatusTypeDef BSP_ADS1115_Init(void)
{
    /* 检测设备是否响应 */
    return HAL_I2C_IsDeviceReady(ADS1115_I2C,
                                  ADS1115_ADDR, 3, 100);
}

/**
 * @brief 读取全部4个通道电压
 * @param voltage 输出数组 float[4]，单位V
 */
HAL_StatusTypeDef BSP_ADS1115_ReadAllChannels(
    float voltage[ADS1115_CH_COUNT])
{
    HAL_StatusTypeDef ret;

    for (int ch = 0; ch < ADS1115_CH_COUNT; ch++) {
        /* 构造配置寄存器 */
        uint16_t cfg = ADS1115_OS_START
                     | MUX_TABLE[ch]
                     | ADS1115_PGA_6144
                     | ADS1115_MODE_SINGLE
                     | ADS1115_DR_128SPS
                     | ADS1115_COMP_DISABLE;

        uint8_t buf[3];
        buf[0] = ADS1115_REG_CFG;
        buf[1] = (cfg >> 8) & 0xFF;
        buf[2] =  cfg       & 0xFF;

        /* 写配置寄存器，启动转换 */
        ret = HAL_I2C_Master_Transmit(
            ADS1115_I2C, ADS1115_ADDR, buf, 3, 100);
        if (ret != HAL_OK) return ret;

        /* 128SPS下等待转换完成（约10ms） */
        vTaskDelay(pdMS_TO_TICKS(12));

        /* 指向转换结果寄存器 */
        buf[0] = ADS1115_REG_CONV;
        ret = HAL_I2C_Master_Transmit(
            ADS1115_I2C, ADS1115_ADDR, buf, 1, 100);
        if (ret != HAL_OK) return ret;

        /* 读2字节结果 */
        uint8_t raw[2];
        ret = HAL_I2C_Master_Receive(
            ADS1115_I2C, ADS1115_ADDR, raw, 2, 100);
        if (ret != HAL_OK) return ret;

        /* 拼成有符号16位，单端测量不会为负 */
        int16_t val = (int16_t)((raw[0] << 8) | raw[1]);
        if (val < 0) val = 0;

        voltage[ch] = (float)val * ADS1115_LSB;
    }

    return HAL_OK;
}
