# 🔬 智能药盒 + 摔倒检测系统 — 项目概览

> **MCU:** STM32F407ZGT6 (LQFP-144) | **RTOS:** FreeRTOS CMSIS-V1 | **编译工具链:** GCC/CMake
>  **项目代号:** `yao_he002` / `pillbox` | **生成配置:** STM32CubeMX 6.17.0 + STM32Cube FW_F4 V1.28.3

---

## 一、系统功能简介

本系统是一个**智能服药提醒与老人摔倒检测**一体化嵌入式方案，硬件以 STM32F407 为主控芯片，协调多个外设协同工作，主要功能包括：

| 序号 | 功能模块 | 描述 |
|------|---------|------|
| 1 | **药品闹钟提醒** | DS3231 高精度 RTC 按设定时间触发闹铃，支持降压药/血糖药双药品分类管理 |
| 2 | **串口屏人机交互** | TJC3224T120 3.2寸触摸屏，提供 booting→standby→reminding→confirmed/missed/snooze 多页面界面 |
| 3 | **语音合成播报** | LU6288 中文语音合成模块，闹铃触发时播报"该吃药了吃两颗"，确认时播报"已服药祝您身体健康" |
| 4 | **霍尔传感器开盖检测** | 49E 线性霍尔传感器 + ADC 轮询，判断用户是否真正打开药盒取药 |
| 5 | **AI 视觉摔倒检测** | MaixCAM Pro 运行 YOLO11-Pose 人体姿态检测，通过 Maix 二进制通信协议将摔倒事件上报给 STM32 |
| 6 | **微信实时推送** | ESP-01 (ESP8266) WiFi 模块通过 AT 指令连接 SSL，经 Server酱 向微信推送摔倒警报 |
| 7 | **RGB LED 状态指示** | 三色调试灯 (PA5=R, PA6=G, PA7=B) + 双路绿色药品指示灯 (PA0=降压药, PA1=血糖药) |

### 工作流程（简版）

```
上电 → Booting(2秒) → Standby(等待设闹钟/显示当前时间)
                                   ↓
                      用户在串口屏设定闹钟 (b11/b22按钮)
                                   ↓
                      DS3231闹钟到点 → SQW引脚触发PB12 EXTI中断
                                   ↓
                      Reminding(串口屏+语音提醒)
                      ├─ 用户触摸确认按钮 + 霍尔检测到开盖 → Confirmed
                      └─ 5分钟超时 → Missed/Snooze
                                   ↓
                      返回Standby，等待下次闹钟

并行运行：MaixCAM Pro 持续做姿态检测 → 检测到摔倒 → ESP-01 → 微信推送
```

---

## 二、系统架构

```
┌──────────────────────────────────────────────────────────┐
│                     STM32F407ZGT6                        │
│                                                          │
│  ┌─────────────────────────────────────────────────┐     │
│  │  FreeRTOS (CMSIS-V1)                             │     │
│  │  ┌───────────┐ ┌──────────┐ ┌─────────────────┐ │     │
│  │  │MainHmiTask│ │VoiceTask │ │  SensorTask     │ │     │
│  │  │(High,256) │ │(Norm,256)│ │  (Low,128)      │ │     │
│  │  │主状态机FSM │ │语音播报  │ │  RTC刷屏+ADC    │ │     │
│  │  └───────────┘ └──────────┘ └─────────────────┘ │     │
│  │  ┌───────────┐ ┌──────────┐                     │     │
│  │  │OpenmvTask │ │Esp01Task │  defaultTask(Idle)  │     │
│  │  │(Norm,256) │ │(Norm,512)│  (Norm,128)         │     │
│  │  │摔倒数据接收│ │WiFi+推送 │                     │     │
│  │  └───────────┘ └──────────┘                     │     │
│  └─────────────────────────────────────────────────┘     │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │  IPC: BinarySemaphore(HMI RX) + Queues(Voice,    │    │
│  │       OpenMV, FallAlert, ESP01) + Mutex(UART1)    │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ │
│  │USART1│ │USART2│ │USART3│ │USART6│ │ADC3  │ │EXTI  │ │
│  │115200│ │9600  │ │115200│ │115200│ │IN9   │ │12/3  │ │
│  │屏幕  │ │语音  │ │OpenMV│ │ESP01 │ │霍尔  │ │闹钟  │ │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ │
└──────────────────────────────────────────────────────────┘
        │        │        │        │        │        │
   ┌────┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐
   │TJC屏 │ │LU6288│ │Maix │ │ESP01│ │49E  │ │DS3231│
   │3.2"  │ │语音  │ │CAM  │ │WiFi │ │霍尔 │ │RTC  │
   └──────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘
```

---

## 三、STM32 全部接线总表

> **说明：** 以下列出 STM32F407ZGT6 上**每一根有实际连接的引脚**，按功能分组。未列出的引脚为悬空/未使用。

### 3.0 芯片基本信息

