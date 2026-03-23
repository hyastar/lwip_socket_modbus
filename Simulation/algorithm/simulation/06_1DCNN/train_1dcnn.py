"""
06_1DCNN: 1D卷积神经网络训练与评估
输入: Data/05_Kalman_Slow/X_kf_slow.npy   shape=(N, T, 4)
输出: Data/06_1DCNN/best_model.pth
      Data/06_1DCNN/training_log.csv

任务: 二分类 — 正常空气(0) vs 酒精暴露(1)
输入张量格式: (batch, channels=4, time_steps=100)
              PyTorch Conv1d约定: (batch, C_in, L)
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
from sklearn.metrics import (confusion_matrix, classification_report,
                              roc_curve, auc)
import warnings
warnings.filterwarnings('ignore')

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
DATA_DIR   = r'G:\Cursor\kato-modbus\Simulation\algorithm\Data'
INPUT_PATH = os.path.join(DATA_DIR, '05_Kalman_Slow', 'X_kf_slow.npy')
LABEL_PATH = os.path.join(DATA_DIR, '05_Kalman_Slow', 'y_labels.npy')
OUTPUT_DIR = os.path.join(DATA_DIR, '06_1DCNN')
PHOTO_DIR  = r'G:\Cursor\kato-modbus\Simulation\algorithm\Photo\06_1DCNN'
os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(PHOTO_DIR,  exist_ok=True)

# 训练超参数
BATCH_SIZE  = 32
EPOCHS      = 100
LR          = 1e-3
WEIGHT_DECAY= 1e-4
TRAIN_RATIO = 0.7
VAL_RATIO   = 0.15
TEST_RATIO  = 0.15
RANDOM_SEED = 42

# 设备
DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

DPI      = 300
PNG_ARGS = dict(dpi=DPI, bbox_inches='tight', format='png')

CLASS_NAMES = ['正常空气', '酒精暴露']


# ─────────────────────────────────────────────
# Dataset
# ─────────────────────────────────────────────

class MQDataset(Dataset):
    """
    MQ传感器时序数据集
    X shape: (N, T, C) -> 转置为 (N, C, T) 适配Conv1d
    """
    def __init__(self, X, y):
        # (N, T, C) -> (N, C, T)
        self.X = torch.FloatTensor(X.transpose(0, 2, 1))
        self.y = torch.LongTensor(y)

    def __len__(self):
        return len(self.y)

    def __getitem__(self, idx):
        return self.X[idx], self.y[idx]


# ─────────────────────────────────────────────
# 1D CNN 模型
# ─────────────────────────────────────────────

class MQ1DCNN(nn.Module):
    """
    面向MQ气敏传感器阵列的1D卷积神经网络
    输入: (batch, 4, 100)  — 4通道，100时间步
    输出: (batch, 2)       — 二分类logits

    架构:
      Conv1d块1: 提取局部时序特征（短窗口）
      Conv1d块2: 提取中等尺度特征
      Conv1d块3: 提取全局特征
      全局平均池化: 降维，增强泛化
      全连接分类头
    """
    def __init__(self, in_channels=4, num_classes=2):
        super(MQ1DCNN, self).__init__()

        # 卷积块1：局部特征，小卷积核
        self.conv1 = nn.Sequential(
            nn.Conv1d(in_channels, 32, kernel_size=3, padding=1),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.Conv1d(32, 32, kernel_size=3, padding=1),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.MaxPool1d(2),        # 100 -> 50
            nn.Dropout(0.2),
        )

        # 卷积块2：中等特征
        self.conv2 = nn.Sequential(
            nn.Conv1d(32, 64, kernel_size=5, padding=2),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.Conv1d(64, 64, kernel_size=5, padding=2),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.MaxPool1d(2),        # 50 -> 25
            nn.Dropout(0.2),
        )

        # 卷积块3：全局特征
        self.conv3 = nn.Sequential(
            nn.Conv1d(64, 128, kernel_size=7, padding=3),
            nn.BatchNorm1d(128),
            nn.ReLU(),
            nn.MaxPool1d(2),        # 25 -> 12
            nn.Dropout(0.3),
        )

        # 全局平均池化
        self.gap = nn.AdaptiveAvgPool1d(1)   # -> (batch, 128, 1)

        # 分类头
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Dropout(0.4),
            nn.Linear(64, num_classes),
        )

    def forward(self, x):
        x = self.conv1(x)
        x = self.conv2(x)
        x = self.conv3(x)
        x = self.gap(x)
        x = self.classifier(x)
        return x


# ─────────────────────────────────────────────
# 训练与验证
# ─────────────────────────────────────────────

def train_epoch(model, loader, criterion, optimizer, device):
    model.train()
    total_loss, correct, total = 0.0, 0, 0
    for X_batch, y_batch in loader:
        X_batch, y_batch = X_batch.to(device), y_batch.to(device)
        optimizer.zero_grad()
        logits = model(X_batch)
        loss   = criterion(logits, y_batch)
        loss.backward()
        optimizer.step()
        total_loss += loss.item() * len(y_batch)
        correct    += (logits.argmax(1) == y_batch).sum().item()
        total      += len(y_batch)
    return total_loss / total, correct / total


def eval_epoch(model, loader, criterion, device):
    model.eval()
    total_loss, correct, total = 0.0, 0, 0
    all_probs, all_labels = [], []
    with torch.no_grad():
        for X_batch, y_batch in loader:
            X_batch, y_batch = X_batch.to(device), y_batch.to(device)
            logits = model(X_batch)
            loss   = criterion(logits, y_batch)
            probs  = torch.softmax(logits, dim=1)[:, 1]
            total_loss += loss.item() * len(y_batch)
            correct    += (logits.argmax(1) == y_batch).sum().item()
            total      += len(y_batch)
            all_probs.extend(probs.cpu().numpy())
            all_labels.extend(y_batch.cpu().numpy())
    return (total_loss / total, correct / total,
            np.array(all_probs), np.array(all_labels))


# ─────────────────────────────────────────────
# 可视化
# ─────────────────────────────────────────────

def plot_training_curves(log_df):
    """图1：训练曲线 Loss + Accuracy"""
    fp = get_fp()
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    ax = axes[0]
    ax.plot(log_df['epoch'], log_df['train_loss'],
            color='#378ADD', lw=2.0, label='训练集Loss')
    ax.plot(log_df['epoch'], log_df['val_loss'],
            color='#D85A30', lw=2.0, label='验证集Loss')
    best_ep = log_df.loc[log_df['val_loss'].idxmin(), 'epoch']
    ax.axvline(x=best_ep, color='gray', lw=1.2,
               linestyle='--', alpha=0.7,
               label=f'最佳模型 Epoch={best_ep}')
    ax.set_title('训练/验证 Loss 曲线', fontsize=13,
                 fontproperties=fp)
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
    ax.set_title('训练/验证 准确率曲线', fontsize=13,
                 fontproperties=fp)
    ax.set_xlabel('Epoch', fontsize=12, fontproperties=fp)
    ax.set_ylabel('准确率 (%)', fontsize=12, fontproperties=fp)
    ax.legend(prop=fp, fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 105)

    plt.suptitle('1D CNN 训练过程曲线', fontsize=14,
                 fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '01_training_curves.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图1] 训练曲线 -> {path}")


def plot_confusion_matrix(y_true, y_pred, dataset_name='测试集'):
    """图2：混淆矩阵"""
    fp = get_fp()
    cm = confusion_matrix(y_true, y_pred)

    fig, ax = plt.subplots(figsize=(7, 6))
    im = ax.imshow(cm, interpolation='nearest',
                   cmap=plt.cm.Blues)
    plt.colorbar(im, ax=ax)

    thresh = cm.max() / 2.0
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            ax.text(j, i, f'{cm[i,j]}',
                    ha='center', va='center', fontsize=16,
                    fontproperties=fp,
                    color='white' if cm[i,j] > thresh else 'black')

    ax.set_xticks([0, 1])
    ax.set_yticks([0, 1])
    ax.set_xticklabels(CLASS_NAMES,
                       fontproperties=fp, fontsize=12)
    ax.set_yticklabels(CLASS_NAMES,
                       fontproperties=fp, fontsize=12)
    ax.set_xlabel('预测标签', fontsize=13, fontproperties=fp)
    ax.set_ylabel('真实标签', fontsize=13, fontproperties=fp)
    ax.set_title(f'混淆矩阵 — {dataset_name}',
                 fontsize=14, fontproperties=fp)

    # 标注准确率
    acc = np.diag(cm).sum() / cm.sum()
    ax.text(0.98, 0.02, f'准确率 = {acc*100:.2f}%',
            transform=ax.transAxes,
            ha='right', va='bottom', fontsize=12,
            fontproperties=fp,
            bbox=dict(boxstyle='round',
                      facecolor='#E6F1FB', alpha=0.8))

    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '02_confusion_matrix.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图2] 混淆矩阵 -> {path}")


def plot_roc_curve(y_true, y_probs):
    """图3：ROC曲线"""
    fp = get_fp()
    fpr, tpr, _ = roc_curve(y_true, y_probs)
    roc_auc     = auc(fpr, tpr)

    fig, ax = plt.subplots(figsize=(7, 6))
    ax.plot(fpr, tpr, color='#378ADD', lw=2.5,
            label=f'ROC曲线 (AUC = {roc_auc:.4f})')
    ax.plot([0, 1], [0, 1], color='#B4B2A9',
            lw=1.5, linestyle='--', label='随机分类基线')
    ax.fill_between(fpr, tpr, alpha=0.15, color='#378ADD')
    ax.set_xlim([-0.02, 1.02])
    ax.set_ylim([-0.02, 1.05])
    ax.set_xlabel('假阳率 (FPR)', fontsize=13,
                  fontproperties=fp)
    ax.set_ylabel('真阳率 (TPR)', fontsize=13,
                  fontproperties=fp)
    ax.set_title('ROC曲线 — 测试集', fontsize=14,
                 fontproperties=fp)
    ax.legend(prop=fp, fontsize=11, loc='lower right')
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '03_roc_curve.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图3] ROC曲线 -> {path}")


def plot_prediction_demo(model, X_test, y_test, device):
    """
    图4：推理演示
    取4条测试样本（2空气+2酒精），展示MQ3通道序列
    和对应的分类概率
    """
    fp = get_fp()
    model.eval()

    air_idx  = np.where(y_test == 0)[0][:2]
    alc_idx  = np.where(y_test == 1)[0][:2]
    demo_idx = np.concatenate([air_idx, alc_idx])

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()
    t = np.arange(X_test.shape[1])
    colors_scene = ['#378ADD', '#378ADD', '#D85A30', '#D85A30']

    for ax_i, (ax, idx) in enumerate(zip(axes, demo_idx)):
        x_tensor = torch.FloatTensor(
            X_test[idx].transpose(1, 0)[np.newaxis]
        ).to(device)
        with torch.no_grad():
            logits = model(x_tensor)
            probs  = torch.softmax(logits, dim=1).cpu().numpy()[0]

        true_label = CLASS_NAMES[y_test[idx]]
        pred_label = CLASS_NAMES[probs.argmax()]
        correct    = '✓' if y_test[idx] == probs.argmax() else '✗'

        # MQ3通道（最灵敏）
        ax.plot(t, X_test[idx, :, 1],
                color=colors_scene[ax_i], lw=2.0,
                label='MQ-3 通道')

        ax.set_title(
            f'真实: {true_label}  预测: {pred_label} {correct}',
            fontsize=12, fontproperties=fp)
        ax.set_xlabel('时间步 (×0.1 s)', fontsize=10,
                      fontproperties=fp)
        ax.set_ylabel('Rs/R0', fontsize=10, fontproperties=fp)
        ax.legend(prop=fp, fontsize=9)
        ax.grid(True, alpha=0.3)

        # 概率条
        ax.text(0.98, 0.95,
                f'P(空气)={probs[0]*100:.1f}%\n'
                f'P(酒精)={probs[1]*100:.1f}%',
                transform=ax.transAxes,
                ha='right', va='top', fontsize=10,
                fontproperties=fp,
                bbox=dict(boxstyle='round',
                          facecolor='#FAEEDA', alpha=0.85))

    plt.suptitle('1D CNN 推理演示（MQ-3通道，测试集样本）',
                 fontsize=14, fontproperties=fp)
    plt.tight_layout()
    path = os.path.join(PHOTO_DIR, '04_inference_demo.png')
    plt.savefig(path, **PNG_ARGS)
    plt.close()
    print(f"  [图4] 推理演示 -> {path}")


# ─────────────────────────────────────────────
# 主流程
# ─────────────────────────────────────────────

def main():
    print("=" * 60)
    print("  06_1DCNN: 1D卷积神经网络训练与评估")
    print("=" * 60)
    print(f"  设备: {DEVICE}")
    if DEVICE.type == 'cuda':
        print(f"  GPU: {torch.cuda.get_device_name(0)}")

    # ── 1. 加载数据 ──
    print(f"\n[1/6] 加载数据...")
    X = np.load(INPUT_PATH)   # (N, T, 4)
    y = np.load(LABEL_PATH)
    print(f"  X: {X.shape}  y: {y.shape}")
    print(f"  空气={( y==0).sum()}  酒精={(y==1).sum()}")

    # ── 2. 划分数据集 7:1.5:1.5 ──
    print(f"\n[2/6] 划分训练/验证/测试集 "
          f"({int(TRAIN_RATIO*10)}:{int(VAL_RATIO*10)}"
          f":{int(TEST_RATIO*10)})...")
    X_tmp, X_test, y_tmp, y_test = train_test_split(
        X, y, test_size=TEST_RATIO,
        random_state=RANDOM_SEED, stratify=y)
    val_ratio_adj = VAL_RATIO / (TRAIN_RATIO + VAL_RATIO)
    X_train, X_val, y_train, y_val = train_test_split(
        X_tmp, y_tmp, test_size=val_ratio_adj,
        random_state=RANDOM_SEED, stratify=y_tmp)

    print(f"  训练集: {len(y_train)}  "
          f"验证集: {len(y_val)}  "
          f"测试集: {len(y_test)}")

    # ── 3. 构建DataLoader ──
    train_ds = MQDataset(X_train, y_train)
    val_ds   = MQDataset(X_val,   y_val)
    test_ds  = MQDataset(X_test,  y_test)

    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE,
                              shuffle=True,  drop_last=False)
    val_loader   = DataLoader(val_ds,   batch_size=BATCH_SIZE,
                              shuffle=False, drop_last=False)
    test_loader  = DataLoader(test_ds,  batch_size=BATCH_SIZE,
                              shuffle=False, drop_last=False)

    # ── 4. 初始化模型 ──
    print(f"\n[3/6] 初始化模型...")
    model     = MQ1DCNN(in_channels=4, num_classes=2).to(DEVICE)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(),
                           lr=LR, weight_decay=WEIGHT_DECAY)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(
        optimizer, T_max=EPOCHS, eta_min=1e-5)

    # 打印模型参数量
    total_params = sum(p.numel() for p in model.parameters()
                       if p.requires_grad)
    print(f"  可训练参数量: {total_params:,}")

    # ── 5. 训练循环 ──
    print(f"\n[4/6] 开始训练 ({EPOCHS} epochs)...")
    best_val_acc  = 0.0
    best_val_loss = float('inf')
    log_records   = []
    patience      = 20
    no_improve    = 0

    for epoch in range(1, EPOCHS + 1):
        tr_loss, tr_acc = train_epoch(
            model, train_loader, criterion, optimizer, DEVICE)
        va_loss, va_acc, _, _ = eval_epoch(
            model, val_loader, criterion, DEVICE)
        scheduler.step()

        log_records.append({
            'epoch':     epoch,
            'train_loss': tr_loss,
            'train_acc':  tr_acc,
            'val_loss':   va_loss,
            'val_acc':    va_acc,
        })

        # 保存最佳模型（以验证集loss为准）
        if va_loss < best_val_loss:
            best_val_loss = va_loss
            best_val_acc  = va_acc
            torch.save(model.state_dict(),
                       os.path.join(OUTPUT_DIR, 'best_model.pth'))
            no_improve = 0
            flag = '★'
        else:
            no_improve += 1
            flag = ''

        if epoch % 10 == 0 or epoch <= 5:
            print(f"  Epoch {epoch:3d}/{EPOCHS}  "
                  f"TrainLoss={tr_loss:.4f}  TrainAcc={tr_acc*100:.1f}%  "
                  f"ValLoss={va_loss:.4f}  ValAcc={va_acc*100:.1f}%  "
                  f"{flag}")

        # 早停
        if no_improve >= patience:
            print(f"\n  早停触发 (连续{patience}epoch无改善)")
            break

    log_df = pd.DataFrame(log_records)
    log_df.to_csv(os.path.join(OUTPUT_DIR, 'training_log.csv'),
                  index=False)
    print(f"\n  最佳验证集: Loss={best_val_loss:.4f}  "
          f"Acc={best_val_acc*100:.2f}%")

    # ── 6. 测试集评估 ──
    print(f"\n[5/6] 测试集评估...")
    model.load_state_dict(
        torch.load(os.path.join(OUTPUT_DIR, 'best_model.pth'),
                   map_location=DEVICE))
    te_loss, te_acc, te_probs, te_labels = eval_epoch(
        model, test_loader, criterion, DEVICE)
    # 使用argmax与训练目标保持一致，避免0.5阈值假设
    te_preds = (te_probs >= 0.5).astype(int)   # 保留用于ROC
    te_preds_argmax = np.array([
        1 if p >= 0.5 else 0 for p in te_probs
    ])
    # 分类报告和混淆矩阵统一用argmax结果
    te_preds = te_preds_argmax

    print(f"  测试集 Loss={te_loss:.4f}  Acc={te_acc*100:.2f}%")
    print(f"\n  分类报告:")
    print(classification_report(
        te_labels, te_preds,
        target_names=CLASS_NAMES, digits=4))

    # 保存测试结果
    np.save(os.path.join(OUTPUT_DIR, 'test_probs.npy'),  te_probs)
    np.save(os.path.join(OUTPUT_DIR, 'test_labels.npy'), te_labels)
    np.save(os.path.join(OUTPUT_DIR, 'test_preds.npy'),  te_preds)

    # ── 7. 可视化 ──
    print(f"\n[6/6] 生成可视化图片 ({DPI} DPI)...")
    plot_training_curves(log_df)
    plot_confusion_matrix(te_labels, te_preds)
    plot_roc_curve(te_labels, te_probs)
    plot_prediction_demo(model, X_test, y_test, DEVICE)

    print(f"\n{'='*60}")
    print(f"  训练完成！")
    print(f"  测试集准确率: {te_acc*100:.2f}%")
    print(f"  注意: 当前结果基于仿真数据，用于验证模型结构可行性")
    print(f"        最终真实性能需待实物到货后用真实采样数据评估")
    print(f"  模型保存: {OUTPUT_DIR}/best_model.pth")
    print(f"  图片保存: {PHOTO_DIR}")
    print(f"{'='*60}")


if __name__ == '__main__':
    torch.manual_seed(RANDOM_SEED)
    np.random.seed(RANDOM_SEED)
    main()
