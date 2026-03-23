"""
05_Kalman_Slow: 一阶低速卡尔曼滤波（全通道统一处理）
输入: Data/04_TH_Compensation/X_th_comp.npy   shape=(N, T, 4)
输出: Data/05_Kalman_Slow/X_kf_slow.npy       shape=(N, T, 4)

作用:
  对温湿度补偿后的序列做平滑处理
  稳定送入CNN的输入窗口，降低误分类概率
  同时抑制补偿模型不完美带来的低频残余波动

与高速KF的区别:
  高速KF: Q/R比值大 -> 响应快，平滑弱，保留动态特征
  低速KF: Q/R比值小 -> 响应慢，平滑强，稳定输入序列
"""

import numpy as np
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
# 配置
# ─────────────────────────────────────────────
CHANNELS  = ['mq2', 'mq3', 'mq5', 'mq138']
CH_LABELS = {'mq2': 'MQ-2', 'mq3': 'MQ-3',
             'mq5': 'MQ-5', 'mq138': 'MQ-138'}

# 低速卡尔曼参数：Q/R比值小 -> 平滑强，响应慢
# 对比高速KF (Q=0.01, R=0.10, 比值=0.10)
# 低速KF: Q/R比值更小，响应更慢，平滑作用更强
KF_SLOW_PARAMS = {
    'mq2':   {'q': 0.001, 'r': 0.50},
    'mq3':   {'q': 0.001, 'r': 0.50},
    'mq5':   {'q': 0.001, 'r': 0.50},
    'mq138': {'q': 0.001, 'r': 0.50},
}

# 高速KF参数（用于对比图）
KF_FAST_PARAMS = {
    'mq2':   {'q': 0.01, 'r': 0.10},
    'mq3':   {'q': 0.01, 'r': 0.10},
    'mq5':   {'q': 0.01, 'r': 0.10},
    'mq138': {'q': 0.01, 'r': 0.10},
}

DATA_DIR   = r'G:\Cursor\kato-modbus\Simulation\algorithm\Data'
INPUT_PATH = os.path.join(DATA_DIR, '04_TH_Compensation', 'X_th_comp.npy')
LABEL_PATH = os.path.join(DATA_DIR, '04_TH_Compensation', 'y_labels.npy')
# 同时加载高速KF结果用于三路对比
RAW_PATH   = os.path.join(DATA_DIR, '01_Raw', 'X_response_sim.npy')
FAST_PATH  = os.path.join(DATA_DIR, '02_Kalman_Fast', 'X_kf_fast.npy')
OUTPUT_DIR = os.path.join(DATA_DIR, '05_Kalman_Slow')
PHOTO_DIR  = r'G:\Cursor\kato-modbus\Simulation\algorithm\Photo\05_Kalman_Slow'
os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(PHOTO_DIR,  exist_ok=True)

DPI      = 300
PNG_ARGS = dict(dpi=DPI, bbox_inches='tight', format='png')

COLOR_RAW    = '#B4B2A9'
COLOR_INPUT  = '#378ADD'
COLOR_SLOW   = '#1D9E75'
COLOR_ACCENT = '#D85A30'


# ─────────────────────────────────────────────
# 卡尔曼滤波核心（与02共用相同实现）
# ─────────────────────────────────────────────

def kalman_filter_1d(signal, q, r):
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
# 图1：高速KF vs 低速KF 参数对比示意
# 用同一段信号展示Q/R比值不同的效果差异
# ─────────────────────────────────────────────

