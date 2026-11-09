# ESP32-C3 + PCF8563 实时时钟（RTC）

基于 **ESP32-C3**（Arduino 框架）通过 **I²C** 读写 **PCF8563** 实时时钟芯片的小项目。
上电后串口每秒打印一次当前时间，并支持用一条串口命令设置日期和时间。

## 功能

- 通过 I²C 读取 PCF8563 的年、月、日、时、分、秒、星期
- 串口命令 `set YYYY-MM-DD HH:MM:SS` 设置时间
- 自动根据日期计算星期（星期日 = 0）
- 校验日期和时间合法性，支持闰年（2 月 29 日）
- 检测 PCF8563 的 `VL`（电压过低）标志并给出警告

## 硬件连接

| PCF8563 | ESP32-C3 | 说明 |
|---------|----------|------|
| SDA     | GPIO6    | I²C 数据线，需上拉电阻（通常模块自带 4.7kΩ） |
| SCL     | GPIO7    | I²C 时钟线，需上拉电阻 |
| VCC     | 3.3V     | 供电 |
| GND     | GND      | 共地 |
| VBAT    | 纽扣电池 | 可选，掉电保持时间（CR2032 等） |

- I²C 从机地址：`0x51`
- 串口波特率：`115200`

> 如果模块引脚序号与此不同，改动 `src/main.cpp` 顶部的 `SDA_PIN`、`SCL_PIN` 即可。

## 编译与烧录

需要安装 [PlatformIO](https://platformio.org/)（CLI 或 VS Code 插件）。

```bash
# 编译
pio run

# 烧录
pio run --target upload

# 打开串口监视器
pio device monitor
```

VS Code 用户也可以直接使用 PlatformIO 插件底部的 **Build / Upload / Monitor** 按钮。

## 使用说明

串口监视器打开后，会看到欢迎信息和命令提示。发送：

```text
set 2026-11-09 20:45:00
```

设置成功后输出类似：

```text
RTC time set successfully!
2026-11-09 20:45:00  Sunday
```

之后程序每秒自动打印当前时间：

```text
2026-11-09 20:45:01  Sunday
2026-11-09 20:45:02  Sunday
...
```

### 命令格式

```text
set YYYY-MM-DD HH:MM:SS
```

- 年份范围：`2000` – `2099`（PCF8563 只保存两位年份）
- 日期和时间必须合法，否则会提示 `Invalid date or time.`
- 输入过程中（有字符尚未回车）会暂停自动刷新，方便输入

### 错误提示

| 输出 | 含义 |
|------|------|
| `Invalid command.` | 命令格式不匹配 |
| `Invalid date or time.` | 日期/时间越界（如 2 月 30 日） |
| `Error: Failed to stop RTC.` / `Failed to start RTC.` | 写控制寄存器失败，检查接线和地址 |
| `RTC communication error` / `RTC read error` | I²C 通信失败 |
| `[WARNING: VL=1]` | 芯片检测到供电电压过低，时间可能不可靠，建议重新设置 |

## 项目结构

```text
.
├── platformio.ini        # PlatformIO 工程配置（esp32-c3-devkitm-1 / Arduino）
├── src/
│   └── main.cpp          # 全部逻辑：I²C 读写、BCD 转换、命令解析
├── include/              # 项目头文件（当前未使用）
├── lib/                  # 项目私有库（当前未使用）
├── test/                 # 单元测试（当前未使用）
└── .gitignore
```

## 实现要点

- PCF8563 的寄存器以 **BCD** 格式存储，代码中用 `decToBcd()` / `bcdToDec()` 转换
- 时间寄存器从 `0x02`（秒）开始连续 7 字节：秒、分、时、日、星期、月、年
- 秒寄存器的最高位 `VL` 表示电压过低；写入秒寄存器即可清除
- 设置时间前先向 `0x00` 写 `0x20` 停止计时，写完后写 `0x00` 重新启动
- 星期由 Zeller 同类算法在本地计算，不依赖芯片默认值

## 说明

- 本工程仅使用 Arduino 内置的 `Wire` 库，无需额外安装依赖。
- 需要掉电走时请务必接 `VBAT` 电池；否则每次断电后都需要重新 `set` 时间。