| 项目 | 参数 |
|------|------|
| 芯片型号 | STM32F407ZGT6 |
| 封装 | LQFP-144 |
| 主频 | 168MHz (HSE 8MHz → PLL: M=8, N=336, P=2) |
| APB1 总线频率 | 42MHz |
| APB2 总线频率 | 84MHz |
| Flash Latency | 5 WS |
| 系统供电 | 3.3V |
| FreeRTOS 堆/栈 | Heap=0x200, Stack=0x400 |

---

### 3.1 电源引脚

| 引脚编号 | 引脚名 | 接到哪里 | 说明 |
|----------|--------|----------|------|
| 6, 11, 19, 21, 22, 28, 50, 75, 100 | **VDD** | 3.3V 电源 | 数字供电，所有 VDD 并联，每组就近接 100nF + 10µF 去耦电容 |
| 10, 20, 27, 49, 74, 99 | **VSS** | GND | 数字地 |
| 31 | **VDDA** | 3.3V 模拟电源 | ADC/复位等模拟电路供电，经磁珠/0Ω 电阻与 VDD 隔离 |
| 32 | **VREF+** | 3.3V | ADC 参考电压正极 |
| 33 | **VSSA** | GND | 模拟地 |
| 34 | **VBAT** | 纽扣电池 (3.0V) 或 3.3V | RTC 备份域供电，断电保持备份寄存器 |

---

### 3.2 时钟引脚

| 引脚编号 | 引脚名 | 接到哪里 | 说明 |
|----------|--------|----------|------|
| 12 | **PH0 (OSC_IN)** | 8MHz 无源晶振 + 20pF 电容 → GND | HSE 高速外部时钟，系统主时钟源 |
| 13 | **PH1 (OSC_OUT)** | 8MHz 无源晶振 + 20pF 电容 → GND | HSE 高速外部时钟 |
| 8 | **PC14 (OSC32_IN)** | 32.768kHz 无源晶振 + 10pF 电容 → GND | LSE 低速外部时钟（开发板标配，本项目用 DS3231 替代内部 RTC，但晶振仍焊在板上） |
| 9 | **PC15 (OSC32_OUT)** | 32.768kHz 无源晶振 + 10pF 电容 → GND | LSE 低速外部时钟 |

---

### 3.3 SWD 调试接口 (ST-Link / J-Link)

| 引脚编号 | 引脚名 | 接到哪里 | 说明 |
|----------|--------|----------|------|
| 72 | **PA13 (SWDIO)** | 调试器 SWDIO | 串行线调试数据，内部上拉 |
| 76 | **PA14 (SWCLK)** | 调试器 SWCLK | 串行线调试时钟，内部下拉 |
| 14 | **NRST** | 复位按钮 → GND + 100nF → GND | 手动复位 + 上电复位，调试器也可驱动此引脚 |
| 94 | **BOOT0** | 跳线帽：GND (运行) / 3.3V (烧录) | 启动模式选择 |

---

### 3.4 USART1 (PA9/PA10 被 CubeMX 占用，代码重映射至 PB6/PB7) — TJC 串口屏

| 引脚编号 | 引脚名 | 方向 | 接到的设备 | STM32 配置 | 实际接线 |
|----------|--------|------|------------|-------------|----------|
| 136 | **PB6** | TX → | TJC 屏幕 RX | AF7 (USART1_TX), 推挽, 无上下拉, LOW speed | STM32 PB6 → 屏幕背面排针 RX |
| 137 | **PB7** | RX ← | TJC 屏幕 TX | AF7 (USART1_RX), 推挽, 无上下拉, LOW speed | STM32 PB7 ← 屏幕背面排针 TX |

| 参数 | 值 |
|------|-----|
| 波特率 | **115200** bps, 8N1 |
| 中断 | USART1_IRQn, NVIC 优先级 5:0 |
| 收发模式 | TX + RX (全双工) |
| 流控 | 无 |
| 线程保护 | `uart1Mutex` 互斥锁 (CMSIS-RTOS) |
| ISR 处理 | `HAL_UART_RxCpltCallback` → `HMI_ParseByte(byte)` 逐字节解析 0x55 协议帧 |
| ⚠️ 注意事项 | CubeMX 自动生成了 PA9/PA10 作为 USART1 默认引脚，**实际代码中手动改为 PB6/PB7**。请勿使用 PA9/PA10 接屏幕！ |

---

### 3.5 USART2 — LU6288 语音合成模块

| 引脚编号 | 引脚名 | 方向 | 接到的设备 | STM32 配置 | 实际接线 |
|----------|--------|------|------------|-------------|----------|
| 36 | **PA2** | TX → | LU6288 RX | AF7 (USART2_TX), 推挽, 无上下拉, LOW speed | STM32 PA2 → LU6288 模块 RX 引脚 |
| 37 | **PA3** | RX ← | LU6288 TX | AF7 (USART2_RX), 推挽, 无上下拉, LOW speed | ⚠️ **实际未接**——语音模块不向 MCU 发数据 |

