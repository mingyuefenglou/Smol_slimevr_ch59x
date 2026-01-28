# SlimeVR CH59X 编译脚本 (PowerShell)
# 用法: .\build.ps1 [-Target tracker|receiver|all] [-Chip CH591|CH592] [-Version x.y.z]

param(
    [string]$Target = "tracker",
    [string]$Chip = "CH592",
    [string]$Version = "0.4.2",
    [switch]$Clean
)

# 工具链路径
$ToolchainPath1 = "C:\Users\ROG\Documents\xpack-riscv-none-elf-gcc-15.2.0-1\bin"
$ToolchainPath2 = "C:\MounRiver\MounRiver_Studio\toolchain\RISC-V Embedded GCC\bin"

# 选择可用的工具链
$ToolchainPath = $null
if (Test-Path "$ToolchainPath1\riscv-none-elf-gcc.exe") {
    $ToolchainPath = $ToolchainPath1
    Write-Host "[INFO] 使用工具链: $ToolchainPath" -ForegroundColor Green
} elseif (Test-Path "$ToolchainPath2\riscv-none-elf-gcc.exe") {
    $ToolchainPath = $ToolchainPath2
    Write-Host "[INFO] 使用工具链: $ToolchainPath" -ForegroundColor Green
} else {
    Write-Host "[ERROR] 未找到 RISC-V 工具链!" -ForegroundColor Red
    exit 1
}

# 设置环境变量
$env:PATH = "$ToolchainPath;$env:PATH"
$env:CROSS_COMPILE = "riscv-none-elf-"

# 项目目录
$ProjectDir = $PSScriptRoot
$BuildDir = "$ProjectDir\build\$Target"
$OutputDir = "$ProjectDir\output"

# 清理
if ($Clean) {
    Write-Host "[CLEAN] 清理构建文件..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
}

# 创建目录
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# 编译器
$CC = "$ToolchainPath\riscv-none-elf-gcc.exe"
$AS = "$ToolchainPath\riscv-none-elf-gcc.exe"
$LD = "$ToolchainPath\riscv-none-elf-gcc.exe"
$OBJCOPY = "$ToolchainPath\riscv-none-elf-objcopy.exe"
$SIZE = "$ToolchainPath\riscv-none-elf-size.exe"

# 解析版本号
$VersionParts = $Version.Split('.')
$VersionMajor = if ($VersionParts.Length -gt 0) { $VersionParts[0] } else { "0" }
$VersionMinor = if ($VersionParts.Length -gt 1) { $VersionParts[1] } else { "4" }
$VersionPatch = if ($VersionParts.Length -gt 2) { $VersionParts[2] } else { "2" }

# 编译标志
$CPUFlags = "-march=rv32imac_zicsr_zifencei -mabi=ilp32 -msmall-data-limit=8"
$OptFlags = "-Os -ffunction-sections -fdata-sections -fno-common -fno-exceptions -fshort-enums"
$WarnFlags = "-Wall -Wextra -Wno-unused-parameter -Wno-unused-function"
$LTOFlags = "-flto"
$DepFlags = "-MMD -MP"

# 定义
$Defines = "-D$Chip -DCH59X -DVERSION_MAJOR=$VersionMajor -DVERSION_MINOR=$VersionMinor -DVERSION_PATCH=$VersionPatch"
if ($Chip -eq "CH591") {
    $Defines += " -DMAX_TRACKERS=16 -DFLASH_SIZE=256K -DRAM_SIZE=18K"
} else {
    $Defines += " -DMAX_TRACKERS=24 -DFLASH_SIZE=448K -DRAM_SIZE=26K"
}

if ($Target -eq "receiver") {
    $Defines += " -DBUILD_RECEIVER"
    $Project = "slimevr_receiver"
} else {
    $Defines += " -DBUILD_TRACKER"
    $Project = "slimevr_tracker"
}

# 包含路径
$Includes = "-Iinclude -Isrc -Isdk/StdPeriphDriver/inc -Isdk/RVMSIS -Isdk/BLE/LIB -Isdk/BLE/HAL/include"

# CFLAGS
$CFLAGS = "$CPUFlags $OptFlags $WarnFlags $Defines $Includes $LTOFlags $DepFlags"
$ASFLAGS = "$CPUFlags $Defines $Includes"

# 链接标志
$LDFLAGS = "$CPUFlags -Wl,--gc-sections -Wl,-Map=$BuildDir\$Project.map -Tsdk/Ld/Link.ld -nostartfiles --specs=nano.specs --specs=nosys.specs $LTOFlags -lm -u _printf_float"

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "SlimeVR CH59X 编译脚本" -ForegroundColor Cyan
Write-Host "版本: v$Version | 芯片: $Chip | 目标: $Target" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan

# 这里应该读取 Makefile 中的源文件列表，但为了简化，我们直接使用 make
# 实际上，最好的方法是直接调用 make，但确保环境正确

Write-Host "[INFO] 开始编译..." -ForegroundColor Green

# 使用 make 命令
$makeCmd = "make"
$makeArgs = @(
    "TARGET=$Target",
    "CHIP=$Chip",
    "VERSION=$Version",
    "CROSS_COMPILE=riscv-none-elf-"
)

# 执行 make
& $makeCmd $makeArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n==============================================" -ForegroundColor Green
    Write-Host "编译完成!" -ForegroundColor Green
    Write-Host "==============================================" -ForegroundColor Green
    Write-Host "输出文件:" -ForegroundColor Yellow
    if (Test-Path "$OutputDir\$Target`_$Chip`_v$Version.bin") {
        $size = (Get-Item "$OutputDir\$Target`_$Chip`_v$Version.bin").Length
        Write-Host "  BIN: $OutputDir\$Target`_$Chip`_v$Version.bin ($size bytes)" -ForegroundColor White
    }
    if (Test-Path "$OutputDir\$Target`_$Chip`_v$Version.hex") {
        Write-Host "  HEX: $OutputDir\$Target`_$Chip`_v$Version.hex" -ForegroundColor White
    }
    if (Test-Path "$OutputDir\$Target`_$Chip`_v$Version.uf2") {
        Write-Host "  UF2: $OutputDir\$Target`_$Chip`_v$Version.uf2" -ForegroundColor White
    }
} else {
    Write-Host "`n[ERROR] 编译失败!" -ForegroundColor Red
    exit 1
}
