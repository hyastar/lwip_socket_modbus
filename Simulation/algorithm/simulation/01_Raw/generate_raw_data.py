"""
01_Raw: 基于真实MQ传感器手册数据的仿真响应数据生成
传感器组合: MQ2 / MQ3 / MQ5 / MQ138
数据说明: 依据手册灵敏度曲线和温湿度特性生成的传感器响应仿真数据
         输出为Rs/R0域的响应序列，非ADC原始电压采样值
场景0: 正常空气 (Rs/R0 ≈ 1.0)
场景1: 喷洒酒精 (Rs/R0 按手册曲线下降)
"""

import numpy as np
import pandas as pd
import os
from scipy.interpolate import interp1d

# ─────────────────────────────────────────────
# 手册真实数据：各传感器对乙醇的灵敏度 (ppm -> Rs/R0)
# 注意：MQ2/MQ5手册起点为300ppm，100ppm为对数外推估计值
#       外推值高于0.43，仅用于仿真，不作为标定依据
# ─────────────────────────────────────────────
SENSITIVITY = {
    'mq2': {
        # 手册原始数据点（乙醇曲线）
        'ppm':   [300,   1000,  5000,  10000],
        'rs_r0': [0.43,  0.24,  0.095, 0.063],
        'note':  '乙醇曲线，300ppm起，低于300ppm为外推估计',
    },
    'mq3': {
        # 手册原始数据点（乙醇曲线，主攻传感器）
        'ppm':   [50,    100,   200,   5000],
        'rs_r0': [0.18,  0.085, 0.046, 0.012],
        'note':  '乙醇曲线，50ppm起，数据点覆盖目标浓度范围',
    },
    'mq5': {
        # 手册原始数据点（乙醇曲线，与MQ2相同）
        'ppm':   [300,   1000,  5000,  10000],
        'rs_r0': [0.43,  0.24,  0.095, 0.063],
        'note':  '乙醇曲线，300ppm起，低于300ppm为外推估计',
    },
    'mq138': {
        # 手册原始数据点（乙醇曲线）
        'ppm':   [10,    50,    100,   120,   150],
        'rs_r0': [0.42,  0.21,  0.14,  0.098, 0.062],
        'note':  '乙醇曲线，10ppm起，数据点覆盖目标浓度范围',
    },
}

# 通用温湿度补偿表（手册数据，基准：25°C/60%RH = 1.00）
TH_TABLE = {
    'temp': [-10,  -5,   0,    5,    10,   15,   20,   25,   30,   35,   40,   45,   50],
    '30rh': [1.71, 1.63, 1.59, 1.50, 1.43, 1.30, 1.25, 1.18, 1.15, 1.11, 1.01, 0.93, 0.87],
    '60rh': [1.45, 1.38, 1.35, 1.27, 1.22, 1.11, 1.05, 1.00, 0.98, 0.94, 0.86, 0.79, 0.73],
    '85rh': [1.26, 1.20, 1.17, 1.10, 1.05, 0.96, 0.92, 0.88, 0.86, 0.82, 0.73, 0.68, 0.64],
}

# ─────────────────────────────────────────────
# 参数配置
# ─────────────────────────────────────────────
SAMPLE_RATE = 10         # Hz
WINDOW_SEC  = 10         # 每条样本时间窗口（秒）
T           = SAMPLE_RATE * WINDOW_SEC   # 100个时间步

N_AIR      = 300
N_ALCOHOL  = 300
SEED       = 42
np.random.seed(SEED)

# 图书馆典型环境范围
TEMP_MEAN, TEMP_STD = 22.0, 3.0    # °C
HUMI_MEAN, HUMI_STD = 55.0, 10.0   # %RH

# 各传感器噪声水平
NOISE_STD = {
    'mq2':   0.015,
    'mq3':   0.012,
    'mq5':   0.018,
    'mq138': 0.014,
}

# 酒精喷雾浓度范围（ppm）
# MQ2/MQ5手册起点300ppm，仿真范围设为200~800ppm
# 使其落在手册数据覆盖范围内，避免过度依赖外推
ALCOHOL_PPM_RANGE = (200, 800)

CHANNELS = ['mq2', 'mq3', 'mq5', 'mq138']

OUTPUT_DIR = r'G:\Cursor\kato-modbus\Simulation\algorithm\Data\01_Raw'
os.makedirs(OUTPUT_DIR, exist_ok=True)


