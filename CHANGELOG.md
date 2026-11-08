# CHANGELOG

本文档记录 W1251 OTA 项目的所有重要变更。

---

## [Unreleased] - 2026-11-05

### Added
- SPI 状态屏：`C/lcd.c` / `H/lcd.h`
  - ST7735S 128×160、4 线 SPI（SCK/MOSI/CS/DC，另加 RST），RGB565
  - SPI0：SCK = PB8、MOSI = PB9；CS = PB13、DC = PB15、RST = PD8（普通 GPIO）
  - SPI 时钟 8 MHz（SPR = 2，32 MHz / 4），模式 0，MSB first，8 位
  - 初始化拆成状态机（复位/SWRESET/SLPOUT/DISPON 分步等待），主循环照常处理 CAN
  - 直接写屏，无帧缓冲；5×8 ASCII 字库，支持 1×/2× 缩放
  - 每个 SPI 字节带超时保护：屏幕未接时自动停用，不影响 UDS
- 状态界面：`C/lcd_ui.c` / `H/lcd_ui.h`
  - 显示 Boot 版本、编译日期、APP 版本/日期、UDS 状态、下载进度条与百分比、会话/安全状态、CAN ID
  - 静态区逐项绘制 + 动态区 200 ms 节流刷新，只重画变化的行
  - `Lcd_UiLongOp()`：擦除/复制前只重画状态行（约 2 ms），让 `ERASING` / `COPYING` 可见
  - `Lcd_UiRefreshNow()`：`FF02` 成功后立即显示 `ACTIVATED`
- 串口日志：`H/log.h`
  - `BOOT_LOG_ENABLE` 默认 `0`；为 0 时 `LOG()` / `LOGF()` 编译为空，printf 不会链接
  - `BOOT_LOG_BAUD` 默认 115200，仅在日志开启时生效
  - 记录启动分支、会话切换、解锁、擦除、每 16 KB 下载进度、传输结束、CRC、复制/激活、所有非 `0x78` 负响应
- `OTA_GetInfo()`：从 INFO 区读出 version/date/size/crc 供屏幕显示
- `C/boot.h`：Bootloader 版本宏 `BOOT_VERSION_STR` / `BOOT_BUILD_DATE`、`BOOT_PART_NUMBER` / `BOOT_HW_VERSION_STR`
- `H/uds.h`：`UdsUiStateTypeDef`、`UdsUiStatusTypeDef`、`Uds_GetUiStatus()`
- **`0x22 ReadDataByIdentifier`**：默认会话即可读，无需解锁
  - `0xF186` 当前会话、`0xF187` 零件号、`0xF189`/`0xF195` Boot 版本、`0xF191` 硬件版本
  - `0x0100` Boot 版本号（major/minor/patch）、`0x0101` 编译日期
  - `0x0102` APP 版本、`0x0103` APP 日期、`0x0104` APP 大小、`0x0105` APP CRC
  - `0x0106` 刷写状态：记录有效位、状态（none/staging/committed）、刷写次数、上次结果
  - 支持一次请求多个 DID，未知 DID 回 NRC `0x31`，超长回 NRC `0x14`
- **`OTA_RequestUpgrade()`**：APP 请求升级的参考实现。只擦 `INFO_ADDRESS + 0x200` 这一个扇区并写入 `0xAA55AA55`，不再破坏版本/日期/CRC
- **INFO 记录完整性**：新增 `magic`（`0x4F544149`）、`staging`/`committed` 状态字、`info_crc`（覆盖 `0x00..0x1F`）、`flash_count`、`last_result`
- `OTA_InfoStart()` / `OTA_InfoFinish()`：把"写记录 → 复制 → 校验 → 提交"拆成两个阶段，中途掉电的记录不可信
- `tools/uds_flash.py --info`：读取上述 DID 并以可读形式打印

