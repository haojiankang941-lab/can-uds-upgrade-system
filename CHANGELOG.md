# CHANGELOG

本文档记录 W1251 OTA 项目的所有重要变更。

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