| 参数 | 值 |
|------|-----|
| 波特率 | **9600** bps, 8N1 |
| 中断 | USART2_IRQn, NVIC 优先级 5:0 |
| 收发模式 | 仅 TX 有效使用 (半双工) |
| ISR 处理 | 接收字节直接丢弃，仅重开中断保持 RX 活跃 |
| 协议 | LU6288 `<G>GBK中文` 指令串 |

---

### 3.6 USART3 — MaixCAM Pro 摔倒检测

| 引脚编号 | 引脚名 | 方向 | 接到的设备 | STM32 配置 | 实际接线 |
|----------|--------|------|------------|-------------|----------|
| 111 | **PC10** | TX → | MaixCAM RX (A18) | AF7 (USART3_TX), 推挽, 无上下拉, **HIGH speed** | STM32 PC10 → MaixCAM IO A18 |
| 112 | **PC11** | RX ← | MaixCAM TX (A19) | AF7 (USART3_RX), 推挽, 无上下拉, **HIGH speed** | STM32 PC11 ← MaixCAM IO A19 |

| 参数 | 值 |
|------|-----|
| 波特率 | **115200** bps, 8N1 |
| 中断 | USART3_IRQn, NVIC 优先级 5:0 |
| 收发模式 | TX + RX (全双工) |
| 协议 | Maix 官方二进制通信协议: `[Header 0xAACABCBB][data_len LE][flags][cmd][body][CRC16-IBM LE]` |
| ISR 状态机 | `PROTO_SYNC → HEADER → DATA_LEN → PAYLOAD` 逐字节解析 |
| 防抖 | 摔倒报警冷却 30s |

---

### 3.7 USART6 — ESP-01 WiFi 模块

| 引脚编号 | 引脚名 | 方向 | 接到的设备 | STM32 配置 | 实际接线 |
|----------|--------|------|------------|-------------|----------|
| 96 | **PC6** | TX → | ESP-01 RX | AF8 (USART6_TX), 推挽, 无上下拉, **HIGH speed** | STM32 PC6 → ESP-01 排针 RX |
| 97 | **PC7** | RX ← | ESP-01 TX | AF8 (USART6_RX), 推挽, 无上下拉, **HIGH speed** | STM32 PC7 ← ESP-01 排针 TX |

| 参数 | 值 |
|------|-----|
| 波特率 | **115200** bps, 8N1 |
| 中断 | USART6_IRQn, NVIC 优先级 5:0 |
| 收发模式 | TX + RX (全双工) |
| 协议 | ESP8266 AT 指令集 |
| 线程保护 | `esp01_tx_mutex` 互斥锁 |

---

### 3.8 软件 I2C + EXTI — DS3231 RTC 时钟模块

| 引脚编号 | 引脚名 | 方向 | 接到的设备 | STM32 配置 | 实际接线 |
|----------|--------|------|------------|-------------|----------|
| 134 | **PB10** | SCL ↔ | DS3231 SCL | 软 I2C: 推挽输出 / 硬 I2C: AF4 开漏, 无上下拉 | STM32 PB10 → DS3231 模块 SCL 排针 |
| 135 | **PB11** | SDA ↔ | DS3231 SDA | 软 I2C: 开漏输出 / 硬 I2C: AF4 开漏, 无上下拉 | STM32 PB11 → DS3231 模块 SDA 排针 |
| 138 | **PB12** | INT ← | DS3231 SQW/INT | EXTI12, 下降沿触发, 内部上拉, LOW speed | STM32 PB12 ← DS3231 模块 SQW/INT 排针 |

| 参数 | 值 |
|------|-----|
| I2C 频率 | 100kHz (标准模式) |
| I2C 实现 | **代码实际使用软件 I2C** (`soft_i2c_ds3231.c`)，保留硬件 I2C1 代码备用 |
| DS3231 地址 | 0xD0 (写), 0xD1 (读) |
| 闹钟中断 | PB12 → EXTI15_10_IRQn, NVIC 优先级 5:0, ISR 中置位 `g_alarm_triggered = 1` |
| DS3231 供电 | 3.3V (主供电) + 纽扣电池 (CR2032, 断电走时保持) |
| 上拉电阻 | PB10/PB12 依赖 STM32 内部上拉或 DS3231 模块自带的上拉电阻 |

> ⚠️ **注意：** 此项目前后有两种 I2C 接线方案——早期测试版使用 PA15/PB7 (G431 开发板)，最终 F407 版本使用 PB10/PB11。`pin_test.c`、`bus_hardreset_test.c` 等诊断文件是 PA15/PB7 旧方案的遗留，**不要参考它们来接线**。

---

### 3.9 ADC3_IN9 — 49E 线性霍尔传感器 (药盒开盖检测)