# ─────────────────────────────────────────────
# 工具函数
# ─────────────────────────────────────────────

def build_interpolator(channel):
    """对数域插值，符合MQ传感器log-log特性"""
    data = SENSITIVITY[channel]
    log_ppm   = np.log10(data['ppm'])
    log_rs_r0 = np.log10(data['rs_r0'])
    return interp1d(log_ppm, log_rs_r0,
                    kind='linear',
                    bounds_error=False,
                    fill_value='extrapolate')

INTERP = {ch: build_interpolator(ch) for ch in CHANNELS}


def ppm_to_rs_r0(channel, ppm):
    """由ppm浓度查询Rs/R0（对数域插值/外推）"""
    log_ppm = np.log10(max(ppm, 1.0))
    log_val = float(INTERP[channel](log_ppm))
    val = 10 ** log_val
    # 物理上限：Rs/R0不超过清洁空气基线的1.5倍
    return float(np.clip(val, 0.005, 1.5))


def get_th_factor(temp, humi):
    """
    双线性插值温湿度补偿系数
    基准：25°C / 60%RH = 1.00
    """
    temps = np.array(TH_TABLE['temp'], dtype=float)
    rh30  = np.array(TH_TABLE['30rh'], dtype=float)
    rh60  = np.array(TH_TABLE['60rh'], dtype=float)
    rh85  = np.array(TH_TABLE['85rh'], dtype=float)

    f30 = interp1d(temps, rh30, fill_value='extrapolate')
    f60 = interp1d(temps, rh60, fill_value='extrapolate')
    f85 = interp1d(temps, rh85, fill_value='extrapolate')

    v30 = float(f30(temp))
    v60 = float(f60(temp))
    v85 = float(f85(temp))

    humi = float(np.clip(humi, 30, 85))
    if humi <= 60:
        factor = v30 + (v60 - v30) * (humi - 30) / 30.0
    else:
        factor = v60 + (v85 - v60) * (humi - 60) / 25.0

    return factor


# ─────────────────────────────────────────────
# 波形生成
# ─────────────────────────────────────────────

def gen_air_sample(channel, temp, humi):
    """正常空气：Rs/R0在温湿度修正基线附近随机游走"""
    th_factor = get_th_factor(temp, humi)
    base = 1.0 * th_factor

    # 低频慢漂移
    drift = np.cumsum(np.random.randn(T) * 0.003)
    drift -= drift.mean()

    signal = base + drift + np.random.randn(T) * NOISE_STD[channel]
    return np.clip(signal, 0.3, 3.0)


def gen_alcohol_sample(channel, temp, humi, ppm):
    """
    酒精暴露三段式波形：
    0~20%  : 基线稳定
    20~58% : 指数下降（气体快速吸附）
    58~100%: 指数恢复（气体缓慢扩散）
    Rs/R0最低值由手册曲线插值，叠加温湿度修正
    """
    th_factor = get_th_factor(temp, humi)
    base      = 1.0 * th_factor
    rs_r0_min = ppm_to_rs_r0(channel, ppm) * th_factor

    t_expose = int(T * 0.20)
    t_peak   = int(T * 0.58)

    signal = np.zeros(T)

    # 段1：基线
    signal[:t_expose] = base + np.random.randn(t_expose) * NOISE_STD[channel]

    # 段2：指数下降（tau=0.3，前期快后期慢）
    for i in range(t_expose, t_peak):
        progress = (i - t_expose) / (t_peak - t_expose)
        ratio = 1 - np.exp(-progress / 0.3)
        signal[i] = base + (rs_r0_min - base) * ratio

    # 段3：指数恢复（tau=0.5，比下降慢）
    for i in range(t_peak, T):
        progress = (i - t_peak) / (T - t_peak)
        ratio = 1 - np.exp(-progress / 0.5)
        signal[i] = rs_r0_min + (base - rs_r0_min) * ratio

    # 叠加噪声和个体差异
    signal += np.random.randn(T) * NOISE_STD[channel]
    signal *= (1.0 + np.random.randn() * 0.05)

    return np.clip(signal, 0.01, 3.0)


# ─────────────────────────────────────────────
# 批量生成
# ─────────────────────────────────────────────

