#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "algo_chain.h"

/* 预热时间：MQ传感器上电预热60秒 */
#define MQ_WARMUP_MS            60000

/* 基线标定采样次数 */
#define CALIBRATION_SAMPLES     50

/* 采样周期：100ms = 10Hz */
#define SENSOR_PERIOD_MS        100

/* TH更新周期：2秒更新一次温湿度 */
#define TH_UPDATE_PERIOD_MS     2000

/* 任务栈和优先级 */
#define SENSOR_TASK_STACK       512
#define SENSOR_TASK_PRIO        3     /* 高于InferenceTask */

extern TaskHandle_t xSensorTaskHandle;

void SensorTask_Init(void);
void SensorTask(void *argument);

#endif /* SENSOR_TASK_H */