| 引脚编号 | 引脚名 | 方向 | 接到的设备 | STM32 配置 | 实际接线 |
|----------|--------|------|------------|-------------|----------|
| 53 | **PF3** | IN ← | 49E 霍尔 OUT | ADC3_IN9, 模拟输入, 无上下拉 | STM32 PF3 → 49E 传感器 OUT 引脚 |

| 参数 | 值 |
|------|-----|
| ADC 分辨率 | 12bit (0~4095) |
| 采样时间 | 28 周期 |
| 时钟分频 | ADC_CLOCKPRESCALER_PCLK_DIV4 |
| 触发方式 | 软件触发, 单次转换 |
| 开盖阈值 | **ADC < 2200** → 磁铁远离 → 判定开盖 |
| 实测值 (关盖) | 磁铁靠近时 ADC ≈ 2353~2822 |
| 实测值 (开盖) | 磁铁远离时 ADC ≈ 2057~2061 |
| 轮询位置 | vMainHmiTask → SYS_REMINDING 子循环中调用 `Hall_GetState()` |
| 49E 供电 | 3.3V (VCC), GND |

> **原理：** 磁铁装在药盒盖上，49E 固定在盒体。关盖 → 磁铁靠近传感器 → 输出电压升高 → ADC 值高；开盖 → 磁铁远离 → 输出电压降低 → ADC 值低 → 低于 2200 阈值 → 确认开盖。

---

### 3.10 独立 GPIO — LED、按键

| 引脚编号 | 引脚名 | 方向 | 接到的设备 | STM32 配置 | 实际接线 |
|----------|--------|------|------------|-------------|----------|
| 34 | **PA0** | OUT → | 降压药 LED (绿色) | 推挽输出, 无上下拉, LOW speed | STM32 PA0 → 220Ω 限流电阻 → LED 阳极, LED 阴极 → GND |
| 35 | **PA1** | OUT → | 血糖药 LED (绿色) | 推挽输出, 无上下拉, LOW speed | STM32 PA1 → 220Ω 限流电阻 → LED 阳极, LED 阴极 → GND |
| 40 | **PA5** | OUT → | 调试 RGB LED — 红色通道 | 推挽输出, 无上下拉, LOW speed | STM32 PA5 → 限流电阻 → RGB LED R 引脚 |
| 41 | **PA6** | OUT → | 调试 RGB LED — 绿色通道 | 推挽输出, 无上下拉, LOW speed | STM32 PA6 → 限流电阻 → RGB LED G 引脚 |
| 42 | **PA7** | OUT → | 调试 RGB LED — 蓝色通道 | 推挽输出, 无上下拉, LOW speed | STM32 PA7 → 限流电阻 → RGB LED B 引脚 |
| 26 | **PB0** | IN ← | 手动摔倒测试按键 | 输入, 内部上拉, LOW speed | STM32 PB0 → 按键 → GND (按下低电平) |
| 27 | **PB1** | IN ← | 霍尔传感器备用 (实际悬空) | 输入, 无上下拉, LOW speed | ⚠️ **未接线**——原用作霍尔数字输入，后改用 PF3 ADC |
| 133 | **PB3** | IN ← | KEY 按键 (已弃用) | EXTI3, 下降沿触发, 内部上拉, LOW speed | ⚠️ **已弃用**——原用作通用调试按键 |
| 7 | **PC13** | OUT → | 板载蓝色 LED | 推挽输出, 无上下拉, LOW speed | 已在开发板上固定连接，**低电平点亮**，USART RX ISR 翻转作为数据心跳指示 |

**LED 汇总：**

| 功能 | 引脚 | 颜色 | 点亮电平 | 用途 |
|------|------|------|----------|------|
| 降压药指示灯 | PA0 | 🟢 绿色 | HIGH=亮 | 降压药闹钟触发时亮，确认服药后灭 |
| 血糖药指示灯 | PA1 | 🟢 绿色 | HIGH=亮 | 血糖药闹钟触发时亮，确认服药后灭 |
| 调试 RGB-R | PA5 | 🔴 红色 | HIGH=亮 | 摔倒事件检测到 / 调试用 |
| 调试 RGB-G | PA6 | 🟢 绿色 | HIGH=亮 | 摔倒事件已转发 / 调试用 |
| 调试 RGB-B | PA7 | 🔵 蓝色 | HIGH=亮 | 收到有效帧 / 调试用 |
| 板载 LED | PC13 | 🔵 蓝色 | **LOW=亮** | 板载调试灯, USART ISR 翻转指示数据活动 |

---

### 3.11 NVIC 中断优先级总表

| 中断源 | 优先级 (Preempt:Sub) | 触发引脚 | 用途 |
|--------|---------------------|----------|------|
| USART1_IRQn | 5:0 | PB7 | TJC 串口屏 RX 数据到达 |
| USART2_IRQn | 5:0 | PA3 | LU6288 语音模块 RX (仅维持中断活跃) |
| USART3_IRQn | 5:0 | PC11 | MaixCAM Pro RX 摔倒数据到达 |
| USART6_IRQn | 5:0 | PC7 | ESP-01 WiFi AT 响应数据到达 |
| EXTI3_IRQn | 5:0 | PB3 | KEY 按键 (已弃用，但中断仍使能) |
| EXTI15_10_IRQn | 5:0 | PB12 | DS3231 闹钟 SQW/INT 中断 |
| PendSV_IRQn | 15:0 | — | FreeRTOS 任务上下文切换 |
| SysTick_IRQn | 15:0 | — | FreeRTOS 时间片轮转 (1ms tick) |