### Changed
- `C/main.C`：进入 UDS 模式后初始化状态屏，主循环调用 `Lcd_UiPoll()`；加入启动/跳转日志
- `C/boot.c`：日志开启时用 `BOOT_LOG_BAUD`（115200）初始化 UART0，关闭时仍为 9600
- `C/uds.c`：在现有服务分支中维护 UI 状态快照；`FF02` 成功后的跳转等待由 20 ms 改为 `LCD_ACTIVATE_HOLD_MS`（300 ms）
- `Test.uvprojx`：加入 `lcd.c`、`lcd_ui.c`、`lcd.h`、`lcd_ui.h`、`log.h`
- **`31 01 FF02` 激活流程改为断电安全**：
  1. 备份区 CRC 校验（失败直接 NEG，INFO 不动、APP 不擦）
  2. `OTA_InfoStart()` 先写新镜像记录（staging）并清除升级请求
  3. `OTA_Flash_CopyRange()` 复制 BACKUP → APP
  4. 校验 APP CRC
  5. `OTA_InfoFinish(OTA_RESULT_OK)` 写 committed + `info_crc`，记录才算有效
- **`OTA_Flash_CopyRange()` 改为倒序复制**：从最高字往 0 地址写，向量表（栈指针 + 复位向量）最后落盘。中途掉电时 `OTA_IsAppValid()` 读到 SP=0，停留在 Boot 而不是跳进半写好的 APP
- **INFO 布局扩展**：保留原来 0x00~0x10 的 5 个字段位置（旧记录仍可读），`0x14` 之后为新增字段；升级请求标志移到 `INFO_ADDRESS + 0x200` 的独立扇区，请求升级不再擦掉版本信息
- `C/boot.h`：版本号提升为 `BOOT v1.1.0`

### Fixed
- `startup_w1251.s`：向量表里的 `CAN_IRQHandler` 未声明，汇编报 `A1516E`。按其它中断的写法补上 `EXPORT ... [WEAK]` 和默认处理函数；`boot_can.c` 中的强符号仍会覆盖它
- `OTA_GetInfo()`：原实现只检查 `size` 范围，半写/擦除中断的记录也可能被判为有效。现在要求 `magic` + `info_crc` + `committed` 全部匹配；旧格式记录（无 magic）仍按原规则兼容

### Notes
- 屏幕只在进入 Bootloader 升级模式时初始化，正常启动直接跳 APP，不增加启动时间
- 若显示错位或颜色异常，调整 `H/lcd.h` 的 `LCD_X_OFFSET` / `LCD_Y_OFFSET`，或修改 `C/lcd.c` 中 MADCTL 值（`0xC8`；红蓝反了用 `0xC0`，上下颠倒用 `0x00`，左右镜像用 `0xA8`）
- 发布版必须保持 `BOOT_LOG_ENABLE = 0`，否则 printf 会占用额外 ROM
- 实测占用（ARMCC V5.06，-O1）：日志关闭 Code 12348 + RO 744 ≈ 12.9 KB（含 RW 112），ZI 2984 ≈ 2.9 KB。仍在 Boot 区 16 KB / RAM 8 KB 以内
- 断电保护边界：`34`~`FF01` 阶段 APP 完全不动，掉电只会丢备份区；从 `FF02` 开始 APP 会被擦除，因此**一旦进入 FF02，掉电后只能重新刷写**（此时备份区可能仍在，但 APP 已不完整，Boot 会拒绝跳转并停留在升级模式）
- 升级请求标志在 `OTA_InfoStart()` 里清除（即 `FF02` 开始时）。如果 `FF02` 之前就放弃升级，标志会保留，下次上电仍进 Boot；这是为了掉电/断线后能直接重试
- `0x22` 由 Bootloader 提供，所以只有在 Boot 模式（升级请求置位或 APP 无效）下才有响应；APP 若也需要被诊断读取版本，需在 APP 中实现同一套 DID
- INFO 区使用两个 512 字节扇区：`INFO_ADDRESS`（镜像记录）和 `INFO_ADDRESS + 0x200`（升级请求），互不影响

