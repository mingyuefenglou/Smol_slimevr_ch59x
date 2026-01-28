# SlimeVR CH59X 完整编译脚本 (PowerShell)
# 编译 Bootloader + Tracker + Receiver，并合并 HEX 文件

param(
    [string]$Target = "all",
    [string]$Chip = "CH592",
    [string]$Version = "0.4.2",
    [switch]$Clean
)

# 工具链路径
$ToolchainPath = "C:\Users\ROG\Documents\xpack-riscv-none-elf-gcc-15.2.0-1\bin"
if (-not (Test-Path "$ToolchainPath\riscv-none-elf-gcc.exe")) {
    Write-Host "[ERROR] 未找到工具链: $ToolchainPath" -ForegroundColor Red
    exit 1
}

# 设置环境变量
$env:PATH = "$ToolchainPath;C:\mingw64\bin;$env:PATH"
$env:CROSS_COMPILE = "riscv-none-elf-"

# 项目目录
$ProjectDir = $PSScriptRoot
$BootloaderDir = "$ProjectDir\bootloader"
$OutputDir = "$ProjectDir\output"
$ToolsDir = "$ProjectDir\tools"

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "SlimeVR CH59X 完整编译脚本" -ForegroundColor Cyan
Write-Host "版本: v$Version | 芯片: $Chip | 目标: $Target" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan

# 清理
if ($Clean) {
    Write-Host "[CLEAN] 清理构建文件..." -ForegroundColor Yellow
    if (Test-Path "$ProjectDir\build") {
        Remove-Item -Recurse -Force "$ProjectDir\build"
    }
    if (Test-Path "$BootloaderDir\build") {
        Remove-Item -Recurse -Force "$BootloaderDir\build"
    }
}

# 创建输出目录
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

#==============================================================================
# 编译 Bootloader
#==============================================================================
Write-Host "`n[1/3] 编译 Bootloader..." -ForegroundColor Green
Push-Location $BootloaderDir

try {
    # 编译 bootloader
    $chipLower = $Chip.ToLower()
    & make CHIP=$Chip PREFIX=riscv-none-elf- 2>&1 | Out-Null
    
    $bootHex = "output\bootloader_$chipLower.hex"
    $bootBin = "output\bootloader_$chipLower.bin"
    
    if (Test-Path $bootHex) {
        $bootSize = (Get-Item $bootBin).Length
        Write-Host "  [OK] Bootloader 编译成功 ($bootSize bytes)" -ForegroundColor Green
        
        if ($bootSize -gt 4096) {
            Write-Host "  [ERROR] Bootloader 超过 4KB 限制!" -ForegroundColor Red
            exit 1
        }
        
        $BootHexPath = (Resolve-Path $bootHex).Path
    } else {
        Write-Host "  [ERROR] Bootloader 编译失败" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

#==============================================================================
# 编译 Tracker 和 Receiver
#==============================================================================
Push-Location $ProjectDir

try {
    # 编译 Tracker
    if ($Target -eq "all" -or $Target -eq "tracker") {
        Write-Host "`n[2/3] 编译 Tracker..." -ForegroundColor Green
        & make TARGET=tracker CHIP=$Chip VERSION=$Version CROSS_COMPILE=riscv-none-elf- 2>&1 | Out-Null
        
        $trackerHex = "output\tracker_${Chip}_v${Version}.hex"
        if (Test-Path $trackerHex) {
            Write-Host "  [OK] Tracker 编译成功" -ForegroundColor Green
            $TrackerHexPath = (Resolve-Path $trackerHex).Path
        } else {
            Write-Host "  [ERROR] Tracker 编译失败" -ForegroundColor Red
            exit 1
        }
    }
    
    # 编译 Receiver
    if ($Target -eq "all" -or $Target -eq "receiver") {
        Write-Host "`n[2/3] 编译 Receiver..." -ForegroundColor Green
        & make TARGET=receiver CHIP=$Chip VERSION=$Version CROSS_COMPILE=riscv-none-elf- 2>&1 | Out-Null
        
        $receiverHex = "output\receiver_${Chip}_v${Version}.hex"
        if (Test-Path $receiverHex) {
            Write-Host "  [OK] Receiver 编译成功" -ForegroundColor Green
            $ReceiverHexPath = (Resolve-Path $receiverHex).Path
        } else {
            Write-Host "  [ERROR] Receiver 编译失败" -ForegroundColor Red
            exit 1
        }
    }
} finally {
    Pop-Location
}

#==============================================================================
# 合并 HEX 文件
#==============================================================================
Write-Host "`n[3/3] 合并 HEX 文件..." -ForegroundColor Green

$mergeTool = "$ToolsDir\merge_hex.py"
if (-not (Test-Path $mergeTool)) {
    Write-Host "  [ERROR] 未找到合并工具: $mergeTool" -ForegroundColor Red
    exit 1
}

# 合并 Tracker
if ($Target -eq "all" -or $Target -eq "tracker") {
    if ($TrackerHexPath -and $BootHexPath) {
        $trackerCombined = "$OutputDir\tracker_${Chip}_v${Version}_combined.hex"
        & python $mergeTool $BootHexPath $TrackerHexPath -o $trackerCombined
        if (Test-Path $trackerCombined) {
            Write-Host "  [OK] Tracker 合并完成: tracker_${Chip}_v${Version}_combined.hex" -ForegroundColor Green
        }
    }
}

# 合并 Receiver
if ($Target -eq "all" -or $Target -eq "receiver") {
    if ($ReceiverHexPath -and $BootHexPath) {
        $receiverCombined = "$OutputDir\receiver_${Chip}_v${Version}_combined.hex"
        & python $mergeTool $BootHexPath $ReceiverHexPath -o $receiverCombined
        if (Test-Path $receiverCombined) {
            Write-Host "  [OK] Receiver 合并完成: receiver_${Chip}_v${Version}_combined.hex" -ForegroundColor Green
        }
    }
}

#==============================================================================
# 显示摘要
#==============================================================================
Write-Host "`n==============================================" -ForegroundColor Cyan
Write-Host "编译完成！" -ForegroundColor Green
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Output files:" -ForegroundColor Yellow

if ($Target -eq "all" -or $Target -eq "tracker") {
    $trackerFile = "$OutputDir\tracker_${Chip}_v${Version}_combined.hex"
    if (Test-Path $trackerFile) {
        $size = (Get-Item $trackerFile).Length
        $sizeKB = [math]::Round($size/1KB, 2)
        Write-Host "  - tracker_${Chip}_v${Version}_combined.hex" -ForegroundColor White
        Write-Host "    Size: $sizeKB KB" -ForegroundColor Gray
    }
}

if ($Target -eq "all" -or $Target -eq "receiver") {
    $receiverFile = "$OutputDir\receiver_${Chip}_v${Version}_combined.hex"
    if (Test-Path $receiverFile) {
        $size = (Get-Item $receiverFile).Length
        $sizeKB = [math]::Round($size/1KB, 2)
        Write-Host "  - receiver_${Chip}_v${Version}_combined.hex" -ForegroundColor White
        Write-Host "    Size: $sizeKB KB" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "File location: $OutputDir" -ForegroundColor Yellow
Write-Host "==============================================" -ForegroundColor Cyan
