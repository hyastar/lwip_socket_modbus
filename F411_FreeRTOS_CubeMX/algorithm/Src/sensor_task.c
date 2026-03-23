/**
 * @file    sensor_task.c
 * @brief   传感器采集任务
 *
 * 执行流程：
 *  1. 等待MQ预热（60秒，可跳过调试）
 *  2. 标定基线V0（采50次平均）
 *  3. 循环采样：
 *     a. ADS1115读4路电压
 *     b. AlgoChain_Feed()处理算法链
 *     c. 每2秒更新一次SHT30温湿度
 *     d. 窗口满时发送给InferenceTask
 */

#include "sensor_task.h"
#include "inference_task.h"
#include "bsp_ads1115.h"
#include "bsp_sht30.h"
#include "algo_chain.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

TaskHandle_t xSensorTaskHandle = NULL;

/* 算法链上下文（静态分配，不占任务栈） */
static AlgoChain_t g_algo_ctx;

/* ─────────────────────────────────────────────
 * 基线标定
 * ───────────────────────────────────────────── */
static void do_calibration(void)
{
    float sum[CH_COUNT] = {0};
    float vout[CH_COUNT];

    for (int s = 0; s < CALIBRATION_SAMPLES; s++) {
        if (BSP_ADS1115_ReadAllChannels(vout) == HAL_OK) {
            for (int c = 0; c < CH_COUNT; c++) {
                sum[c] += vout[c];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    float V0[CH_COUNT];
    for (int c = 0; c < CH_COUNT; c++) {
        V0[c] = sum[c] / CALIBRATION_SAMPLES;
    }

    AlgoChain_Calibrate(&g_algo_ctx, V0);
}

/* ─────────────────────────────────────────────
 * SensorTask主体
 * ───────────────────────────────────────────── */
void SensorTask(void *argument)
{
    float vout[CH_COUNT];
    float temperature = 25.0f;
    float humidity    = 60.0f;
    InferenceWindow_t window;
    TickType_t last_th_tick  = 0;
    TickType_t last_sample   = xTaskGetTickCount();

    /* ── 初始化BSP ── */
    if (BSP_ADS1115_Init() != HAL_OK) {
        /* ADS1115初始化失败，挂起任务等待硬件就绪 */
        vTaskSuspend(NULL);
    }
    if (BSP_SHT30_Init() != HAL_OK) {
        /* SHT30初始化失败，使用默认温湿度继续运行 */
        temperature = 25.0f;
        humidity    = 60.0f;
    }

    /* ── 初始化算法链 ── */
    AlgoChain_Init(&g_algo_ctx, ADS1115_VC);

    /* ── 先读一次TH作为初始值 ── */
    BSP_SHT30_Read(&temperature, &humidity);
    AlgoChain_UpdateTH(&g_algo_ctx, temperature, humidity);

    /* ── MQ预热等待 ── */
    /* 调试阶段可注释掉下面这行跳过预热 */
    vTaskDelay(pdMS_TO_TICKS(MQ_WARMUP_MS));

    /* ── 基线标定 ── */
    do_calibration();

    /* ── 主采样循环 ── */
    for (;;) {
        /* 精确周期：100ms = 10Hz */
        vTaskDelayUntil(&last_sample,
                        pdMS_TO_TICKS(SENSOR_PERIOD_MS));

        /* 每2秒更新一次温湿度 */
        TickType_t now = xTaskGetTickCount();
        if ((now - last_th_tick) >=
            pdMS_TO_TICKS(TH_UPDATE_PERIOD_MS))
        {
            if (BSP_SHT30_Read(&temperature,
                                &humidity) == HAL_OK) {
                AlgoChain_UpdateTH(&g_algo_ctx,
                                   temperature, humidity);
            }
            last_th_tick = now;
        }

        /* 读4路ADS1115电压 */
        if (BSP_ADS1115_ReadAllChannels(vout) != HAL_OK) {
            continue;   /* I2C故障跳过本次 */
        }

        /* 算法链处理，窗口满则推理 */
        if (AlgoChain_Feed(&g_algo_ctx, vout)) {
            AlgoChain_GetWindow(&g_algo_ctx, window.data);
            /* 非阻塞发送，InferenceTask繁忙时丢弃本帧 */
            xQueueSend(xInferenceQueue, &window, 0);
        }
    }
}

void SensorTask_Init(void)
{
    xTaskCreate(SensorTask,
                "SensorTask",
                SENSOR_TASK_STACK,
                NULL,
                SENSOR_TASK_PRIO,
                &xSensorTaskHandle);
}
