"""
03_Normalization: Rs/R0 基线归一化
输入: Data/02_Kalman_Fast/X_kf_fast.npy   shape=(N, T, 4)
输出: Data/03_Normalization/X_normalized.npy  shape=(N, T, 4)

作用:
  消除传感器个体差异、零点偏移、批次差异
  使不同通道/不同传感器基线统一对齐到 1.0 附近
  对应真实部署中: Vout -> Rs -> Rs/R0 的物理量转换步骤

说明:
  仿真数据已在Rs/R0域，此步用每条样本暴露前稳定段
  均值作为该样本的局部基线R0_local，做二次归一化
  模拟真实传感器个体差异的消除过程
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

# 基线段定义：前20步为暴露前稳定段
BASELINE_END = 20

DATA_DIR   = r'G:\Cursor\kato-modbus\Simulation\algorithm\Data'
INPUT_PATH = os.path.join(DATA_DIR, '02_Kalman_Fast', 'X_kf_fast.npy')
LABEL_PATH = os.path.join(DATA_DIR, '02_Kalman_Fast', 'y_labels.npy')
OUTPUT_DIR = os.path.join(DATA_DIR, '03_Normalization')
PHOTO_DIR  = r'G:\Cursor\kato-modbus\Simulation\algorithm\Photo\03_Normalization'
os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(PHOTO_DIR,  exist_ok=True)

DPI      = 300
PNG_ARGS = dict(dpi=DPI, bbox_inches='tight', format='png')

COLORS = ['#378ADD', '#D85A30', '#1D9E75', '#BA7517']
COLOR_BEFORE = '#B4B2A9'
COLOR_AFTER  = '#378ADD'


# ─────────────────────────────────────────────
# 归一化核心
# ─────────────────────────────────────────────

def normalize_sample(sample, baseline_end):
    """
    对单条样本做基线归一化
    sample: shape=(T, 4)
    每个通道各自用自己的基线段均值做归一化
    R0_local = mean(sample[:baseline_end, c])
    output   = sample / R0_local
    """
    out    = sample.copy()
    r0_local = np.zeros(sample.shape[1])

    for c in range(sample.shape[1]):
        baseline_val = np.mean(sample[:baseline_end, c])
        # 防止除零
        if baseline_val < 1e-4:
            baseline_val = 1e-4
        r0_local[c] = baseline_val
        out[:, c]   = sample[:, c] / baseline_val

    return out, r0_local


def apply_normalization(X, baseline_end):
    """对全部样本做基线归一化"""
    N, T, C  = X.shape
    X_out    = np.zeros_like(X)
    R0_all   = np.zeros((N, C))   # 记录每条样本每通道的R0_local

    for i in range(N):
        X_out[i], R0_all[i] = normalize_sample(X[i], baseline_end)

    return X_out, R0_all


# ─────────────────────────────────────────────
# 图1：归一化前后基线对比（四通道叠加）
# ─────────────────────────────────────────────

def plot_baseline_alignment(X_in, X_out, y):
    """
    图1：归一化前后四通道基线对齐效果
    左：归一化前（基线有偏差）
    右：归一化后（基线统一到1.0）
    """
    fp = get_fp()
    air_indices = np.where(y == 0)[0][:5]   # 取5条空气样本
    t = np.arange(X_in.shape[1])

    fig, axes = plt.subplots(1, 2, figsize=(16, 6))

    for ax, X_data, title in zip(
        axes,
        [X_in, X_out],
        ['归一化前（各通道基线存在偏差）',
         '归一化后（基线统一对齐至 1.0）']
    ):
        for c, ch in enumerate(CHANNELS):
            for idx in air_indices:
                ax.plot(t, X_data[idx, :, c],
                        color=COLORS[c], lw=1.0, alpha=0.6)
            # 画一条粗线作为代表
            ax.plot(t, X_data[air_indices[0], :, c],
                    color=COLORS[c], lw=2.2,
                    label=CH_LABELS[ch])

        ax.axhline(y=1.0, color='red', lw=1.2,
                   linestyle='--', alpha=0.6, label='基准线 Rs/R0=1.0')
        ax.set_title(title, fontsize=13, fontproperties=fp)
        ax.set_xlabel('时间步 (×0.1 s)', fontsize=12, fontproperties=fp)
        ax.set_ylabel('Rs/R0', fontsize=12, fontproperties=fp)
        ax.legend(prop=fp, fontsize=10)
        ax.grid(True, alpha=0.3)
        ax.set_ylim(0.5, 1.8)

    plt.suptitle('Rs/R0 基线归一化效果对比（正常空气样本）',
                 fontsize=14, fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '01_baseline_alignment.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图1] 基线对齐对比 -> {path}")


# ─────────────────────────────────────────────
# 图2：酒精暴露下归一化前后对比（每通道）
# ─────────────────────────────────────────────

def plot_alcohol_response(X_in, X_out, y):
    """
    图2：酒精暴露样本归一化前后对比
    证明：归一化后响应曲线形状保留，基线统一
    """
    fp = get_fp()
    alcohol_idx = np.where(y == 1)[0][0]
    air_idx     = np.where(y == 0)[0][0]
    t = np.arange(X_in.shape[1])

    for c, ch in enumerate(CHANNELS):
        fig, axes = plt.subplots(2, 1, figsize=(12, 9))

        for ax, idx, scene in zip(
            axes,
            [alcohol_idx, air_idx],
            ['酒精暴露样本', '正常空气样本']
        ):
            before = X_in[ idx,  :, c]
            after  = X_out[idx, :, c]

            ax.plot(t, before, color=COLOR_BEFORE, lw=1.5,
                    label='归一化前', alpha=0.85)
            ax.plot(t, after,  color=COLOR_AFTER,  lw=2.2,
                    label='归一化后')
            ax.axhline(y=1.0, color='red', lw=1.0,
                       linestyle='--', alpha=0.5, label='基准线')

            # 基线均值统计
            base_before = np.mean(before[:BASELINE_END])
            base_after  = np.mean(after[:BASELINE_END])
            ax.text(0.02, 0.96,
                    f'基线均值：归一化前 = {base_before:.4f}    '
                    f'归一化后 = {base_after:.4f}',
                    transform=ax.transAxes, fontsize=10,
                    va='top', fontproperties=fp,
                    bbox=dict(boxstyle='round',
                              facecolor='#EAF3DE', alpha=0.8))

            ax.set_title(f'{CH_LABELS[ch]} — {scene}',
                         fontsize=13, fontproperties=fp)
            ax.set_xlabel('时间步 (×0.1 s)', fontsize=11,
                          fontproperties=fp)
            ax.set_ylabel('Rs/R0', fontsize=11, fontproperties=fp)
            ax.legend(prop=fp, fontsize=10, loc='upper right')
            ax.grid(True, alpha=0.3)

        plt.suptitle(f'{CH_LABELS[ch]}  基线归一化前后对比',
                     fontsize=14, fontproperties=fp)
        plt.tight_layout()
        path = os.path.join(PHOTO_DIR, f'02_{ch}_normalization.png')
        plt.savefig(path, **PNG_ARGS)
        plt.close()
        print(f"  [图2-{ch}] 归一化对比 -> {path}")


# ─────────────────────────────────────────────
# 图3：基线标准差柱状图（归一化效果量化）
# ─────────────────────────────────────────────

def plot_baseline_std_bar(X_in, X_out, y):
    """
    图3：归一化前后基线标准差对比
    证明：归一化后不同样本间基线一致性显著提升
    """
    fp = get_fp()
    air_indices = np.where(y == 0)[0]

    # 计算所有空气样本基线段的跨样本标准差（衡量一致性）
    std_before = []
    std_after  = []
    for c in range(len(CHANNELS)):
        baselines_before = X_in[air_indices, :BASELINE_END, c].mean(axis=1)
        baselines_after  = X_out[air_indices, :BASELINE_END, c].mean(axis=1)
        std_before.append(np.std(baselines_before))
        std_after.append( np.std(baselines_after))

    x      = np.arange(len(CHANNELS))
    width  = 0.35
    labels = [CH_LABELS[ch] for ch in CHANNELS]

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(x - width/2, std_before, width,
           label='归一化前', color=COLOR_BEFORE,
           edgecolor='gray', linewidth=0.8)
    ax.bar(x + width/2, std_after,  width,
           label='归一化后', color=COLOR_AFTER,
           edgecolor='#185FA5', linewidth=0.8)

    for i, (b, a) in enumerate(zip(std_before, std_after)):
        reduc = (b - a) / b * 100 if b > 1e-8 else 0.0
        ax.text(x[i], max(b, a) + 0.0003,
                f'↓{reduc:.1f}%',
                ha='center', va='bottom', fontsize=11,
                color='#D85A30', fontproperties=fp,
                fontweight='bold')

    ax.set_xlabel('传感器通道', fontsize=13, fontproperties=fp)
    ax.set_ylabel('基线跨样本标准差', fontsize=13, fontproperties=fp)
    ax.set_title('归一化前后基线一致性对比（空气样本）',
                 fontsize=14, fontproperties=fp)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontproperties=fp, fontsize=12)
    ax.legend(prop=fp, fontsize=11)
    ax.grid(True, axis='y', alpha=0.3)
    ax.set_ylim(0, max(std_before) * 1.35)

    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '03_baseline_std_bar.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图3] 基线一致性柱状图 -> {path}")


# ─────────────────────────────────────────────
# 统计报告
# ─────────────────────────────────────────────

def print_stats(X_in, X_out, y, R0_all):
    print(f"\n{'='*56}")
    print(f"  Rs/R0 基线归一化效果统计")
    print(f"{'='*56}")
    air_idx = np.where(y == 0)[0]
    for c, ch in enumerate(CHANNELS):
        b_before = X_in[air_idx,  :BASELINE_END, c].mean(axis=1)
        b_after  = X_out[air_idx, :BASELINE_END, c].mean(axis=1)
        print(f"  {CH_LABELS[ch]:8s}  "
              f"基线均值: 前={b_before.mean():.4f}±{b_before.std():.4f}  "
              f"后={b_after.mean():.4f}±{b_after.std():.4f}")
    print(f"\n  R0_local 范围（各通道）:")
    for c, ch in enumerate(CHANNELS):
        print(f"  {CH_LABELS[ch]:8s}  "
              f"min={R0_all[:,c].min():.4f}  "
              f"max={R0_all[:,c].max():.4f}  "
              f"mean={R0_all[:,c].mean():.4f}")
    print(f"\n  输入张量: {X_in.shape}")
    print(f"  输出张量: {X_out.shape}")
    print(f"  图片分辨率: {DPI} DPI")
    print(f"{'='*56}")


# ─────────────────────────────────────────────
# 主流程
# ─────────────────────────────────────────────

def main():
    print("=" * 56)
    print("  03_Normalization: Rs/R0 基线归一化")
    print("=" * 56)

    print(f"\n[1/5] 读取数据...")
    X_in = np.load(INPUT_PATH)
    y    = np.load(LABEL_PATH)
    print(f"  输入: {X_in.shape}  标签: {y.shape}")

    print(f"\n[2/5] 执行基线归一化...")
    X_out, R0_all = apply_normalization(X_in, BASELINE_END)
    print(f"  完成，共处理 {len(y)} 条样本")

    print(f"\n[3/5] 保存结果...")
    X_out = X_out.astype(np.float32)
    y = y.astype(np.int64)
    np.save(os.path.join(OUTPUT_DIR, 'X_normalized.npy'), X_out)
    np.save(os.path.join(OUTPUT_DIR, 'y_labels.npy'),     y)
    np.save(os.path.join(OUTPUT_DIR, 'R0_local.npy'),     R0_all)
    print(f"  已保存 X_normalized.npy  y_labels.npy  R0_local.npy")

    print(f"\n[4/5] 生成可视化图片 ({DPI} DPI)...")
    plot_baseline_alignment(X_in, X_out, y)
    plot_alcohol_response(X_in, X_out, y)
    plot_baseline_std_bar(X_in, X_out, y)

    print(f"\n[5/5] 统计报告...")
    print_stats(X_in, X_out, y, R0_all)
    print(f"\n完成！图片保存在: {PHOTO_DIR}")


if __name__ == '__main__':
    main()
