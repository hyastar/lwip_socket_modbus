/**
 * @file    algo_chain.c
 * @brief   MQ传感器算法链（STM32F411 FPU实现）
 *
 * 算法链：
 *   Vout(ADS1115)
 *     → 高速卡尔曼（去瞬时噪声）
 *     → Rs/R0归一化（消除个体差异和基线漂移）
 *     → TH温湿度补偿（消除环境影响）
 *     → 低速卡尔曼（平滑，稳定CNN输入）
 *     → 滑动窗口（积累100个时间步）
 *     → 输出给InferenceTask
 *
 * 注意：所有浮点运算使用float（单精度），
 *       F411 FPU自动加速，无需额外配置
 */

#include "algo_chain.h"
#include <string.h>

/* ─────────────────────────────────────────────
 * 温湿度补偿表（手册数据，基准25°C/60%RH=1.00）
 * ───────────────────────────────────────────── */
static const float TH_TEMP[]  = {-10,-5,0,5,10,15,20,25,30,35,40,45,50};
static const float TH_RH30[]  = {1.71f,1.63f,1.59f,1.50f,1.43f,1.30f,
                                  1.25f,1.18f,1.15f,1.11f,1.01f,0.93f,0.87f};
static const float TH_RH60[]  = {1.45f,1.38f,1.35f,1.27f,1.22f,1.11f,
                                  1.05f,1.00f,0.98f,0.94f,0.86f,0.79f,0.73f};
static const float TH_RH85[]  = {1.26f,1.20f,1.17f,1.10f,1.05f,0.96f,
                                  0.92f,0.88f,0.86f,0.82f,0.73f,0.68f,0.64f};
#define TH_TABLE_LEN  13

/* ─────────────────────────────────────────────
 * 内部工具函数
 * ───────────────────────────────────────────── */

/* 线性插值 */
static float lerp(float x0, float y0, float x1, float y1, float x)
{
    if (x1 == x0) return y0;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

/* 在有序数组中查找插值位置 */
static float interp1d(const float *xs, const float *ys,
                      int len, float x)
{
    if (x <= xs[0])       return ys[0];
    if (x >= xs[len-1])   return ys[len-1];
    for (int i = 0; i < len-1; i++) {
        if (x >= xs[i] && x <= xs[i+1]) {
            return lerp(xs[i], ys[i], xs[i+1], ys[i+1], x);
        }
    }
    return ys[len-1];
}

/* 温湿度补偿系数（双线性插值）*/
static float get_th_factor(float temp, float humi)
{
    float v30 = interp1d(TH_TEMP, TH_RH30, TH_TABLE_LEN, temp);
    float v60 = interp1d(TH_TEMP, TH_RH60, TH_TABLE_LEN, temp);
    float v85 = interp1d(TH_TEMP, TH_RH85, TH_TABLE_LEN, temp);

    if (humi < 30.0f) humi = 30.0f;
    if (humi > 85.0f) humi = 85.0f;

    if (humi <= 60.0f) {
        return v30 + (v60 - v30) * (humi - 30.0f) / 30.0f;
    } else {
        return v60 + (v85 - v60) * (humi - 60.0f) / 25.0f;
    }
}

/* 一阶卡尔曼滤波单步更新 */
static float kalman_update(KalmanFilter_t *kf, float z)
{
    kf->p += kf->q;
    float K = kf->p / (kf->p + kf->r);
    kf->x  += K * (z - kf->x);
    kf->p  *= (1.0f - K);
    return kf->x;
}

/* ─────────────────────────────────────────────
 * 公开接口实现
 * ───────────────────────────────────────────── */

void AlgoChain_Init(AlgoChain_t *ctx, float Vc)
{
    memset(ctx, 0, sizeof(AlgoChain_t));
    ctx->Vc          = Vc;
    ctx->temperature = 25.0f;
    ctx->humidity    = 60.0f;
    ctx->calibrated  = 0;

    for (int c = 0; c < CH_COUNT; c++) {
        /* 高速KF: Q/R=0.1，响应快 */
        ctx->kf_fast[c].q = 0.01f;
        ctx->kf_fast[c].r = 0.10f;
        ctx->kf_fast[c].p = 1.0f;

        /* 低速KF: Q/R=0.002，平滑强 */
        ctx->kf_slow[c].q = 0.001f;
        ctx->kf_slow[c].r = 0.50f;
        ctx->kf_slow[c].p = 1.0f;
    }
}

void AlgoChain_Calibrate(AlgoChain_t *ctx, float V0[CH_COUNT])
{
    for (int c = 0; c < CH_COUNT; c++) {
        /* 防止除零 */
        ctx->V0[c] = (V0[c] > 0.05f) ? V0[c] : 0.05f;

        /* 用基准值初始化KF状态 */
        ctx->kf_fast[c].x = V0[c];
        ctx->kf_slow[c].x = 1.0f;   /* Rs/R0基准=1.0 */
    }
    ctx->calibrated = 1;
}

void AlgoChain_UpdateTH(AlgoChain_t *ctx, float temp, float humi)
{
    ctx->temperature = temp;
    ctx->humidity    = humi;
}

uint8_t AlgoChain_Feed(AlgoChain_t *ctx, float vout[CH_COUNT])
{
    /* 未标定则不处理 */
    if (!ctx->calibrated) return 0;

    float th_factor = get_th_factor(ctx->temperature,
                                    ctx->humidity);

    for (int c = 0; c < CH_COUNT; c++) {
        float v = vout[c];

        /* 防止电压异常 */
        if (v < 0.05f) v = 0.05f;

        /* ① 高速卡尔曼（电压域去噪）*/
        float v_filtered = kalman_update(&ctx->kf_fast[c], v);

        /* ② Rs/R0归一化
         *    Rs/R0 = (Vc/Vout - 1) / (Vc/V0 - 1)
         *    RL在分子分母中约掉，无需测量 */
        float num = ctx->Vc / v_filtered - 1.0f;
        float den = ctx->Vc / ctx->V0[c]  - 1.0f;
        float rs_r0 = (den > 0.001f) ? (num / den) : 1.0f;

        /* ③ 温湿度补偿 */
        float rs_r0_comp = rs_r0 / th_factor;

        /* 限幅：防止异常值污染CNN输入 */
        if (rs_r0_comp < 0.01f)  rs_r0_comp = 0.01f;
        if (rs_r0_comp > 3.0f)   rs_r0_comp = 3.0f;

        /* ④ 低速卡尔曼（特征域平滑）*/
        float smoothed = kalman_update(&ctx->kf_slow[c], rs_r0_comp);

        /* ⑤ 写入滑动窗口 */
        ctx->window[c][ctx->win_idx] = smoothed;
    }

    /* 更新窗口指针 */
    ctx->win_idx = (ctx->win_idx + 1) % WINDOW_SIZE;
    if (ctx->win_count < WINDOW_SIZE)
        ctx->win_count++;

    /* 返回1表示窗口已满，可以推理 */
    return (ctx->win_count >= WINDOW_SIZE) ? 1 : 0;
}

void AlgoChain_GetWindow(AlgoChain_t *ctx,
                         float out[WINDOW_SIZE][CH_COUNT])
{
    /* 窗口是环形缓冲，需要按时间顺序取出 */
    for (int t = 0; t < WINDOW_SIZE; t++) {
        int idx = (ctx->win_idx + t) % WINDOW_SIZE;
        for (int c = 0; c < CH_COUNT; c++) {
            out[t][c] = ctx->window[c][idx];
        }
    }
}