> **设计原则：** 所有外设中断优先级统一为 5（子优先级 0），低于 `configMAX_SYSCALL_INTERRUPT_PRIORITY`（FreeRTOS 要求≥5），因此 ISR 中可安全使用 `xSemaphoreGiveFromISR`、`osMessagePut` 等 FromISR API。

---

### 3.12 引脚使用统计

| 分类 | 使用引脚数 | 引脚列表 |
|------|-----------|----------|
| 电源 | 14 | VDD×9, VSS×6, VDDA, VSSA, VREF+, VBAT |
| 时钟 | 4 | PH0, PH1, PC14, PC15 |
| SWD 调试 | 2 | PA13, PA14 |
| 复位/启动 | 2 | NRST, BOOT0 |
| UART 收发 | 8 | PB6, PB7, PA2, PA3, PC10, PC11, PC6, PC7 |
| I2C (软件) | 2 | PB10, PB11 |
| EXTI 中断 | 1 | PB12 |
| ADC 模拟输入 | 1 | PF3 |
| GPIO 输出 (LED) | 6 | PA0, PA1, PA5, PA6, PA7, PC13 |
| GPIO 输入 (按键) | 3 | PB0, PB1 (未用), PB3 (弃用) |
| **合计有实际连接的 I/O** | **~28 个** | (不含电源/地/复位/启动) |

---

## 四、FreeRTOS 任务一览

| 任务名称 | 优先级 | 堆栈(字) | 函数 | 职责 |
|----------|--------|----------|------|------|
| MainHmiTask | High | 256 | `vMainHmiTask` | 主 FSM 状态机, 屏幕交互, 开盖检测 |
| VoiceTask | Normal | 256 | `vVoiceTask` | 从 Queue 接收事件, 发送 GBK 语音指令 |
| SensorTask | Low | 128 | `vSensorTask` | 每 500ms 读 DS3231, 更新屏幕时间 |
| OpenmvTask | Normal | 256 | `vOpenmvTask` | MaixCAM 协议帧接收, 摔倒防抖, 转发 |
| Esp01Task | Normal | 512 | `vEsp01Task` | ESP-01 AT指令序列, Server酱推送 |
| defaultTask | Normal | 128 | `StartDefaultTask` | 空闲占位任务 |

### IPC (进程间通信) 一览

| 对象 | 类型 | 深度 | 方向 |
|------|------|------|------|
| `xHmiRxSemaphore` | Binary Semaphore | - | ISR(USART1) → MainHmiTask |
| `xVoiceQueue` | Queue | 8 × uint32_t | MainHmiTask → VoiceTask |
| `openmvQueueHandle` | Message Queue | 16 × uint32_t | ISR(USART3) → OpenmvTask |
| `fallAlertQueue` | Queue | 4 × uint32_t | OpenmvTask → Esp01Task |
| `esp01QueueHandle` | Queue | 8 × uint32_t | Esp01Task 内部事件 |
| `uart1MutexHandle` | Mutex | - | 保护 USART1 (屏幕) 发送 |

---

## 五、系统 FSM 状态机

```
                 ┌──────────┐
        ┌───────→│ STANDBY  │←──────────────────────┐
        │        │(待机)    │                       │
        │        └────┬─────┘                       │
        │             │ g_alarm_triggered=1          │
        │             ↓                              │
        │        ┌──────────┐  5min超时  ┌────────┐ │
        │        │REMINDING │───────────→│MISSED  │ │
        │        │(提醒中)  │            │(未服药) │ │
        │        └────┬─────┘            └───┬────┘ │
        │             │                     │      │
        │   开盖+确认 │              b111→  │      │
        │             ↓              snooze │      │
        │        ┌──────────┐    ┌────────┐│      │
        │        │CONFIRMED │    │SNOOZE  ││      │
        │        │(已服药)  │    │(稍后)  ││      │
        │        └────┬─────┘    └───┬────┘│      │
        │         5s后│         5s后│      │      │
        │             │              │30s后 │      │
        │             └──────────────┴──────┘      │
        └─────────────────────────────────────────┘
```

---

## 六、外部模块完整接线

> **说明：** 以下列出每个外部模块的**全部引脚接线**，包括接到 STM32 的信号线以及模块自身的电源/地线。接线方向以模块端为基准：`模块引脚 → 目标`。

---

### 6.1 TJC3224T120 串口屏 (3.2" 触摸屏)

