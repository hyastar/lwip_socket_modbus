"""
06_1DCNN: STM32F411CEU6 轻量版 1D CNN
目标硬件: STM32F411CEU6 (Cortex-M4F, 512KB Flash, 128KB SRAM)
输入: Data/05_Kalman_Slow/X_kf_slow.npy   shape=(N, T, 4)  float32
输出: Data/06_1DCNN/f411_model.pth
      Data/06_1DCNN/f411_model.onnx       (用于X-CUBE-AI导入)

设计原则:
  1. 去除BatchNorm (推理时BN可融合进卷积，但轻量版直接省掉更简洁)
  2. 通道数大幅压缩: 4->8->16
  3. 全连接只保留GAP后直接分类
  4. 参数量目标: <3000个 (<12KB Flash占用)
  5. 无float64: 全程强制float32
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import os
import platform
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.metrics import confusion_matrix, classification_report, roc_curve, auc
import warnings
warnings.filterwarnings('ignore')

# ─────────────────────────────────────────────
# 中文字体
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
DATA_DIR    = r'G:\Cursor\kato-modbus\Simulation\algorithm\Data'
INPUT_PATH  = os.path.join(DATA_DIR, '05_Kalman_Slow', 'X_kf_slow.npy')
LABEL_PATH  = os.path.join(DATA_DIR, '05_Kalman_Slow', 'y_labels.npy')
OUTPUT_DIR  = os.path.join(DATA_DIR, '06_1DCNN')
PHOTO_DIR   = r'G:\Cursor\kato-modbus\Simulation\algorithm\Photo\06_1DCNN'
os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(PHOTO_DIR,  exist_ok=True)

BATCH_SIZE   = 32
EPOCHS       = 150
LR           = 1e-3
WEIGHT_DECAY = 1e-4
RANDOM_SEED  = 42
PATIENCE     = 25

DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

DPI      = 300
PNG_ARGS = dict(dpi=DPI, bbox_inches='tight', format='png')
CLASS_NAMES = ['正常空气', '酒精暴露']


# ─────────────────────────────────────────────
# Dataset (强制float32)
# ─────────────────────────────────────────────

class MQDataset(Dataset):
    def __init__(self, X, y):
        # (N,T,C) -> (N,C,T), 强制float32
        X32 = X.astype(np.float32)
        self.X = torch.from_numpy(X32.transpose(0, 2, 1))
        self.y = torch.from_numpy(y.astype(np.int64))

    def __len__(self):
        return len(self.y)

    def __getitem__(self, idx):
        return self.X[idx], self.y[idx]


# ─────────────────────────────────────────────
# F411轻量版 1D CNN
# ─────────────────────────────────────────────

class MQ1DCNN_F411(nn.Module):
    """
    STM32F411CEU6 轻量版 1D CNN
    输入:  (batch, 4, 100)
    输出:  (batch, 2)

    架构:
      Conv1d(4->8,  k=5) + ReLU + MaxPool(2)   # 100->50
      Conv1d(8->16, k=3) + ReLU + MaxPool(2)   # 50->25
      Conv1d(16->32,k=3) + ReLU + MaxPool(5)   # 25->5
      AdaptiveAvgPool1d(1)                      # 5->1
      Linear(32->2)

    无BatchNorm: 嵌入式推理无需BN，减少代码复杂度
    无Dropout:   推理阶段不使用，训练时用weight_decay代替
    """
    def __init__(self, in_channels=4, num_classes=2):
        super(MQ1DCNN_F411, self).__init__()

        self.features = nn.Sequential(
            # 块1: 局部特征，较大卷积核捕捉响应趋势
            nn.Conv1d(in_channels, 8, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.MaxPool1d(2),           # 100 -> 50

            # 块2: 中等特征
            nn.Conv1d(8, 16, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool1d(2),           # 50 -> 25

            # 块3: 压缩至小序列
            nn.Conv1d(16, 32, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool1d(5),           # 25 -> 5
        )

        self.gap = nn.AdaptiveAvgPool1d(1)   # -> (batch, 32, 1)

        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Linear(32, 2),
        )

    def forward(self, x):
        x = self.features(x)
        x = self.gap(x)
        x = self.classifier(x)
        return x


def count_params(model):
    return sum(p.numel() for p in model.parameters() if p.requires_grad)


def estimate_flash(model):
    """估算模型权重Flash占用(float32)"""
    params = count_params(model)
    return params * 4 / 1024   # KB


# ─────────────────────────────────────────────
# 训练/验证
# ─────────────────────────────────────────────

def train_epoch(model, loader, criterion, optimizer, device):
    model.train()
    total_loss, correct, total = 0.0, 0, 0
    for X_b, y_b in loader:
        X_b, y_b = X_b.to(device), y_b.to(device)
        optimizer.zero_grad()
        out  = model(X_b)
        loss = criterion(out, y_b)
        loss.backward()
        optimizer.step()
        total_loss += loss.item() * len(y_b)
        correct    += (out.argmax(1) == y_b).sum().item()
        total      += len(y_b)
    return total_loss / total, correct / total


def eval_epoch(model, loader, criterion, device):
    model.eval()
    total_loss, correct, total = 0.0, 0, 0
    all_probs, all_labels = [], []
    with torch.no_grad():
        for X_b, y_b in loader:
            X_b, y_b = X_b.to(device), y_b.to(device)
            out   = model(X_b)
            loss  = criterion(out, y_b)
            probs = torch.softmax(out, dim=1)[:, 1]
            total_loss += loss.item() * len(y_b)
            correct    += (out.argmax(1) == y_b).sum().item()
            total      += len(y_b)
            all_probs.extend(probs.cpu().numpy())
            all_labels.extend(y_b.cpu().numpy())
    return (total_loss/total, correct/total,
            np.array(all_probs, dtype=np.float32),
            np.array(all_labels, dtype=np.int64))


# ─────────────────────────────────────────────
# 可视化（复用训练曲线+混淆矩阵+ROC，文件名加f411前缀）
# ─────────────────────────────────────────────

def plot_training_curves(log_df, prefix='f411'):
    fp = get_fp()
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    best_ep = log_df.loc[log_df['val_loss'].idxmin(), 'epoch']

    ax = axes[0]
    ax.plot(log_df['epoch'], log_df['train_loss'],
            color='#378ADD', lw=2.0, label='训练集Loss')
    ax.plot(log_df['epoch'], log_df['val_loss'],
            color='#D85A30', lw=2.0, label='验证集Loss')
    ax.axvline(x=best_ep, color='gray', lw=1.2,
               linestyle='--', alpha=0.7,
               label=f'最佳模型 Epoch={best_ep}')
    ax.set_title('训练/验证 Loss 曲线 (F411轻量版)',
                 fontsize=13, fontproperties=fp)
    ax.set_xlabel('Epoch', fontsize=12, fontproperties=fp)
    ax.set_ylabel('Cross-Entropy Loss', fontsize=12,
                  fontproperties=fp)
    ax.legend(prop=fp, fontsize=10)
    ax.grid(True, alpha=0.3)

    ax = axes[1]
    ax.plot(log_df['epoch'], log_df['train_acc'] * 100,
            color='#378ADD', lw=2.0, label='训练集准确率')
    ax.plot(log_df['epoch'], log_df['val_acc'] * 100,
            color='#D85A30', lw=2.0, label='验证集准确率')
    ax.axvline(x=best_ep, color='gray', lw=1.2,
               linestyle='--', alpha=0.7)
    ax.set_title('训练/验证 准确率曲线 (F411轻量版)',
                 fontsize=13, fontproperties=fp)
    ax.set_xlabel('Epoch', fontsize=12, fontproperties=fp)
    ax.set_ylabel('准确率 (%)', fontsize=12, fontproperties=fp)
    ax.legend(prop=fp, fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 105)

    plt.suptitle('F411轻量版 1D CNN 训练过程',
                 fontsize=14, fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, f'{prefix}_01_training_curves.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图1] 训练曲线 -> {path}")


def plot_confusion_matrix(y_true, y_pred, prefix='f411'):
    fp = get_fp()
    cm = confusion_matrix(y_true, y_pred)
    fig, ax = plt.subplots(figsize=(7, 6))
    im = ax.imshow(cm, interpolation='nearest', cmap=plt.cm.Blues)
    plt.colorbar(im, ax=ax)
    thresh = cm.max() / 2.0
    for i in range(2):
        for j in range(2):
            ax.text(j, i, f'{cm[i,j]}',
                    ha='center', va='center', fontsize=16,
                    fontproperties=fp,
                    color='white' if cm[i,j] > thresh else 'black')
    ax.set_xticks([0, 1])
    ax.set_yticks([0, 1])
    ax.set_xticklabels(CLASS_NAMES, fontproperties=fp, fontsize=12)
    ax.set_yticklabels(CLASS_NAMES, fontproperties=fp, fontsize=12)
    ax.set_xlabel('预测标签', fontsize=13, fontproperties=fp)
    ax.set_ylabel('真实标签', fontsize=13, fontproperties=fp)
    acc = np.diag(cm).sum() / cm.sum()
    ax.set_title(f'混淆矩阵 — F411轻量版（测试集准确率={acc*100:.2f}%）',
                 fontsize=13, fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, f'{prefix}_02_confusion_matrix.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图2] 混淆矩阵 -> {path}")


def plot_roc(y_true, y_probs, prefix='f411'):
    fp = get_fp()
    fpr, tpr, _ = roc_curve(y_true, y_probs)
    roc_auc = auc(fpr, tpr)
    fig, ax = plt.subplots(figsize=(7, 6))
    ax.plot(fpr, tpr, color='#1D9E75', lw=2.5,
            label=f'ROC曲线 (AUC={roc_auc:.4f})')
    ax.plot([0,1],[0,1], color='#B4B2A9', lw=1.5,
            linestyle='--', label='随机基线')
    ax.fill_between(fpr, tpr, alpha=0.15, color='#1D9E75')
    ax.set_xlim([-0.02, 1.02])
    ax.set_ylim([-0.02, 1.05])
    ax.set_xlabel('假阳率 (FPR)', fontsize=13, fontproperties=fp)
    ax.set_ylabel('真阳率 (TPR)', fontsize=13, fontproperties=fp)
    ax.set_title('ROC曲线 — F411轻量版（测试集）',
                 fontsize=14, fontproperties=fp)
    ax.legend(prop=fp, fontsize=11, loc='lower right')
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, f'{prefix}_03_roc_curve.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图3] ROC曲线 -> {path}")


def plot_model_comparison(params_pc, params_f411,
                          flash_pc, flash_f411):
    """图4：PC版 vs F411版 参数量和Flash对比"""
    fp = get_fp()
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    for ax, vals, title, unit in zip(
        axes,
        [(params_pc, params_f411), (flash_pc, flash_f411)],
        ['参数量对比', 'Flash占用对比 (float32权重)'],
        ['个', 'KB']
    ):
        bars = ax.bar(['PC验证版', 'F411轻量版'],
                      vals,
                      color=['#378ADD', '#1D9E75'],
                      edgecolor='white', linewidth=0.8,
                      width=0.5)
        for bar, val in zip(bars, vals):
            ax.text(bar.get_x() + bar.get_width()/2,
                    bar.get_height() * 1.02,
                    f'{val:,.0f} {unit}',
                    ha='center', va='bottom',
                    fontsize=12, fontproperties=fp,
                    fontweight='bold')
        reduction = (vals[0] - vals[1]) / vals[0] * 100
        ax.text(0.98, 0.95,
                f'压缩率: {reduction:.1f}%',
                transform=ax.transAxes,
                ha='right', va='top', fontsize=12,
                fontproperties=fp,
                bbox=dict(boxstyle='round',
                          facecolor='#FAEEDA', alpha=0.8))
        ax.set_title(title, fontsize=13, fontproperties=fp)
        ax.set_ylabel(unit, fontsize=12, fontproperties=fp)
        ax.grid(True, axis='y', alpha=0.3)
        ax.set_ylim(0, vals[0] * 1.2)

    plt.suptitle('PC验证版 vs STM32F411轻量版模型对比',
                 fontsize=14, fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, 'f411_04_model_comparison.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图4] 模型对比 -> {path}")


# ─────────────────────────────────────────────
# 导出ONNX
# ─────────────────────────────────────────────

def export_onnx(model, device):
    """导出ONNX用于X-CUBE-AI导入"""
    model.eval()
    # 虚拟输入: batch=1, channels=4, time=100, float32
    dummy = torch.zeros(1, 4, 100,
                        dtype=torch.float32).to(device)
    onnx_path = os.path.join(OUTPUT_DIR, 'f411_model.onnx')
    torch.onnx.export(
        model, dummy, onnx_path,
        export_params=True,
        opset_version=11,
        do_constant_folding=True,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={
            'input':  {0: 'batch_size'},
            'output': {0: 'batch_size'},
        }
    )
    size_kb = os.path.getsize(onnx_path) / 1024
    print(f"  ONNX已导出: {onnx_path}")
    print(f"  ONNX文件大小: {size_kb:.1f} KB")
    return onnx_path


# ─────────────────────────────────────────────
# 主流程
# ─────────────────────────────────────────────

def main():
    torch.manual_seed(RANDOM_SEED)
    np.random.seed(RANDOM_SEED)

    print("=" * 62)
    print("  06_1DCNN F411轻量版: STM32F411CEU6 部署模型训练")
    print("=" * 62)
    print(f"  设备: {DEVICE}")
    if DEVICE.type == 'cuda':
        print(f"  GPU: {torch.cuda.get_device_name(0)}")

    # ── 1. 加载数据（强制float32）──
    print(f"\n[1/7] 加载数据...")
    X = np.load(INPUT_PATH).astype(np.float32)
    y = np.load(LABEL_PATH).astype(np.int64)
    print(f"  X: {X.shape} dtype={X.dtype}")
    print(f"  y: {y.shape} dtype={y.dtype}")
    print(f"  空气={( y==0).sum()}  酒精={(y==1).sum()}")

    # ── 2. 划分数据集 ──
    print(f"\n[2/7] 划分训练/验证/测试集 (7:1.5:1.5)...")
    X_tmp, X_test, y_tmp, y_test = train_test_split(
        X, y, test_size=0.15,
        random_state=RANDOM_SEED, stratify=y)
    X_train, X_val, y_train, y_val = train_test_split(
        X_tmp, y_tmp, test_size=0.15/0.85,
        random_state=RANDOM_SEED, stratify=y_tmp)
    print(f"  训练={len(y_train)}  验证={len(y_val)}  "
          f"测试={len(y_test)}")

    train_loader = DataLoader(
        MQDataset(X_train, y_train),
        batch_size=BATCH_SIZE, shuffle=True)
    val_loader   = DataLoader(
        MQDataset(X_val, y_val),
        batch_size=BATCH_SIZE, shuffle=False)
    test_loader  = DataLoader(
        MQDataset(X_test, y_test),
        batch_size=BATCH_SIZE, shuffle=False)

    # ── 3. 初始化模型 ──
    print(f"\n[3/7] 初始化F411轻量版模型...")
    model     = MQ1DCNN_F411().to(DEVICE)
    params    = count_params(model)
    flash_kb  = estimate_flash(model)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(),
                           lr=LR, weight_decay=WEIGHT_DECAY)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(
        optimizer, T_max=EPOCHS, eta_min=1e-5)

    print(f"  可训练参数量:   {params:,} 个")
    print(f"  权重Flash估算: {flash_kb:.1f} KB (float32)")
    print(f"  STM32F411 Flash剩余余量估算: "
          f"{512 - flash_kb:.0f} KB (不含程序代码)")

    # ── 4. 训练 ──
    print(f"\n[4/7] 开始训练 ({EPOCHS} epochs, "
          f"早停patience={PATIENCE})...")
    best_val_loss = float('inf')
    no_improve    = 0
    log_records   = []

    for epoch in range(1, EPOCHS + 1):
        tr_loss, tr_acc = train_epoch(
            model, train_loader, criterion, optimizer, DEVICE)
        va_loss, va_acc, _, _ = eval_epoch(
            model, val_loader, criterion, DEVICE)
        scheduler.step()

        log_records.append({
            'epoch': epoch,
            'train_loss': tr_loss, 'train_acc': tr_acc,
            'val_loss':   va_loss, 'val_acc':   va_acc,
        })

        if va_loss < best_val_loss:
            best_val_loss = va_loss
            torch.save(model.state_dict(),
                       os.path.join(OUTPUT_DIR, 'f411_model.pth'))
            no_improve = 0
            flag = '★'
        else:
            no_improve += 1
            flag = ''

        if epoch % 10 == 0 or epoch <= 5:
            print(f"  Epoch {epoch:3d}/{EPOCHS}  "
                  f"TrLoss={tr_loss:.4f} TrAcc={tr_acc*100:.1f}%  "
                  f"VaLoss={va_loss:.4f} VaAcc={va_acc*100:.1f}%"
                  f"  {flag}")

        if no_improve >= PATIENCE:
            print(f"\n  早停触发 (连续{PATIENCE}epoch无改善)")
            break

    log_df = pd.DataFrame(log_records)
    log_df.to_csv(
        os.path.join(OUTPUT_DIR, 'f411_training_log.csv'),
        index=False)

    # ── 5. 测试集评估 ──
    print(f"\n[5/7] 测试集评估...")
    model.load_state_dict(
        torch.load(os.path.join(OUTPUT_DIR, 'f411_model.pth'),
                   map_location=DEVICE))
    te_loss, te_acc, te_probs, te_labels = eval_epoch(
        model, test_loader, criterion, DEVICE)
    # argmax与训练目标一致，比0.5阈值更规范
    te_preds = (te_probs >= 0.5).astype(np.int64)

    print(f"  测试集 Loss={te_loss:.4f}  Acc={te_acc*100:.2f}%")
    print(f"\n  分类报告:")
    print(classification_report(
        te_labels, te_preds,
        target_names=CLASS_NAMES, digits=4))

    # ── 6. 导出ONNX ──
    print(f"\n[6/7] 导出ONNX (用于X-CUBE-AI)...")
    export_onnx(model, DEVICE)

    # ── 7. 可视化 ──
    print(f"\n[7/7] 生成图片 ({DPI} DPI)...")
    plot_training_curves(log_df)
    plot_confusion_matrix(te_labels, te_preds)
    plot_roc(te_labels, te_probs)
    # PC版参数量（已知）
    plot_model_comparison(
        params_pc=100866, params_f411=params,
        flash_pc=100866*4/1024, flash_f411=flash_kb
    )

    # ── 汇总 ──
    print(f"\n{'='*62}")
    print(f"  F411轻量版训练完成")
    print(f"  参数量:        {params:,} 个  "
          f"(PC版压缩 {(1-params/100866)*100:.1f}%)")
    print(f"  权重Flash:     {flash_kb:.1f} KB  "
          f"(512KB Flash剩余{512-flash_kb:.0f}KB)")
    print(f"  测试集准确率:  {te_acc*100:.2f}%")
    print(f"  ONNX文件:      {OUTPUT_DIR}/f411_model.onnx")
    print(f"  ─── X-CUBE-AI导入步骤 ───")
    print(f"  1. 打开STM32CubeMX -> 安装X-CUBE-AI扩展包")
    print(f"  2. 新建F411工程 -> Software Packs -> X-CUBE-AI")
    print(f"  3. 导入 f411_model.onnx")
    print(f"  4. Analyze分析内存占用和推理时间")
    print(f"  5. Generate Code生成C推理代码")
    print(f"  6. 在InferenceTask中调用 aiRun() 接口")
    print(f"{'='*62}")
    print(f"  注意: 当前结果基于仿真数据，验证模型结构可行性")
    print(f"        真实性能待硬件到货后用实采数据重新评估")


if __name__ == '__main__':
    main()