def plot_fast_vs_slow_params(X_in, y):
    """
    图1：同一通道同一样本，高速KF vs 低速KF效果对比
    直观展示两级KF的参数差异和各自职责
    """
    fp = get_fp()
    alcohol_idx = np.where(y == 1)[0][0]
    t = np.arange(X_in.shape[1])

    fig, axes = plt.subplots(2, 2, figsize=(16, 10))
    axes = axes.flatten()

    for idx_ax, (c, ch) in enumerate(
        [(1, 'mq3'), (3, 'mq138'), (0, 'mq2'), (2, 'mq5')]
    ):
        ax  = axes[idx_ax]
        sig = X_in[alcohol_idx, :, c]

        kf_fast = kalman_filter_1d(
            sig,
            KF_FAST_PARAMS[ch]['q'],
            KF_FAST_PARAMS[ch]['r']
        )
        kf_slow = kalman_filter_1d(
            sig,
            KF_SLOW_PARAMS[ch]['q'],
            KF_SLOW_PARAMS[ch]['r']
        )

        ax.plot(t, sig,     color=COLOR_RAW,   lw=1.2,
                label='补偿后输入', alpha=0.8)
        ax.plot(t, kf_fast, color=COLOR_INPUT, lw=1.8,
                label=f'高速KF (Q={KF_FAST_PARAMS[ch]["q"]}, '
                      f'R={KF_FAST_PARAMS[ch]["r"]})')
        ax.plot(t, kf_slow, color=COLOR_SLOW,  lw=2.2,
                label=f'低速KF (Q={KF_SLOW_PARAMS[ch]["q"]}, '
                      f'R={KF_SLOW_PARAMS[ch]["r"]})')

        ax.set_title(f'{CH_LABELS[ch]} — 双速卡尔曼对比（酒精暴露）',
                     fontsize=12, fontproperties=fp)
        ax.set_xlabel('时间步 (×0.1 s)', fontsize=10,
                      fontproperties=fp)
        ax.set_ylabel('Rs/R0', fontsize=10, fontproperties=fp)
        ax.legend(prop=fp, fontsize=9, loc='upper right')
        ax.grid(True, alpha=0.3)

    plt.suptitle('不同Q/R参数下卡尔曼滤波输出效果对比（四通道）',
                 fontsize=14, fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '01_fast_vs_slow_kf.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图1] 双速KF对比 -> {path}")


# ─────────────────────────────────────────────
# 图2：低速KF前后对比（每通道，空气+酒精）
# ─────────────────────────────────────────────

def plot_slow_kf_comparison(X_in, X_out, y):
    """
    图2：低速KF前后对比，每通道一张
    证明：补偿后残余波动得到平滑，序列更稳定
    """
    fp = get_fp()
    alcohol_idx = np.where(y == 1)[0][0]
    air_idx     = np.where(y == 0)[0][0]
    t = np.arange(X_in.shape[1])

    for c, ch in enumerate(CHANNELS):
        q = KF_SLOW_PARAMS[ch]['q']
        r = KF_SLOW_PARAMS[ch]['r']
        fig, axes = plt.subplots(2, 1, figsize=(12, 9))

        for ax, idx, scene in zip(
            axes,
            [alcohol_idx, air_idx],
            ['酒精暴露样本', '正常空气样本']
        ):
            before = X_in[idx,  :, c]
            after  = X_out[idx, :, c]

            ax.plot(t, before, color=COLOR_INPUT, lw=1.5,
                    label='低速KF输入（温湿度补偿后）', alpha=0.85)
            ax.plot(t, after,  color=COLOR_SLOW,  lw=2.2,
                    label=f'低速KF输出 (Q={q}, R={r})')

            # 波动量化：用整段信号的局部标准差
            window = 5
            std_before = np.array([
                before[max(0,i-window):i+window].std()
                for i in range(len(before))
            ]).mean()
            std_after = np.array([
                after[max(0,i-window):i+window].std()
                for i in range(len(after))
            ]).mean()
            reduc = (std_before - std_after) / std_before * 100 \
                    if std_before > 1e-8 else 0.0

            ax.text(0.02, 0.96,
                    f'局部波动均值：输入 = {std_before:.4f}    '
                    f'低速KF后 = {std_after:.4f}    '
                    f'平滑率 = {reduc:.1f}%',
                    transform=ax.transAxes, fontsize=10,
                    va='top', fontproperties=fp,
                    bbox=dict(boxstyle='round',
                              facecolor='#E1F5EE', alpha=0.8))

            ax.set_title(f'{CH_LABELS[ch]} — {scene}',
                         fontsize=13, fontproperties=fp)
            ax.set_xlabel('时间步 (×0.1 s)', fontsize=11,
                          fontproperties=fp)
            ax.set_ylabel('Rs/R0', fontsize=11, fontproperties=fp)
            ax.legend(prop=fp, fontsize=10, loc='upper right')
            ax.grid(True, alpha=0.3)

        plt.suptitle(f'{CH_LABELS[ch]}  低速卡尔曼滤波前后对比',
                     fontsize=14, fontproperties=fp)
        plt.tight_layout()
        path = os.path.join(PHOTO_DIR, f'02_{ch}_kf_slow_compare.png')
        plt.savefig(path, **PNG_ARGS)
        plt.close()
        print(f"  [图2-{ch}] 低速KF对比 -> {path}")


# ─────────────────────────────────────────────
# 图3：完整算法链效果总览
# 原始 -> 高速KF -> 归一化+TH补偿 -> 低速KF
# ─────────────────────────────────────────────

def plot_full_pipeline(X_raw, X_fast, X_in, X_out, y):
    """
    图3：完整预处理算法链四阶段对比
    MQ3通道（最灵敏），展示酒精暴露样本
    这张图是论文方法章节最重要的总图
    """
    fp = get_fp()
    alcohol_idx = np.where(y == 1)[0][0]
    air_idx     = np.where(y == 0)[0][0]
    t = np.arange(X_raw.shape[1])
    c = 1   # MQ3，最灵敏通道

    for idx, scene in [(alcohol_idx, '酒精暴露'), (air_idx, '正常空气')]:
        fig, axes = plt.subplots(4, 1, figsize=(13, 16))

        # 使用已保存的高速KF结果作为第二阶段
        kf_fast_sig = X_fast[idx, :, c]

        all_stages = [
            (X_raw[idx, :, c],  '① 原始仿真信号',          COLOR_RAW),
            (kf_fast_sig,        '② 高速卡尔曼滤波后',      COLOR_INPUT),
            (X_in[idx,  :, c],  '③ 归一化 + 温湿度补偿后', '#BA7517'),
            (X_out[idx, :, c],  '④ 低速卡尔曼（CNN输入）', COLOR_SLOW),
        ]

        for ax, (sig, title, color) in zip(axes, all_stages):
            ax.plot(t, sig, color=color, lw=2.0)
            ax.set_title(title, fontsize=12, fontproperties=fp)
            ax.set_ylabel('Rs/R0', fontsize=10, fontproperties=fp)
            ax.grid(True, alpha=0.3)
            ax.set_xlim(0, len(t)-1)

        axes[-1].set_xlabel('时间步 (×0.1 s)', fontsize=11,
                            fontproperties=fp)
        plt.suptitle(
            f'MQ-3  完整预处理算法链关键阶段总览（{scene}样本）\n'
            f'注：归一化与温湿度补偿阶段合并为第③阶段展示',
            fontsize=13, fontproperties=fp)
        plt.tight_layout()
        fname = f'03_full_pipeline_mq3_{scene[:2]}.png'
        path  = os.path.join(PHOTO_DIR, fname)
        plt.savefig(path, **PNG_ARGS)
        plt.close()
        print(f"  [图3-{scene}] 完整算法链 -> {path}")


# ─────────────────────────────────────────────
# 图4：平滑效果柱状图
# ─────────────────────────────────────────────

def plot_smoothness_bar(X_in, X_out):
    """
    图4：四通道平滑效果量化柱状图
    """
    fp = get_fp()
    window = 5

    def local_std_mean(X, c):
        vals = []
        for i in range(X.shape[0]):
            sig = X[i, :, c]
            s = np.array([
                sig[max(0, j-window):j+window].std()
                for j in range(len(sig))
            ]).mean()
            vals.append(s)
        return np.mean(vals)

    before_vals = [local_std_mean(X_in,  c) for c in range(4)]
    after_vals  = [local_std_mean(X_out, c) for c in range(4)]

    x      = np.arange(len(CHANNELS))
    width  = 0.35
    labels = [CH_LABELS[ch] for ch in CHANNELS]

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(x - width/2, before_vals, width,
           label='低速KF输入', color=COLOR_INPUT,
           edgecolor='#185FA5', linewidth=0.8, alpha=0.85)
    ax.bar(x + width/2, after_vals,  width,
           label='低速KF输出', color=COLOR_SLOW,
           edgecolor='#085041', linewidth=0.8, alpha=0.85)

    for i, (b, a) in enumerate(zip(before_vals, after_vals)):
        reduc = (b - a) / b * 100 if b > 1e-8 else 0.0
        ax.text(x[i], max(b, a) * 1.03,
                f'↓{reduc:.1f}%',
                ha='center', va='bottom', fontsize=11,
                color=COLOR_ACCENT, fontproperties=fp,
                fontweight='bold')

    ax.set_xlabel('传感器通道', fontsize=13, fontproperties=fp)
    ax.set_ylabel('局部波动均值（局部标准差）', fontsize=12,
                  fontproperties=fp)
    ax.set_title('低速卡尔曼滤波平滑效果（四通道汇总）',
                 fontsize=14, fontproperties=fp)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontproperties=fp, fontsize=12)
    ax.legend(prop=fp, fontsize=11)
    ax.grid(True, axis='y', alpha=0.3)
    ax.set_ylim(0, max(before_vals) * 1.3)

    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '04_smoothness_bar.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图4] 平滑效果柱状图 -> {path}")


