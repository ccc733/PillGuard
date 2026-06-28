@echo off
REM 从 .elf 文件生成 .bin 文件（用于烧录）
set ELF_FILE=build\Debug\yao_he002.elf
set BIN_FILE=build\Debug\yao_he002.bin

if not exist %ELF_FILE% (
    echo [错误] 找不到 %ELF_FILE%
    echo 请先编译项目!
    exit /b 1
)

echo [生成] %ELF_FILE% → %BIN_FILE%
F:\arm\bin\arm-none-eabi-objcopy.exe -O binary %ELF_FILE% %BIN_FILE%

if %ERRORLEVEL% EQU 0 (
    echo [成功] %BIN_FILE% 已生成!
) else (
    echo [失败] 生成错误!
    exit /b 1
)
