// src/main.rs
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::sync::{Arc, Mutex};

// 寄存器映射（和 STM32 / modpoll.py 保持一致）
// Reg 0: MQ1 ADC值
// Reg 1: MQ2 ADC值
// Reg 2: 温度 × 100
// Reg 3: 湿度 × 100
// Reg 4: 状态标志位

#[tokio::main]
async fn main() {
    let listener = TcpListener::bind("0.0.0.0:48651").await.unwrap();
    println!("Modbus TCP Server 启动，监听 :48651");

    let regs: Arc<Mutex<[u16; 10]>> = Arc::new(Mutex::new([0u16; 10]));

    loop {
        let (mut socket, addr) = listener.accept().await.unwrap();
        println!("STM32 已连接: {}", addr);
        let regs = Arc::clone(&regs);

        tokio::spawn(async move {
            let mut buf = [0u8; 256];
            loop {
                let n = match socket.read(&mut buf).await {
                    Ok(0) | Err(_) => break,
                    Ok(n) => n,
                };

                // 最小帧长：MBAP(6) + 单元ID(1) + 功能码(1) + 起始地址(2) + 数量(2) + 字节数(1) = 13
                if n < 13 { continue; }

                let func  = buf[7];
                let start = u16::from_be_bytes([buf[8],  buf[9]])  as usize;
                let count = u16::from_be_bytes([buf[10], buf[11]]) as usize;
                // buf[12] = 字节数字段（跳过）
                // buf[13] = 第一个寄存器高字节  ← 数据从这里开始

                if func == 0x10 {
                    let resp = {
                        let mut r = regs.lock().unwrap();

                        for i in 0..count {
                            let idx = 13 + i * 2;   // 修正：数据从buf[13]开始
                            if start + i < 10 && idx + 1 < n {
                                r[start + i] = u16::from_be_bytes([buf[idx], buf[idx + 1]]);
                            }
                        }

                        // 按寄存器映射打印（和 modpoll.py 对应）
                        println!(
                            "[数据] MQ1={:4} | MQ2={:4} | 温度={:.2}°C | 湿度={:.2}% | 状态=0x{:04X}",
                            r[0],
                            r[1],
                            r[2] as f32 / 100.0,
                            r[3] as f32 / 100.0,
                            r[4]
                        );

                        // 构造回应帧（在锁作用域内，出括号后锁自动释放）
                        [
                            buf[0], buf[1],   // 事务ID 原样返回
                            0x00, 0x00,       // 协议ID
                            0x00, 0x06,       // 长度
                            buf[6],           // 单元ID
                            0x10,             // 功能码
                            buf[8], buf[9],   // 起始地址
                            buf[10], buf[11], // 寄存器数量
                        ]
                    }; // MutexGuard 在这里释放，await 之前已无锁

                    let _ = socket.write_all(&resp).await;
                }
            }
            println!("STM32 断开: {}", addr);
        });
    }
}