```
┌─────────────────────────────┐
│  TJC3224T120 背面排针        │
│  ┌─────┬─────┬─────┬─────┐  │
│  │ VCC │  RX │  TX │ GND │  │
│  └──┬──┴──┬──┴──┬──┴──┬──┘  │
│     │     │     │     │      │
└─────┼─────┼─────┼─────┼──────┘
      │     │     │     │
      ▼     ▼     ▼     ▼
     5V   PB6   PB7   GND
   (独立  (STM32 (STM32 (共地)
   供电)  USART1 USART1
          _TX)   _RX)
```

| 屏幕端引脚 | 接到的位置 | 线材 | 说明 |
|-----------|-----------|------|------|
| **VCC** | 5V 电源 (独立供电) | 红 | 屏幕工作电压，**不能接 3.3V**，需独立 5V 电源或开发板 5V 输出 |
| **RX** | **STM32 PB6** (USART1_TX) | 任意 | 屏幕接收指令 |
| **TX** | **STM32 PB7** (USART1_RX) | 任意 | 屏幕发送触摸事件 |
| **GND** | 电源 GND + **STM32 GND** | 黑 | 必须与 STM32 共地！ |

---

### 6.2 LU6288 语音合成模块

| 模块端引脚 | 接到的位置 | 线材 | 说明 |
|-----------|-----------|------|------|
| **VCC** | 5V  | 红 | 模块支持宽电压 |
| **RX** | **STM32 PA2** (USART2_TX) | 任意 | MCU → 模块，发送 GBK 合成指令 |
| **TX** | ⚠️ **悬空不接** | — | 模块不向 MCU 回传数据 |
| **GND** | 电源 GND + **STM32 GND** | 黑 | 必须共地 |
| **SPK+/SPK-** | 喇叭 (8Ω 1~3W) | — | 语音输出 |

---

### 6.3 MaixCAM Pro (AI 视觉模块)

| MaixCAM 引脚 | 接到的位置 | 线材 | 说明 |
|-------------|-----------|------|------|
| **IO A19 (UART1_TX)** | **STM32 PC11** (USART3_RX) | 任意 | MaixCAM → STM32, 主动上报摔倒检测结果 |
| **IO A18 (UART1_RX)** | **STM32 PC10** (USART3_TX) | 任意 | STM32 → MaixCAM, 预留 |
| **GND** | **STM32 GND** | 黑 | 共地 |
| **VCC** | 5V (USB-C 供电) | — | MaixCAM 独立供电 |

---

### 6.4 ESP-01 (ESP8266 WiFi, 5 引脚版)

```
┌──────────────────────────┐
│  ESP-01 正面 (5引脚版)    │
│  ┌────┬────┬────┬────┐   │
│  │GND │ TX │ RX │RST │   │
│  │    │    │    │    │   │
│  │    │    │ VCC│    │   │
│  └──┬─┴──┬─┴──┬─┴──┬─┘   │
└─────┼────┼────┼────┼─────┘
      │    │    │    │
      ▼    ▼    ▼    ▼
     GND  PC7  PC6  3.3V
          (RX) (TX) (上拉)
```

| ESP-01 引脚 | 接到的位置 | 线材 | 说明 |
|------------|-----------|------|------|
| **VCC** | 3.3V | 红 | ⚠️ **峰值电流 >300mA**，不能用 STM32 板载 3.3V，必须用独立 LDO (如 AMS1117-3.3) |
| **GND** | 电源 GND + **STM32 GND** | 黑 | 必须共地 |
| **TX** | **STM32 PC7** (USART6_RX) | 任意 | ESP-01 → STM32, AT 响应数据 |
| **RX** | **STM32 PC6** (USART6_TX) | 任意 | STM32 → ESP-01, AT 指令 |




---

### 6.5 DS3231 RTC 高精度时钟模块

| 模块端引脚 | 接到的位置 | 线材 | 说明 |
|-----------|-----------|------|------|
| **VCC** | 3.3V | 红 | 主供电 |
| **GND** | **STM32 GND** | 黑 | 共地 |
| **SCL** | **STM32 PB10** | 任意 | I2C 时钟线 (软件 I2C / 硬件 I2C1 AF4) |
| **SDA** | **STM32 PB11** | 任意 | I2C 数据线 (软件 I2C / 硬件 I2C1 AF4) |
| **SQW/INT** | **STM32 PB12** (EXTI) | 任意 | 闹钟中断输出 (开漏, 需上拉) |
| **32K** | ⚠️ 悬空 (不用) | — | 32kHz 方波输出，本项目不需要 |
| **BAT** | CR2032 纽扣电池座正极 | — | 断电走时保持 |

---

### 6.6 49E 线性霍尔传感器 (药盒开盖检测)

| 传感器引脚 | 接到的位置 | 线材 | 说明 |
|-----------|-----------|------|------|
| **VCC** | 3.3V | 红 | 供电, 工作电压 2.3~10V |
| **GND** | **STM32 GND** | 黑 | 共地 |
| **OUT** | **STM32 PF3** (ADC3_IN9) | 任意 | 模拟电压输出 (0~3.3V 范围) |