# ─────────────────────────────────────────────
# 统计报告
# ─────────────────────────────────────────────

def print_stats(X_in, X_out):
    print(f"\n{'='*56}")
    print(f"  低速卡尔曼滤波效果统计")
    print(f"  平滑性指标: 长度为10的局部窗口标准差均值")
    print(f"  对比参数 — 高速KF: Q=0.01/R=0.10  低速KF: Q=0.001/R=0.50")
    print(f"{'='*56}")
    window = 5
    for c, ch in enumerate(CHANNELS):
        vals_b, vals_a = [], []
        for i in range(X_in.shape[0]):
            for X_, vals in [(X_in, vals_b), (X_out, vals_a)]:
                sig = X_[i, :, c]
                s = np.array([
                    sig[max(0,j-window):j+window].std()
                    for j in range(len(sig))
                ]).mean()
                vals.append(s)
        mb = np.mean(vals_b)
        ma = np.mean(vals_a)
        rd = (mb - ma) / mb * 100 if mb > 1e-8 else 0.0
        print(f"  {CH_LABELS[ch]:8s}  "
              f"局部波动: 前={mb:.5f}  后={ma:.5f}  "
              f"平滑率={rd:.1f}%")
    print(f"\n  输入张量: {X_in.shape}")
    print(f"  输出张量: {X_out.shape}")
    print(f"  图片分辨率: {DPI} DPI")
    print(f"{'='*56}")


