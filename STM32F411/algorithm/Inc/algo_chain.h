#ifndef ALGO_CHAIN_H
#define ALGO_CHAIN_H

#include <stdint.h>

/* ── 通道索引 ── */
#define CH_MQ2    0
#define CH_MQ3    1
#define CH_MQ5    2
#define CH_MQ138  3
#define CH_COUNT  4

/* ── 采样参数 ── */
#define WINDOW_SIZE   100    /* 滑动窗口长度，对应Python T=100 */
#define SAMPLE_RATE   10     /* Hz */

/* ─────────────────────────────────────────────
 * 一阶卡尔曼滤波器
 * ───────────────────────────────────────────── */
typedef struct {
    float x;   /* 状态估计 */
    float p;   /* 误差协方差 */
    float q;   /* 过程噪声方差 */
    float r;   /* 观测噪声方差 */
} KalmanFilter_t;

/* ─────────────────────────────────────────────
 * 算法链上下文
 * 每个通道独立维护状态
 * ───────────────────────────────────────────── */
typedef struct {
    /* 双级卡尔曼 */
    KalmanFilter_t kf_fast[CH_COUNT];   /* 高速KF: Q大R小 */
    KalmanFilter_t kf_slow[CH_COUNT];   /* 低速KF: Q小R大 */

    /* Rs/R0基线 */
    float V0[CH_COUNT];          /* 开机标定的基准电压 */
    float Vc;                    /* 供电电压（实测值，约5.0V）*/

    /* 是否已完成基线标定 */
    uint8_t calibrated;

    /* 滑动窗口缓冲区 */
    float window[CH_COUNT][WINDOW_SIZE];
    uint16_t win_idx;            /* 当前写入位置 */
    uint16_t win_count;          /* 已填充样本数 */

    /* 温湿度（由TH传感器任务更新）*/
    float temperature;
    float humidity;
} AlgoChain_t;

/* ─────────────────────────────────────────────
 * 公开接口
 * ───────────────────────────────────────────── */

/**
 * @brief 初始化算法链（上电调用一次）
 * @param ctx  算法链上下文
 * @param Vc   实测供电电压（如4.95f）
 */
void AlgoChain_Init(AlgoChain_t *ctx, float Vc);

/**
 * @brief 标定基线R0（开机预热后在清洁环境中调用）
 * @param ctx     算法链上下文
 * @param V0      各通道基准电压（ADS1115采集的稳定值）
 */
void AlgoChain_Calibrate(AlgoChain_t *ctx, float V0[CH_COUNT]);

/**
 * @brief 处理一个新采样点，推入窗口
 * @param ctx     算法链上下文
 * @param vout    各通道原始电压（ADS1115读值，单位V）
 * @return 1=窗口已满可以推理, 0=窗口未满继续采集
 */
uint8_t AlgoChain_Feed(AlgoChain_t *ctx, float vout[CH_COUNT]);

/**
 * @brief 获取当前窗口数据（窗口满时调用）
 *        输出格式: out[t][c]，直接填入InferenceWindow_t.data
 * @param ctx  算法链上下文
 * @param out  float[WINDOW_SIZE][CH_COUNT]
 */
void AlgoChain_GetWindow(AlgoChain_t *ctx,
                         float out[WINDOW_SIZE][CH_COUNT]);

/**
 * @brief 更新温湿度（由TH传感器任务定时调用）
 */
void AlgoChain_UpdateTH(AlgoChain_t *ctx,
                        float temp, float humi);

#endif /* ALGO_CHAIN_H */
