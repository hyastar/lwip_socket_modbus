"""
modpoll.py — 模拟 STM32F429 发送 Modbus TCP 数据
用途：在 STM32 硬件就绪之前，验证云端 Rust Server 逻辑是否正确

寄存器映射（和 STM32 端保持一致）：
  Reg 0x0000 : MQ 通道1 ADC 原始值
  Reg 0x0001 : MQ 通道2 ADC 原始值
  Reg 0x0002 : 温度 × 100  (如 25.36°C → 2536)
  Reg 0x0003 : 湿度 × 100  (如 60.12% → 6012)
  Reg 0x0004 : 状态标志位  (0x0001 = 数据有效)
"""

import socket
import struct
import time
import random

# ── 服务器配置 ────────────────────────────────────────
SERVER_IP   = "151.242.85.89"   # 本地测试；上传服务器后改成公网IP
SERVER_PORT = 48651
# ─────────────────────────────────────────────────────

# 寄存器地址
REG_MQ1    = 0x0000
REG_COUNT  = 5

def build_write_frame(trans_id: int, regs: list[int]) -> bytes:
    """
    构造 Modbus TCP Write Multiple Registers 帧 (功能码 0x10)
    完全还原 STM32 端 modbus_build_write_frame() 的逻辑
    """
    data      = b"".join(struct.pack(">H", r) for r in regs)
    byte_cnt  = len(data)                          # 字节数字段
    pdu_len   = 1 + 1 + 2 + 2 + 1 + byte_cnt      # 单元ID之后所有字节数

    frame  = struct.pack(">H", trans_id)           # 事务ID  (2B)
    frame += struct.pack(">H", 0x0000)             # 协议ID  (2B, 固定0)
    frame += struct.pack(">H", pdu_len)            # 长度    (2B)
    frame += struct.pack("B",  0x01)               # 单元ID  (1B, 从站地址)
    frame += struct.pack("B",  0x10)               # 功能码  (1B)
    frame += struct.pack(">H", REG_MQ1)            # 起始寄存器地址 (2B)
    frame += struct.pack(">H", len(regs))          # 寄存器数量     (2B)
    frame += struct.pack("B",  byte_cnt)           # 字节数         (1B)
    frame += data                                  # 数据区

    return frame


def parse_response(resp: bytes) -> bool:
    """解析服务器回应帧，返回是否成功"""
    if len(resp) < 12:
        print(f"  [警告] 回应帧太短: {resp.hex()}")
        return False
    func = resp[7]
    if func == 0x10:
        start = struct.unpack(">H", resp[8:10])[0]
        count = struct.unpack(">H", resp[10:12])[0]
        print(f"  [回应] 功能码=0x10 起始地址=0x{start:04X} 写入数量={count} ✓")
        return True
    elif func == 0x90:          # 0x10 | 0x80 = 异常回应
        print(f"  [错误] 服务器返回异常码: 0x{resp[8]:02X}")
        return False
    return False


def run_once(sock: socket.socket, trans_id: int,
             mq1: int, mq2: int, temp_x100: int, humi_x100: int) -> bool:
    """发送一次数据，返回是否成功"""
    regs  = [mq1, mq2, temp_x100, humi_x100, 0x0001]
    frame = build_write_frame(trans_id, regs)

    print(f"\n[发送] 事务ID={trans_id}")
    print(f"  MQ1={mq1}  MQ2={mq2}  "
          f"温度={temp_x100/100:.2f}°C  湿度={humi_x100/100:.2f}%")
    print(f"  原始帧: {frame.hex(' ').upper()}")

    try:
        sock.sendall(frame)
        resp = sock.recv(256)
        return parse_response(resp)
    except Exception as e:
        print(f"  [异常] {e}")
        return False


def main():
    print("=" * 55)
    print("  modpoll.py — 模拟 STM32F429 Modbus TCP Client")
    print(f"  目标服务器: {SERVER_IP}:{SERVER_PORT}")
    print("=" * 55)

    trans_id = 0

    while True:
        try:
            print(f"\n[连接] 正在连接 {SERVER_IP}:{SERVER_PORT} ...")
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((SERVER_IP, SERVER_PORT))
            print("[连接] 成功！开始发送数据（Ctrl+C 停止）\n")

            while True:
                trans_id = (trans_id + 1) & 0xFFFF

                # 模拟传感器数据（正式环境换成真实读值）
                mq1      = random.randint(200, 800)
                mq2      = random.randint(100, 600)
                temp     = random.randint(2000, 3500)   # 20.00°C ~ 35.00°C
                humi     = random.randint(4000, 8000)   # 40.00% ~ 80.00%

                ok = run_once(sock, trans_id, mq1, mq2, temp, humi)
                if not ok:
                    print("[重连] 发送失败，断开重连...")
                    break

                time.sleep(1)   # 每秒上报一次，和 STM32 任务节奏一致

        except ConnectionRefusedError:
            print(f"[错误] 连接被拒绝，Server 是否已启动？5秒后重试...")
        except socket.timeout:
            print("[错误] 连接超时，5秒后重试...")
        except KeyboardInterrupt:
            print("\n\n[退出] 用户中断")
            break
        except Exception as e:
            print(f"[错误] {e}，5秒后重试...")
        finally:
            try:
                sock.close()
            except Exception:
                pass

        time.sleep(5)


if __name__ == "__main__":
    main()