# ─────────────────────────────────────────────
# 主流程
# ─────────────────────────────────────────────

def main():
    print("=" * 56)
    print("  05_Kalman_Slow: 一阶低速卡尔曼滤波")
    print("=" * 56)

    print(f"\n[1/6] 读取数据...")
    X_in  = np.load(INPUT_PATH)
    y     = np.load(LABEL_PATH)
    X_raw  = np.load(RAW_PATH)
    X_fast = np.load(FAST_PATH)
    print(f"  输入: {X_in.shape}  标签: {y.shape}")

    print(f"\n[2/6] 执行低速卡尔曼滤波...")
    X_out = apply_kf_all_channels(X_in, KF_SLOW_PARAMS)

    print(f"\n[3/6] 保存结果...")
    X_out = X_out.astype(np.float32)
    y = y.astype(np.int64)
    np.save(os.path.join(OUTPUT_DIR, 'X_kf_slow.npy'), X_out)
    np.save(os.path.join(OUTPUT_DIR, 'y_labels.npy'),  y)
    print(f"  已保存 X_kf_slow.npy  y_labels.npy")

    print(f"\n[4/6] 生成可视化图片 ({DPI} DPI)...")
    plot_fast_vs_slow_params(X_in, y)
    plot_slow_kf_comparison(X_in, X_out, y)
    plot_full_pipeline(X_raw, X_fast, X_in, X_out, y)
    plot_smoothness_bar(X_in, X_out)

    print(f"\n[5/6] 统计报告...")
    print_stats(X_in, X_out)

    print(f"\n[6/6] 完成！")
    print(f"  图片保存在: {PHOTO_DIR}")
    print(f"  X_kf_slow.npy 即为 06_1DCNN 的输入数据")


if __name__ == '__main__':
    main()