def generate_dataset():
    all_samples = []
    all_labels  = []
    all_meta    = []

    print("生成正常空气样本...")
    for n in range(N_AIR):
        temp = np.random.normal(TEMP_MEAN, TEMP_STD)
        humi = float(np.clip(np.random.normal(HUMI_MEAN, HUMI_STD), 30, 85))
        sample = np.stack(
            [gen_air_sample(ch, temp, humi) for ch in CHANNELS],
            axis=1
        )
        all_samples.append(sample)
        all_labels.append(0)
        all_meta.append({'temp': round(temp,2), 'humi': round(humi,2),
                         'ppm': 0, 'label': 'air'})

    print("生成酒精暴露样本...")
    for n in range(N_ALCOHOL):
        temp = np.random.normal(TEMP_MEAN, TEMP_STD)
        humi = float(np.clip(np.random.normal(HUMI_MEAN, HUMI_STD), 30, 85))
        ppm  = float(np.random.uniform(*ALCOHOL_PPM_RANGE))
        sample = np.stack(
            [gen_alcohol_sample(ch, temp, humi, ppm) for ch in CHANNELS],
            axis=1
        )
        all_samples.append(sample)
        all_labels.append(1)
        all_meta.append({'temp': round(temp,2), 'humi': round(humi,2),
                         'ppm': round(ppm,1), 'label': 'alcohol'})

    X = np.array(all_samples)   # (600, 100, 4)
    y = np.array(all_labels)

    idx = np.random.permutation(len(y))
    X, y = X[idx], y[idx]
    meta_df = pd.DataFrame(all_meta).iloc[idx].reset_index(drop=True)

    # 保存（命名明确为仿真响应数据）
    X = X.astype(np.float32)
    y = y.astype(np.int64)
    np.save(os.path.join(OUTPUT_DIR, 'X_response_sim.npy'), X)
    np.save(os.path.join(OUTPUT_DIR, 'y_labels.npy'), y)
    meta_df.to_csv(os.path.join(OUTPUT_DIR, 'meta.csv'), index=False)

    # 分别保存空气和酒精的预览CSV（各取3条样本，便于对比查看）
    air_indices     = np.where(y == 0)[0][:3]
    alcohol_indices = np.where(y == 1)[0][:3]

    for csv_label, indices in [('air', air_indices), ('alcohol', alcohol_indices)]:
        frames = []
        for sample_no, idx in enumerate(indices):
            df = pd.DataFrame(X[idx], columns=CHANNELS)
            df.insert(0, 'time_step', range(T))
            df.insert(0, 'sample_no', sample_no)
            frames.append(df)
        out_df = pd.concat(frames, ignore_index=True)
        out_df.to_csv(
            os.path.join(OUTPUT_DIR, f'preview_{csv_label}.csv'),
            index=False
        )
        print(f"已保存 preview_{csv_label}.csv（{len(indices)}条样本预览）")

    # 打印统计
    print(f"\n数据集统计：")
    print(f"  总样本数:    {len(y)}")
    print(f"  空气样本:    {(y==0).sum()}")
    print(f"  酒精样本:    {(y==1).sum()}")
    print(f"  张量形状:    {X.shape}  (样本数, T=100时间步, 4通道)")
    print(f"  Rs/R0范围:   {X.min():.4f} ~ {X.max():.4f}")

    print(f"\n各传感器手册插值结果（仅供参考）：")
    for ppm_test in [100, 300, 1000]:
        print(f"  {ppm_test}ppm酒精 ->", end='')
        for ch in CHANNELS:
            v = ppm_to_rs_r0(ch, ppm_test)
            print(f"  {ch}={v:.4f}", end='')
        print()

    print(f"\n注意：MQ2/MQ5手册数据起点为300ppm")
    print(f"      100ppm以下为对数外推估计值，仅用于仿真")
    print(f"\n输出文件：")
    print(f"  X_response_sim.npy  -- 传感器响应仿真数据(Rs/R0域)")
    print(f"  y_labels.npy        -- 标签(0=空气, 1=酒精)")
    print(f"  meta.csv            -- 各样本温湿度/浓度记录")
    print(f"  preview_air.csv     -- 空气样本预览(3条)")
    print(f"  preview_alcohol.csv -- 酒精样本预览(3条)")

    return X, y


if __name__ == '__main__':
    generate_dataset()