---

### 6.7 LED 灯板

**A. 降压药 + 血糖药 双路绿色指示灯**

```
PA0 ───[220Ω]───►├─── GND    (降压药 LED, 绿色)
PA1 ───[220Ω]───►├─── GND    (血糖药 LED, 绿色)
```

| STM32 引脚 | 元件 | 接 GND 方式 | 说明 |
|------------|------|------------|------|
| PA0 | 220Ω 限流 → 绿色 LED 阳极 | LED 阴极 → GND | HIGH=3.3V 点亮 |
| PA1 | 220Ω 限流 → 绿色 LED 阳极 | LED 阴极 → GND | HIGH=3.3V 点亮 |

**B. 调试 RGB LED (共阴极 4 脚)**

```
PA5 ───[R]───►├───┐
PA6 ───[G]───►├───┤─── GND (共阴极)
PA7 ───[B]───►├───┘
```

| STM32 引脚 | 元件 | 接到 LED | 说明 |
|------------|------|----------|------|
| PA5 | 限流电阻 | RGB-R 引脚 | 红色通道, HIGH=亮 |
| PA6 | 限流电阻 | RGB-G 引脚 | 绿色通道, HIGH=亮 |
| PA7 | 限流电阻 | RGB-B 引脚 | 蓝色通道, HIGH=亮 |
| GND | — | 公共阴极 | 共阴 LED 的公共脚 |



---

## 七、TJC 串口屏页面结构

| 页面 ID | 名称 | 用途 |
|---------|------|------|
| page 0 | `booting` | 启动页，显示 2 秒后自动跳转 standby |
| page 1 | `standby` | 待机主页，显示当前时间 (t0)，设置闹钟 (b11=降压药, b22=血糖药) |
| page 2 | `reminding` | 服药提醒页，显示倒计时 (t1)，确认按钮 (b0) |
| page 3 | `confirmed` | 确认成功页，显示 5 秒后返回 standby |
| page 4 | `missed` | 超时未服药页，含稍后提醒按钮 (b111) |
| page 5 | `snooze` | 稍后提醒页，显示 5 秒后返回 standby |

**屏幕配置 (`program.s`):**
```
bauds=115200          // 与 STM32 USART1 一致
dims=100              // 背光亮度 100%
bkcmd=0               // 关闭指令执行结果返回
page 0                // 上电显示 booting 页
```

---

## 八、通信协议说明

### 8.1 STM32 ↔ TJC 串口屏
- **物理层:** UART 115200-8N1
- **指令格式:** ASCII 字符串 + `\xFF\xFF\xFF` 终止符
- **示例:** `page standby` + `\xFF\xFF\xFF`, `standby.t0.txt="12:30:00"` + `\xFF\xFF\xFF`

### 8.2 TJC 串口屏触摸自动上报
触摸按钮时屏幕自动返回 `0x65 + 页面ID + 控件ID + 0xFF 0xFF 0xFF`，STM32 端解析后触发对应动作。

### 8.3 STM32 ↔ MaixCAM Pro (Maix 二进制协议)
- **帧格式:** `[Header 4B LE] [data_len 4B LE] [flags 1B] [cmd 1B] [body nB] [CRC16 2B LE]`
- **Header:** `0xAA 0xCA 0xAC 0xBB`
- **CRC:** CRC16-IBM (多项式 0xA001, 初始值 0x0000)
- **自定义命令 0x03:** 摔倒警报 (body[0]: 0=正常, 1=摔倒)

### 8.4 STM32 ↔ ESP-01 (AT 指令)
- **AT 序列 (6 步):** AT → AT+GMR → AT+CWMODE=1 → AT+CWJAP → AT+CIPSTART="SSL" → AT+CIPSEND → HTTP GET → AT+CIPCLOSE
- **推送目标:** Server酱 API (`sctapi.ftqq.com`)

---

## 九、调试辅助

### RGB LED 颜色码 (PA5=R, PA6=G, PA7=B)

| 颜色 | 含义 |
|------|------|
| ⚪ 白色 (白闪一下) | 系统上电初始化 OK |
| 🔵 蓝色 (亮) | PC11 (USART3_RX) 收到有效帧 (CRC通过) |
| 🔴 红色 (亮) | 解析到真实摔倒事件 (cmd=0x03, status=1) |
| 🟢 绿色 (亮) | 摔倒事件已入队并转发给 ESP-01 任务 |
| 🟡 黄色 (亮) | ESP-01 正在执行 AT 指令序列 |
| 🟣 品红 (亮/闪) | 错误/超时/异常状态 |
| ⚫ 灭 | 空闲等待 |

### 串口屏诊断码 (显示于 reminding.t1)

