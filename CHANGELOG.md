# CHANGELOG

本文档记录 W1251 OTA 项目的所有重要变更。

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
