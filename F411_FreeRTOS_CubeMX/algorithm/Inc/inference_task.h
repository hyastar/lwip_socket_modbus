#ifndef INFERENCE_TASK_H
#define INFERENCE_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

/**
 * 数据窗口结构体
 * SensorTask完成算法链后填充，发送到xInferenceQueue
 * data[t][c]: t=时间步(0~99), c=通道(0=mq2,1=mq3,2=mq5,3=mq138)
 */
typedef struct {
    float data[100][4];
} InferenceWindow_t;

/* 队列和任务句柄 */
extern QueueHandle_t xInferenceQueue;
extern TaskHandle_t  xInferenceTaskHandle;

/* 推理结果（CommTask读取）*/
extern volatile uint8_t g_inference_result;
extern volatile float   g_prob_air;
extern volatile float   g_prob_alcohol;

/* 初始化入口 */
void InferenceTask_Init(void);
void InferenceTask(void *argument);

#endif /* INFERENCE_TASK_H */