---

## [Unreleased] - 2026-11-03

### Added
- CAN BSP：`C/boot_can.c` / `H/boot_can.h`
  - 经典 CAN 2.0A、500 kbps、11-bit ID，请求 `0x7E0` / 响应 `0x7E8`
  - 位时序 `PRESC=3, SEG1=11, SEG2=2, SJW=1`（32 MHz，16 tq，采样点 81.25%）
  - 消息 RAM 使用绝对地址 `0x20007000`（1400 字节，位于 8 KB 链接区之外）
  - 接收中断 + 16 帧环形缓冲，SysTick 提供 1 ms 时基
- 厂商 CAN 库：`Driver/src/SC32F11xx_can.c` / `Driver/inc/SC32F11xx_can.h`（移植自 `CAN v0.1-wy` demo，保留原编码）
- ISO-TP：`C/isotp.c` / `H/isotp.h`
  - SF / FF / CF / FC 收发，12 位长度，BS / STmin，`N_Bs` / `N_Cr` 超时
  - 接收缓冲 258 字节，支持一条 `0x36` 携带 256 字节数据
- UDS 服务端：`C/uds.c` / `H/uds.h`
  - 服务：`0x10 / 0x11 / 0x27 / 0x28 / 0x31 / 0x34 / 0x36 / 0x37 / 0x3E / 0x85`
  - NRC：`0x11 / 0x12 / 0x13 / 0x22 / 0x24 / 0x31 / 0x33 / 0x35 / 0x36 / 0x37 / 0x71 / 0x72 / 0x73 / 0x78`
  - `0x27` 固定 seed `0x55667788` / key `0x11223344`，连续 3 次失败延时 10 s
  - 长操作（`0x34` 预擦除、`0x31 FF02` 复制/校验）先发 `0x78`
- Flash 抽象：`C/ota_flash.c` / `H/ota_flash.h`
  - 512 字节扇区擦除（带延时与首末字回读）、32 位字编程（跨调用暂存不足 4 字节）
  - CRC32（多项式 `0xEDB88320`）、范围保护（仅 APP / Backup）
  - `OTA_IsAppValid()` 检查栈指针与复位向量
  - `OTA_SaveInfo()` 擦 INFO 后写 version / date / size / crc，`request_flag` 保持 0
- PC 端刷写脚本：`tools/uds_flash.py`（依赖 `python-can`）
  - 标准流程 `10 02 → 27 → 34 → 36… → 37 → 31 01 FF01 → 31 01 FF02`
  - `--raw` 可直接发送任意 UDS 请求，便于分段/链路调试

### Changed
- `C/main.C`：UART 自定义 OTA 改为 INFO 标志触发 + CAN/UDS 下载循环
  - `request_flag == 0xAA55AA55` 或 APP 无效时进入 UDS 服务，否则直接跳 APP
  - `SystemInit()` 只设置 `SCB->VTOR = 0x08D00000`
- `C/boot.c`：改用 `ota_flash.h`；`Jump_To_App()` 在跳转前调用 `OTA_IsAppValid()`
- `C/Uart.c`：`GPIO_Config()` 不再把 PC6（CAN_RX）配置为输出并驱动高，避免与收发器 RXD 冲突
- `startup_w1251.s`：IRQ 28 向量改为 `CAN_IRQHandler`（仅 1 行改动）
- `H/W1251.h`：`CAN_TypeDef` 增加 `CAN_SADDR`（偏移 `0xD8`）；`CAN_RBUF0..15` / `CAN_TBUF0..15` 改为 `CAN_RBUF[16]` / `CAN_TBUF[16]` 数组
- `Driver/inc/SC32F10xx_rcc.h`：新增 `RCC_AHBPeriph_CAN`（`AHB_CFG` bit4）
- `Test.uvprojx`：加入 `boot_can.c`、`isotp.c`、`uds.c`、`ota_flash.c`、`SC32F11xx_can.c`；`ota.c` 不再参与编译

