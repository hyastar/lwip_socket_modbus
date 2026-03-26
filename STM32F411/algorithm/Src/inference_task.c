/**
 * @file    inference_task.c
 * @brief   推理任务 + SensorTask数据接收
 *
 * 数据流：
 *   SensorTask → xInferenceQueue → InferenceTask → g_result → CommTask
 *
 * 输入排列（重要）：
 *   Python: X[t][c]  (T=100行, C=4列)
 *   STM32:  input[c*100 + t]  必须转置后传入
 */

#include "inference_task.h"
#include "ai_inference.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

/* ── 队列和任务句柄 ── */
QueueHandle_t xInferenceQueue   = NULL;
TaskHandle_t  xInferenceTaskHandle = NULL;

/* ── 推理结果（CommTask读取后通过Modbus发给F429）── */
volatile uint8_t g_inference_result = 0;   /* 0=空气 1=酒精 */
volatile float   g_prob_air         = 0.0f;
volatile float   g_prob_alcohol     = 0.0f;

/* ── 推理用缓冲区 ── */
static float ai_input_flat[400];    /* (C=4, T=100) 展平 */
static float ai_output[2];

/**
 * @brief 将(T,C)格式转置为(C,T)展平
 *        SensorTask填充的是 window[t][c]
 *        AI_Run需要的是 input[c*100+t]
 */
static void transpose_input(InferenceWindow_t *win, float *out)
{
    for (int c = 0; c < 4; c++) {
        for (int t = 0; t < 100; t++) {
            out[c * 100 + t] = win->data[t][c];
        }
    }
}

/**
 * @brief InferenceTask主体
 */
void InferenceTask(void *argument)
{
    InferenceWindow_t window;

    /* 网络初始化 */
    if (AI_Init() != 0) {
        vTaskSuspend(NULL);   /* 初始化失败则挂起 */
    }

    for (;;) {
        /* 阻塞等待SensorTask发来的数据窗口 */
        if (xQueueReceive(xInferenceQueue,
                          &window,
                          portMAX_DELAY) == pdTRUE)
        {
            /* 转置输入 */
            transpose_input(&window, ai_input_flat);

            /* 执行推理 */
            if (AI_Run(ai_input_flat, ai_output) == 0) {
                g_prob_air      = ai_output[0];
                g_prob_alcohol  = ai_output[1];
                g_inference_result = (g_prob_alcohol >= 0.5f) ? 1 : 0;
            }
        }
    }
}

/**
 * @brief 创建队列和任务（在freertos.c的MX_FREERTOS_Init中调用）
 */
void InferenceTask_Init(void)
{
    xInferenceQueue = xQueueCreate(1, sizeof(InferenceWindow_t));

    xTaskCreate(InferenceTask,
                "InferTask",
                512,
                NULL,
                2,
                &xInferenceTaskHandle);
}