| 诊断码 | 含义 |
|--------|------|
| RX55 | 检测到 0x55 帧头 |
| RX B0 | 检测到确认按钮 (b0) 被触摸 |
| RX ALARM | 检测到有效的设闹钟帧 |
| MED:BP / MED:SUGAR | 识别到药品类型闹钟 |
| LID IRQ | 霍尔传感器检测到开盖 |
| H{N} | 心跳计数 (每 500ms +1) |
| LOOP {N} | 提醒子循环存活标记 |

---

## 十、目录结构

```
yao_he002/
├── Inc/                        # 头文件
│   ├── main.h                  # 主头文件 (LED 引脚宏定义)
│   ├── gpio.h                  # GPIO 初始化声明
│   ├── usart.h                 # 全部 4 路 UART 声明
│   ├── hmi_screen.h            # TJC 串口屏驱动接口
│   ├── hall_sensor.h           # 49E 霍尔传感器接口
│   ├── soft_i2c_ds3231.h       # 软件 I2C + DS3231 驱动
│   ├── lu6288_voice.h          # LU6288 GBK 语音数据
│   ├── openmv_uart.h           # MaixCAM 通信协议
│   ├── esp01_wifi.h            # ESP-01 WiFi + Server酱
│   ├── debug_rgb.h             # RGB LED 调试模块
│   ├── medicine_manager.h      # 药品管理器
│   ├── i2c.h                   # 硬件 I2C1 定义
│   ├── rtc_driver.h            # RTC 驱动
│   ├── stm32f4xx_hal_conf.h    # HAL 库配置
│   └── FreeRTOSConfig.h        # FreeRTOS 配置文件
├── Src/                        # 源文件
│   ├── main.c                  # 主逻辑 (FSM + 任务实现)
│   ├── app_freertos.c          # FreeRTOS 任务/队列/信号量创建
│   ├── gpio.c                  # GPIO 初始化
│   ├── usart.c                 # 4 路 UART 初始化 + ISR
│   ├── hmi_screen.c            # TJC 0x55 协议帧解析
│   ├── hall_sensor.c           # ADC3_IN9 霍尔读取
│   ├── soft_i2c_ds3231.c       # 软件 I2C + DS3231 寄存器操作
│   ├── openmv_uart.c           # Maix 二进制协议状态机
│   ├── esp01_wifi.c            # ESP-01 AT 指令序列
│   ├── debug_rgb.c             # RGB LED 控制
│   ├── i2c.c                   # 硬件 I2C1 初始化+总线恢复
│   ├── medicine_manager.c      # 药品管理逻辑
│   └── stm32f4xx_hal_msp.c     # HAL MSP 初始化
├── Drivers/                    # HAL 库驱动
├── Middlewares/                # FreeRTOS 中间件
├── build/                      # CMake 构建输出
├── cmake/stm32cubemx/          # CubeMX 生成的 CMake 模块
├── CMakeLists.txt              # 顶层 CMake 配置
├── CMakePresets.json           # CMake 预设
├── STM32F407XX_FLASH.ld        # 链接脚本 (F407)
├── startup_stm32f407xx.s       # 启动汇编 (F407)
├── yao_he002.ioc               # STM32CubeMX 工程文件
├── TJC_控件触控事件代码.txt     # 串口屏触控参考
├── TJC_USART_HMI_program.s配置.txt  # 串口屏启动配置
├── main_maixcam.py             # MaixCAM Pro 摔倒检测 Python 代码
└── maixcam_fall_detect.py      # MaixCAM 摔倒检测备用版
```

---

## 十一、关键设计决策

1. **软件 I2C 替代硬件 I2C**：DS3231 实际使用 `soft_i2c_ds3231.c`（PB10/PB11 位带操作），保留硬件 I2C1 及总线恢复代码作为备选方案。

2. **双确认门控机制**：在 SYS_REMINDING 状态下，必须同时满足「屏幕确认按钮被触摸」(g_screen_confirmed) + 「霍尔传感器检测到开盖」(is_box_opened) 两个条件，才会进入 SYS_CONFIRMED，防止误触。

3. **UART1 命名颠倒**：代码中 `huart1` 实际对应硬件 USART2 (PA2/PA3, 9600bps, 语音模块)，而 `huart2` 对应硬件 USART1 (PB6/PB7, 115200bps, 屏幕模块)。这是因为 STM32CubeMX 的自动编号顺序导致，需注意区分。

4. **摔倒检测防抖**：MaixCAM 摔倒事件有 30 秒冷却时间 (FALL_ALERT_COOLDOWN)，防止短时间内重复微信推送。

5. **ISR 安全设计**：所有外设中断优先级统一为 5:0，低于 `configMAX_SYSCALL_INTERRUPT_PRIORITY`，确保 ISR 可安全调用 FreeRTOS FromISR API。

6. **HSE 故障回退**：若 8MHz 外部晶振启动失败，系统自动回退至 HSI 16MHz 内部振荡器运行。

---

> 📅 *文档生成日期: 2026-06-23*
> 📝 *基于项目源码实际分析，非自动生成*
