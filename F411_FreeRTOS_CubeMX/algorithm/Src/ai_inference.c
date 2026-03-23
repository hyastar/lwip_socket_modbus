/**
 * @file    ai_inference.c
 * @brief   X-CUBE-AI推理封装
 *          简洁接口：AI_Init() + AI_Run()
 */

#include "ai_inference.h"
#include "network.h"
#include "network_data.h"

static ai_handle  network    = AI_HANDLE_NULL;
static ai_buffer *ai_input  = NULL;
static ai_buffer *ai_output = NULL;

/* 激活缓冲区：静态分配，不占任务栈 */
static uint8_t activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE]
    __attribute__((aligned(4)));

/**
 * @brief 初始化网络（只调用一次）
 * @return 0=成功, -1=失败
 */
int AI_Init(void)
{
    ai_error err;

    err = ai_network_create_and_init(
        &network,
        (const ai_handle[]){ activations },
        NULL);

    if (err.type != AI_ERROR_NONE)
        return -1;

    ai_input  = ai_network_inputs_get(network,  NULL);
    ai_output = ai_network_outputs_get(network, NULL);

    return 0;
}

/**
 * @brief 执行一次推理
 * @param input  float[400]，排列方式: input[c*100 + t]
 *               c=通道(0=mq2,1=mq3,2=mq5,3=mq138), t=时间步(0~99)
 * @param output float[2]，output[0]=空气概率, output[1]=酒精概率
 * @return 0=成功, -1=失败
 */
int AI_Run(float *input, float *output)
{
    if (network == AI_HANDLE_NULL)
        return -1;

    ai_input[0].data  = AI_HANDLE_PTR(input);
    ai_output[0].data = AI_HANDLE_PTR(output);

    if (ai_network_run(network, ai_input, ai_output) != 1)
        return -1;

    return 0;
}
