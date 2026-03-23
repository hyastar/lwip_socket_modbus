"""
04_TH_Compensation: 温湿度补偿
输入: Data/03_Normalization/X_normalized.npy   shape=(N, T, 4)
输出: Data/04_TH_Compensation/X_th_comp.npy   shape=(N, T, 4)

作用:
  修正环境温湿度变化对MQ传感器Rs/R0输出的影响
  基准: 25°C / 60%RH = 1.00
  补偿方法: 基于手册温湿度特性表做双线性插值

说明:
  补偿公式: Rs/R0_comp = Rs/R0_measured / TH_factor(T, RH)
  TH_factor > 1 表示当前环境下Rs/R0偏高，需要向下修正
  TH_factor < 1 表示当前环境下Rs/R0偏低，需要向上修正
  手册数据适用于 MQ-2/3/5/138 全系列（通用补偿曲线）
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import os
import platform

# ─────────────────────────────────────────────
# 中文字体配置
# ─────────────────────────────────────────────
def setup_chinese_font():
    if platform.system() == 'Windows':
        for path in ['C:/Windows/Fonts/msyh.ttc',
                     'C:/Windows/Fonts/simhei.ttf',
                     'C:/Windows/Fonts/simsun.ttc']:
            if os.path.exists(path):
                fm.fontManager.addfont(path)
                prop = fm.FontProperties(fname=path)
                plt.rcParams['font.family'] = prop.get_name()
                break
    plt.rcParams['axes.unicode_minus'] = False

def get_fp():
    if platform.system() == 'Windows':
        for path in ['C:/Windows/Fonts/msyh.ttc',
                     'C:/Windows/Fonts/simhei.ttf',
                     'C:/Windows/Fonts/simsun.ttc']:
            if os.path.exists(path):
                return fm.FontProperties(fname=path)
    return None

setup_chinese_font()

# ─────────────────────────────────────────────
# 手册温湿度补偿表（全系列通用，基准25°C/60%RH=1.00）
# ─────────────────────────────────────────────
TH_TABLE = {
    'temp': [-10, -5,  0,   5,   10,  15,  20,  25,
              30,  35,  40,  45,  50],
    '30rh': [1.71,1.63,1.59,1.50,1.43,1.30,1.25,1.18,
             1.15,1.11,1.01,0.93,0.87],
    '60rh': [1.45,1.38,1.35,1.27,1.22,1.11,1.05,1.00,
             0.98,0.94,0.86,0.79,0.73],
    '85rh': [1.26,1.20,1.17,1.10,1.05,0.96,0.92,0.88,
             0.86,0.82,0.73,0.68,0.64],
}
TEMP_ARRAY = np.array(TH_TABLE['temp'], dtype=float)
RH30_ARRAY = np.array(TH_TABLE['30rh'], dtype=float)
RH60_ARRAY = np.array(TH_TABLE['60rh'], dtype=float)
RH85_ARRAY = np.array(TH_TABLE['85rh'], dtype=float)

# ─────────────────────────────────────────────
# 配置
# ─────────────────────────────────────────────
CHANNELS  = ['mq2', 'mq3', 'mq5', 'mq138']
CH_LABELS = {'mq2': 'MQ-2', 'mq3': 'MQ-3',
             'mq5': 'MQ-5', 'mq138': 'MQ-138'}

DATA_DIR   = r'G:\Cursor\kato-modbus\Simulation\algorithm\Data'
INPUT_PATH = os.path.join(DATA_DIR, '03_Normalization', 'X_normalized.npy')
LABEL_PATH = os.path.join(DATA_DIR, '03_Normalization', 'y_labels.npy')
META_PATH  = os.path.join(DATA_DIR, '01_Raw', 'meta.csv')
OUTPUT_DIR = os.path.join(DATA_DIR, '04_TH_Compensation')
PHOTO_DIR  = r'G:\Cursor\kato-modbus\Simulation\algorithm\Photo\04_TH_Compensation'
os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(PHOTO_DIR,  exist_ok=True)

DPI      = 300
PNG_ARGS = dict(dpi=DPI, bbox_inches='tight', format='png')

COLORS       = ['#378ADD', '#D85A30', '#1D9E75', '#BA7517']
COLOR_BEFORE = '#B4B2A9'
COLOR_AFTER  = '#1D9E75'


# ─────────────────────────────────────────────
# 温湿度补偿系数计算（双线性插值）
# ─────────────────────────────────────────────

def get_th_factor(temp, humi):
    """
    由温度和湿度查询补偿系数（双线性插值）
    基准: 25°C / 60%RH = 1.00
    temp: 摄氏度  humi: %RH (30~85)
    """
    # 温度插值：在各湿度列上分别插值
    v30 = float(np.interp(temp, TEMP_ARRAY, RH30_ARRAY))
    v60 = float(np.interp(temp, TEMP_ARRAY, RH60_ARRAY))
    v85 = float(np.interp(temp, TEMP_ARRAY, RH85_ARRAY))

    # 湿度插值
    humi = float(np.clip(humi, 30, 85))
    if humi <= 60:
        factor = v30 + (v60 - v30) * (humi - 30) / 30.0
    else:
        factor = v60 + (v85 - v60) * (humi - 60) / 25.0

    return factor


def apply_th_compensation(X, meta_df):
    """
    对每条样本用对应的温湿度做补偿
    Rs/R0_comp = Rs/R0_measured / TH_factor(T, RH)
    """
    N, T, C = X.shape
    X_out   = X.copy()
    factors = np.zeros(N)

    for i in range(N):
        temp   = float(meta_df.iloc[i]['temp'])
        humi   = float(meta_df.iloc[i]['humi'])
        factor = get_th_factor(temp, humi)
        factors[i] = factor
        X_out[i]   = X[i] / factor

    return X_out, factors


# ─────────────────────────────────────────────
# 图1：TH补偿系数热力图
# 展示不同温湿度组合下的补偿系数分布
# ─────────────────────────────────────────────

def plot_th_heatmap():
    """
    图1：温湿度补偿系数热力图
    证明：温湿度对传感器输出影响显著，补偿有必要
    """
    fp = get_fp()

    temps = np.arange(-10, 52, 2)
    humis = np.arange(30, 87, 2)
    Z = np.zeros((len(humis), len(temps)))

    for i, h in enumerate(humis):
        for j, t in enumerate(temps):
            Z[i, j] = get_th_factor(t, h)

    fig, ax = plt.subplots(figsize=(14, 7))
    im = ax.contourf(temps, humis, Z,
                     levels=20, cmap='RdYlBu_r')
    cs = ax.contour(temps, humis, Z,
                    levels=10, colors='black',
                    linewidths=0.5, alpha=0.4)
    ax.clabel(cs, fmt='%.2f', fontsize=8)

    # 标注基准点
    ax.plot(25, 60, 'w*', markersize=14, label='基准点 (25°C, 60%RH, 系数=1.00)')
    # 标注图书馆典型环境区域
    ax.add_patch(plt.Rectangle((15, 40), 15, 35,
                                fill=False, edgecolor='white',
                                linewidth=2, linestyle='--'))
    ax.text(22.5, 77, '图书馆典型\n环境范围',
            ha='center', va='bottom', color='white',
            fontsize=10, fontproperties=fp)

    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('温湿度补偿系数 TH_factor', fontsize=12,
                   fontproperties=fp)

    ax.set_xlabel('温度 (°C)', fontsize=13, fontproperties=fp)
    ax.set_ylabel('相对湿度 (%RH)', fontsize=13, fontproperties=fp)
    ax.set_title('MQ传感器温湿度补偿系数分布（手册数据双线性插值）',
                 fontsize=14, fontproperties=fp)
    ax.legend(prop=fp, fontsize=10, loc='upper right')

    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '01_th_factor_heatmap.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图1] 温湿度系数热力图 -> {path}")


# ─────────────────────────────────────────────
# 图2：不同温湿度条件下响应曲线对比
# ─────────────────────────────────────────────

def plot_th_effect(X_in, X_out, y, meta_df):
    """
    图2：选取高温高湿/常温常湿/低温干燥三种样本
    对比补偿前后曲线，证明补偿有效消除温湿度影响
    """
    fp = get_fp()

    # 筛选三种典型环境的酒精样本
    alc_mask = (y == 1)
    alc_idx  = np.where(alc_mask)[0]

    # 按TH_factor分高/中/低
    factors_alc = []
    for i in alc_idx:
        t_ = float(meta_df.iloc[i]['temp'])
        h_ = float(meta_df.iloc[i]['humi'])
        factors_alc.append(get_th_factor(t_, h_))
    factors_alc = np.array(factors_alc)

    # 高系数（低温干燥）、中系数（常温常湿）、低系数（高温潮湿）
    high_i  = alc_idx[np.argmax(factors_alc)]
    low_i   = alc_idx[np.argmin(factors_alc)]
    mid_i   = alc_idx[np.argmin(np.abs(factors_alc - 1.0))]

    t_axis = np.arange(X_in.shape[1])

    for c, ch in enumerate(CHANNELS):
        fig, axes = plt.subplots(1, 2, figsize=(16, 6))

        labels_map = {
            high_i: f'低温干燥 ({meta_df.iloc[high_i]["temp"]:.1f}°C, '
                    f'{meta_df.iloc[high_i]["humi"]:.1f}%RH)',
            mid_i:  f'常温常湿 ({meta_df.iloc[mid_i]["temp"]:.1f}°C, '
                    f'{meta_df.iloc[mid_i]["humi"]:.1f}%RH)',
            low_i:  f'高温潮湿 ({meta_df.iloc[low_i]["temp"]:.1f}°C, '
                    f'{meta_df.iloc[low_i]["humi"]:.1f}%RH)',
        }
        colors_3 = ['#378ADD', '#D85A30', '#1D9E75']

        for ax, X_data, title in zip(
            axes,
            [X_in, X_out],
            ['温湿度补偿前', '温湿度补偿后']
        ):
            for color, (idx, lbl) in zip(
                colors_3, labels_map.items()
            ):
                ax.plot(t_axis, X_data[idx, :, c],
                        color=color, lw=2.0, label=lbl)

            ax.set_title(f'{CH_LABELS[ch]} — {title}',
                         fontsize=13, fontproperties=fp)
            ax.set_xlabel('时间步 (×0.1 s)', fontsize=11,
                          fontproperties=fp)
            ax.set_ylabel('Rs/R0', fontsize=11, fontproperties=fp)
            ax.legend(prop=fp, fontsize=9, loc='upper right')
            ax.grid(True, alpha=0.3)

        plt.suptitle(
            f'{CH_LABELS[ch]}  不同温湿度条件下补偿前后对比（酒精暴露样本）',
            fontsize=13, fontproperties=fp)
        plt.tight_layout()
        path = os.path.join(PHOTO_DIR, f'02_{ch}_th_compare.png')
        plt.savefig(path, **PNG_ARGS)
        plt.close()
        print(f"  [图2-{ch}] 温湿度条件对比 -> {path}")


# ─────────────────────────────────────────────
# 图3：补偿系数分布直方图
# ─────────────────────────────────────────────

def plot_factor_distribution(factors, y):
    """
    图3：数据集中TH补偿系数分布
    证明：样本覆盖了多种温湿度条件，补偿是必要的
    """
    fp = get_fp()

    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    for ax, mask, label, color in zip(
        axes,
        [y == 0, y == 1],
        ['正常空气样本', '酒精暴露样本'],
        ['#378ADD', '#D85A30']
    ):
        f = factors[mask]
        ax.hist(f, bins=25, color=color, alpha=0.8,
                edgecolor='white', linewidth=0.5)
        ax.axvline(x=1.0, color='red', lw=1.5,
                   linestyle='--', label='基准系数=1.00')
        ax.axvline(x=f.mean(), color='black', lw=1.5,
                   linestyle='-',
                   label=f'均值={f.mean():.3f}')
        ax.set_title(f'TH补偿系数分布 — {label}',
                     fontsize=13, fontproperties=fp)
        ax.set_xlabel('补偿系数 TH_factor', fontsize=12,
                      fontproperties=fp)
        ax.set_ylabel('样本数量', fontsize=12, fontproperties=fp)
        ax.legend(prop=fp, fontsize=10)
        ax.grid(True, alpha=0.3)
        ax.text(0.98, 0.95,
                f'范围: {f.min():.3f} ~ {f.max():.3f}\n'
                f'标准差: {f.std():.3f}',
                transform=ax.transAxes,
                ha='right', va='top', fontsize=10,
                fontproperties=fp,
                bbox=dict(boxstyle='round',
                          facecolor='#E6F1FB', alpha=0.8))

    plt.suptitle('数据集温湿度补偿系数分布',
                 fontsize=14, fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '03_factor_distribution.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图3] 补偿系数分布 -> {path}")


# ─────────────────────────────────────────────
# 统计报告
# ─────────────────────────────────────────────

def print_stats(X_in, X_out, y, factors):
    print(f"\n{'='*56}")
    print(f"  温湿度补偿效果统计")
    print(f"{'='*56}")
    print(f"  补偿系数范围: {factors.min():.4f} ~ "
          f"{factors.max():.4f}  均值={factors.mean():.4f}")
    print(f"  系数>1 样本数（偏高需向下修正）: "
          f"{(factors > 1.0).sum()}")
    print(f"  系数<1 样本数（偏低需向上修正）: "
          f"{(factors < 1.0).sum()}")

    print(f"\n  各通道补偿前后基线对比（酒精样本）:")
    alc = np.where(y == 1)[0]
    for c, ch in enumerate(CHANNELS):
        before = X_in[alc, :20, c].mean(axis=1).std()
        after  = X_out[alc, :20, c].mean(axis=1).std()
        print(f"  {CH_LABELS[ch]:8s}  "
              f"跨样本基线std: 前={before:.4f}  后={after:.4f}")

    print(f"\n  输入张量: {X_in.shape}")
    print(f"  输出张量: {X_out.shape}")
    print(f"  图片分辨率: {DPI} DPI")
    print(f"{'='*56}")


# ─────────────────────────────────────────────
# 主流程
# ─────────────────────────────────────────────

def main():
    print("=" * 56)
    print("  04_TH_Compensation: 温湿度补偿")
    print("=" * 56)

    print(f"\n[1/5] 读取数据...")
    X_in   = np.load(INPUT_PATH)
    y      = np.load(LABEL_PATH)
    meta   = pd.read_csv(META_PATH)
    print(f"  输入: {X_in.shape}  标签: {y.shape}")
    print(f"  温度范围: {meta['temp'].min():.1f}~"
          f"{meta['temp'].max():.1f}°C  "
          f"湿度范围: {meta['humi'].min():.1f}~"
          f"{meta['humi'].max():.1f}%RH")

    print(f"\n[2/5] 执行温湿度补偿...")
    X_out, factors = apply_th_compensation(X_in, meta)
    print(f"  完成，补偿系数范围: "
          f"{factors.min():.4f} ~ {factors.max():.4f}")

    print(f"\n[3/5] 保存结果...")
    X_out = X_out.astype(np.float32)
    y = y.astype(np.int64)
    np.save(os.path.join(OUTPUT_DIR, 'X_th_comp.npy'),  X_out)
    np.save(os.path.join(OUTPUT_DIR, 'y_labels.npy'),   y)
    np.save(os.path.join(OUTPUT_DIR, 'th_factors.npy'), factors)
    print(f"  已保存 X_th_comp.npy  y_labels.npy  th_factors.npy")

    print(f"\n[4/5] 生成可视化图片 ({DPI} DPI)...")
    plot_th_heatmap()
    plot_th_effect(X_in, X_out, y, meta)
    plot_factor_distribution(factors, y)

    print(f"\n[5/5] 统计报告...")
    print_stats(X_in, X_out, y, factors)
    print(f"\n完成！图片保存在: {PHOTO_DIR}")


if __name__ == '__main__':
    main()