### Fixed
- 修正 `main.C` 中升级标志被 `if (1)` 跳过的问题
- 修正 `OTA_SaveInfo()` 早于复制完成、导致标志提前清除的问题（现由 `FF02` 成功后统一写 INFO）
- 修正 `OTA_Flash_EraseRange()` 未清理暂存字、可能把上一次残留在新下载中编程的问题
- 修正 ISO-TP 首帧 `dlc < 2` 时读取 `data[1]` 越界的问题
- 环形缓冲满时的注释与实际行为一致（丢弃当前帧，不覆盖已缓冲帧）
- 修正 CAN 发送/接收只处理 8 字节时仍按 4 个 32 位字访问的问题（经典 CAN 只需 2 个字，原写法会越界读写 `data[8]`）

### Removed
- UART 自定义文件长度 / 512 字节分块 ACK 收包流程（UART 仅保留调试输出）
- `C/ota.c` 参与编译（文件保留作参考）

### Notes
- 上板前需确认 HIRC 实际频率、PC6 是否确为 CAN_RX、CAN 收发器与 120 Ω 终端
- 若实际 HIRC 为 33.3 MHz，按 `H/boot_can.h` 注释改用 `PRESC=5, SEG1=7, SEG2=1`
- `0x27` 当前为固定 key，后续可替换为真正的 seed/key 算法
- `0x34` 的地址参数必须是 `0x08000000`，数据实际写入 Backup 区，`FF02` 再复制到 APP

---

## [Unreleased] - 2026-07-14

### Added
- 新增 `C/iap.c` 和 `H/iap.h` — IAP (In-Application Programming) 模块，支持 Flash 在线编程
  - `IAP_Unlock()` — IAP 解锁
  - `IAP_Erase512B()` — 按 512 字节扇区擦除
  - `IAP_EraseAll()` — 全片擦除
  - `IAP_UnlockTest()` — IAP 锁定状态检测
  - `IAP_WriteCmd()` — 使能/关闭 Flash 编程模式
  - `IAP_SERASE()` — 扇区擦除（带地址偏移）
  - `IAP_WriteData_Byte()` — 按字节写入
  - `IAP_WriteData_HalfWord()` — 按半字写入
  - `IAP_WriteData_Word()` — 按字写入
  - `IAP_ReadData_Byte()` — 按字节读取
  - `IAP_ReadData_HalfWord()` — 按半字读取
  - `IAP_ReadData_Word()` — 按字读取

### Fixed
- 修复 `H/iap.h` 缺少 `<stdint.h>` 包含，导致 `uint32_t`/`uint8_t`/`uint16_t` 类型未定义
- 修复 `C/iap.c` 缺少 `"W1251.h"` 包含，导致 `IAP` 外设宏未定义
- 修复 `C/iap.c` `IAP_Unlock()` 函数中多余的右括号语法错误
- 修复 `C/iap.c` `IAP_Unlock()` 函数缺少闭合花括号
- 修复 `C/main.C` 中未使用的变量 `a` 警告
- 修复 `H/iap.h` 缺少 `IAP_WriteCmd()` 函数声明
- 修复 `H/iap.h` 声明但 `C/iap.c` 未实现的函数：`IAP_Erase512B`、`IAP_EraseAll`、`IAP_UnlockTest`、`IAP_WriteData_Byte/HalfWord/Word`、`IAP_ReadData_Byte/HalfWord/Word`
- 修复 `IAP_WriteCmd` 的 iap.h 声明 (`uint8_t`) 与 iap.c 定义 (`FunctionalState`) 类型不一致问题

---

## [c3154ea] - uart_finish

### Added
- UART 通信功能完成
- 基础项目结构（CMSIS、Driver、启动文件）
- 外设驱动：GPIO、RCC、WDT
- W1251 芯片头文件及寄存器定义
