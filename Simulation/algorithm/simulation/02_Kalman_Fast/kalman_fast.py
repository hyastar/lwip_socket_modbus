"""
02_Kalman_Fast: 一阶高速卡尔曼滤波（全通道统一处理）
输入: Data/01_Raw/X_response_sim.npy   shape=(N, T, 4)
输出: Data/02_Kalman_Fast/X_kf_fast.npy  shape=(N, T, 4)
作用: 去除瞬时采样噪声，保留气体响应上升沿动态
参数: Q/R比值大 -> 响应快，平滑弱
说明: 当前处理的是仿真Rs/R0响应序列，非ADC原始电压
      真实部署时高速KF应作用于电压域，此处仿真直接在响应域操作
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import os
import platform

# ─────────────────────────────────────────────
# 中文字体配置（解决乱码）
# ─────────────────────────────────────────────
def setup_chinese_font():
    system = platform.system()
    if system == 'Windows':
        font_candidates = [
            'C:/Windows/Fonts/msyh.ttc',    # 微软雅黑
            'C:/Windows/Fonts/simhei.ttf',  # 黑体
            'C:/Windows/Fonts/simsun.ttc',  # 宋体
        ]
        for font_path in font_candidates:
            if os.path.exists(font_path):
                font_prop = fm.FontProperties(fname=font_path)
                plt.rcParams['font.family'] = font_prop.get_name()
                fm.fontManager.addfont(font_path)
                break
    plt.rcParams['axes.unicode_minus'] = False
    return plt.rcParams['font.family']

FONT_FAMILY = setup_chinese_font()

def get_font_prop():
    """获取中文字体属性对象，用于需要fontproperties参数的场合"""
    system = platform.system()
    if system == 'Windows':
        for path in ['C:/Windows/Fonts/msyh.ttc',
                     'C:/Windows/Fonts/simhei.ttf',
                     'C:/Windows/Fonts/simsun.ttc']:
            if os.path.exists(path):
                return fm.FontProperties(fname=path)
    return None

# ─────────────────────────────────────────────
# 配置
# ─────────────────────────────────────────────
CHANNELS = ['mq2', 'mq3', 'mq5', 'mq138']
CH_LABELS = {
    'mq2':   'MQ-2',
    'mq3':   'MQ-3',
    'mq5':   'MQ-5',
    'mq138': 'MQ-138',
}

# 高速卡尔曼参数：Q/R比值大 -> 跟踪快，平滑弱
KF_PARAMS = {
    'mq2':   {'q': 0.01, 'r': 0.10},
    'mq3':   {'q': 0.01, 'r': 0.10},
    'mq5':   {'q': 0.01, 'r': 0.10},
    'mq138': {'q': 0.01, 'r': 0.10},
}

DATA_DIR   = r'G:\Cursor\kato-modbus\Simulation\algorithm\Data'
INPUT_PATH = os.path.join(DATA_DIR, '01_Raw', 'X_response_sim.npy')
LABEL_PATH = os.path.join(DATA_DIR, '01_Raw', 'y_labels.npy')
OUTPUT_DIR = os.path.join(DATA_DIR, '02_Kalman_Fast')
PHOTO_DIR  = r'G:\Cursor\kato-modbus\Simulation\algorithm\Photo\02_Kalman_Fast'
os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(PHOTO_DIR,  exist_ok=True)

# 高清PNG参数
DPI      = 300
PNG_ARGS = dict(dpi=DPI, bbox_inches='tight', format='png')

# 配色
COLOR_RAW    = '#B4B2A9'
COLOR_KF     = '#378ADD'
COLOR_ACCENT = '#D85A30'


# ─────────────────────────────────────────────
# 一阶卡尔曼滤波核心
# ─────────────────────────────────────────────

def kalman_filter_1d(signal, q, r):
    """
    一阶标量卡尔曼滤波
    状态方程: x_k = x_{k-1}
    观测方程: z_k = x_k + noise
    q: 过程噪声方差
    r: 观测噪声方差
    高速KF: q/r比值大，增益K偏大，跟随测量快
    """
    n   = len(signal)
    x   = signal[0]
    p   = 1.0
    out = np.zeros(n)
    for k in range(n):
        p      = p + q
        K      = p / (p + r)
        x      = x + K * (signal[k] - x)
        p      = (1 - K) * p
        out[k] = x
    return out


def apply_kf_all_channels(X, kf_params):
    N, T, C = X.shape
    X_out   = X.copy()
    for c, ch in enumerate(CHANNELS):
        q = kf_params[ch]['q']
        r = kf_params[ch]['r']
        for i in range(N):
            X_out[i, :, c] = kalman_filter_1d(X[i, :, c], q, r)
        print(f"  [{ch}] 完成  Q={q}  R={r}")
    return X_out


# ─────────────────────────────────────────────
# 图1：四通道原始信号对比
# 空气 vs 酒精，一图看出阵列特征差异
# ─────────────────────────────────────────────

def plot_raw_four_channels(X_raw, y):
    """
    图1：原始四通道曲线对比（空气 vs 酒精）
    证明：不同传感器对酒精灵敏度差异构成多维特征基础
    """
    fp = get_font_prop()

    alcohol_idx = np.where(y == 1)[0][0]
    air_idx     = np.where(y == 0)[0][0]

    colors = ['#378ADD', '#D85A30', '#1D9E75', '#BA7517']
    t = np.arange(X_raw.shape[1])

    fig, axes = plt.subplots(1, 2, figsize=(16, 6))

    for ax, idx, scene in zip(axes,
                               [air_idx, alcohol_idx],
                               ['正常空气', '酒精暴露']):
        for c, ch in enumerate(CHANNELS):
            ax.plot(t, X_raw[idx, :, c],
                    color=colors[c], lw=1.8,
                    label=CH_LABELS[ch])
        ax.set_title(f'原始传感器响应 — {scene}',
                     fontsize=14, fontproperties=fp)
        ax.set_xlabel('时间步 (×0.1 s)', fontsize=12, fontproperties=fp)
        ax.set_ylabel('Rs/R0', fontsize=12, fontproperties=fp)
        ax.legend(prop=fp, fontsize=11)
        ax.grid(True, alpha=0.3)
        ax.set_ylim(0, 1.6)

    plt.suptitle('MQ传感器阵列原始响应曲线对比', fontsize=15,
                 fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '01_raw_four_channels.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图1] 原始四通道对比 -> {path}")


# ─────────────────────────────────────────────
# 图2：高速KF前后对比（每通道一张，共4张）
# ─────────────────────────────────────────────

def plot_kf_comparison(X_raw, X_kf, y):
    """
    图2：每个传感器通道的KF前后对比
    上：酒精暴露样本  下：正常空气样本
    证明：去掉毛刺，不破坏响应上升沿
    """
    fp = get_font_prop()
    alcohol_idx = np.where(y == 1)[0][0]
    air_idx     = np.where(y == 0)[0][0]
    t = np.arange(X_raw.shape[1])

    for c, ch in enumerate(CHANNELS):
        q = KF_PARAMS[ch]['q']
        r = KF_PARAMS[ch]['r']
        fig, axes = plt.subplots(2, 1, figsize=(12, 9))

        for ax, idx, scene in zip(axes,
                                   [alcohol_idx, air_idx],
                                   ['酒精暴露样本', '正常空气样本']):
            raw = X_raw[idx, :, c]
            kf  = X_kf[idx,  :, c]

            ax.plot(t, raw, color=COLOR_RAW, lw=1.2,
                    label='原始信号', alpha=0.9, zorder=1)
            ax.plot(t, kf,  color=COLOR_KF,  lw=2.2,
                    label=f'高速卡尔曼滤波后 (Q={q}, R={r})', zorder=2)

            # 基线噪声统计
            n_raw = np.std(raw[:20])
            n_kf  = np.std(kf[:20])
            reduc = (n_raw - n_kf) / n_raw * 100 if n_raw > 1e-8 else 0.0

            ax.text(0.02, 0.96,
                    f'基线噪声：原始 = {n_raw:.4f}    '
                    f'滤波后 = {n_kf:.4f}    '
                    f'噪声抑制率 = {reduc:.1f}%',
                    transform=ax.transAxes, fontsize=10,
                    va='top', fontproperties=fp,
                    bbox=dict(boxstyle='round',
                              facecolor='#FAEEDA', alpha=0.8))

            ax.set_title(f'{CH_LABELS[ch]} — {scene}',
                         fontsize=13, fontproperties=fp)
            ax.set_xlabel('时间步 (×0.1 s)', fontsize=11,
                          fontproperties=fp)
            ax.set_ylabel('Rs/R0', fontsize=11, fontproperties=fp)
            ax.legend(prop=fp, fontsize=10, loc='upper right')
            ax.grid(True, alpha=0.3)

        plt.suptitle(f'{CH_LABELS[ch]}  高速卡尔曼滤波前后对比',
                     fontsize=14, fontproperties=fp)
        plt.tight_layout()
        path = os.path.join(PHOTO_DIR, f'02_{ch}_kf_compare.png')
        plt.savefig(path, **PNG_ARGS)
        plt.close()
        print(f"  [图2-{ch}] KF前后对比 -> {path}")


# ─────────────────────────────────────────────
# 图3：噪声抑制效果柱状图（全通道汇总）
# ─────────────────────────────────────────────

def plot_noise_bar(X_raw, X_kf):
    """
    图3：四通道噪声抑制效果柱状图
    证明：高速KF对所有通道均有效降噪
    """
    fp = get_font_prop()

    raw_noise = []
    kf_noise  = []
    for c in range(len(CHANNELS)):
        raw_noise.append(X_raw[:, :20, c].std(axis=1).mean())
        kf_noise.append( X_kf[:,  :20, c].std(axis=1).mean())

    x     = np.arange(len(CHANNELS))
    width = 0.35
    labels = [CH_LABELS[ch] for ch in CHANNELS]

    fig, ax = plt.subplots(figsize=(10, 6))
    bars1 = ax.bar(x - width/2, raw_noise, width,
                   label='原始信号噪声', color=COLOR_RAW,
                   edgecolor='gray', linewidth=0.8)
    bars2 = ax.bar(x + width/2, kf_noise,  width,
                   label='高速KF滤波后噪声', color=COLOR_KF,
                   edgecolor='#185FA5', linewidth=0.8)

    # 标注抑制率
    for i, (r, k) in enumerate(zip(raw_noise, kf_noise)):
        reduc = (r - k) / r * 100 if r > 1e-8 else 0.0
        ax.text(x[i], max(r, k) + 0.0003,
                f'↓{reduc:.1f}%',
                ha='center', va='bottom', fontsize=11,
                color=COLOR_ACCENT, fontproperties=fp,
                fontweight='bold')

    ax.set_xlabel('传感器通道', fontsize=13, fontproperties=fp)
    ax.set_ylabel('基线段噪声标准差', fontsize=13, fontproperties=fp)
    ax.set_title('高速卡尔曼滤波噪声抑制效果（四通道汇总）',
                 fontsize=14, fontproperties=fp)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontproperties=fp, fontsize=12)
    ax.legend(prop=fp, fontsize=11)
    ax.grid(True, axis='y', alpha=0.3)
    ax.set_ylim(0, max(raw_noise) * 1.3)

    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '03_noise_reduction_bar.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图3] 噪声抑制柱状图 -> {path}")


# ─────────────────────────────────────────────
# 统计报告
# ─────────────────────────────────────────────

def print_stats(X_raw, X_kf):
    print(f"\n{'='*56}")
    print(f"  高速卡尔曼滤波效果统计")
    print(f"{'='*56}")
    for c, ch in enumerate(CHANNELS):
        rn = X_raw[:, :20, c].std(axis=1).mean()
        kn = X_kf[:,  :20, c].std(axis=1).mean()
        rd = (rn - kn) / rn * 100 if rn > 1e-8 else 0.0
        print(f"  {CH_LABELS[ch]:8s}  "
              f"原始={rn:.5f}  KF后={kn:.5f}  "
              f"抑制率={rd:.1f}%")
    print(f"\n  输入张量: {X_raw.shape}")
    print(f"  输出张量: {X_kf.shape}")
    print(f"  图片分辨率: {DPI} DPI")
    print(f"{'='*56}")


# ─────────────────────────────────────────────
# 主流程
# ─────────────────────────────────────────────

def main():
    print("=" * 56)
    print("  02_Kalman_Fast: 一阶高速卡尔曼滤波")
    print("=" * 56)

    print(f"\n[1/5] 读取数据...")
    X_raw = np.load(INPUT_PATH)
    y     = np.load(LABEL_PATH)
    print(f"  输入: {X_raw.shape}  标签: {y.shape}")
    print(f"  空气={( y==0).sum()}  酒精={(y==1).sum()}")

    print(f"\n[2/5] 执行高速卡尔曼滤波...")
    X_kf = apply_kf_all_channels(X_raw, KF_PARAMS)

    print(f"\n[3/5] 保存结果...")
    X_kf = X_kf.astype(np.float32)
    y = y.astype(np.int64)
    np.save(os.path.join(OUTPUT_DIR, 'X_kf_fast.npy'), X_kf)
    np.save(os.path.join(OUTPUT_DIR, 'y_labels.npy'),  y)
    print(f"  已保存 X_kf_fast.npy  y_labels.npy")

    print(f"\n[4/5] 生成可视化图片 ({DPI} DPI)...")
    plot_raw_four_channels(X_raw, y)
    plot_kf_comparison(X_raw, X_kf, y)
    plot_noise_bar(X_raw, X_kf)

    print(f"\n[5/5] 统计报告...")
    print_stats(X_raw, X_kf)
    print(f"\n完成！图片保存在: {PHOTO_DIR}")


if __name__ == '__main__':
    main()
