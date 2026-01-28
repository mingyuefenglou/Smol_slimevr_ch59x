# SlimeVR CH59X 完整编译脚本
# 编译 Bootloader + Tracker + Receiver，并合并 HEX 文件

param(
    [string]$Chip = "CH592",
    [string]$Version = "0.4.2"
)

$ErrorActionPreference = "Stop"

# 工具链路径
$ToolchainPath = $null

$path1 = "C:\Users\ROG\Documents\xpack-riscv-none-elf-gcc-15.2.0-1\bin\riscv-none-elf-gcc.exe"
$path2 = "C:\MounRiver\MounRiver_Studio\toolchain\RISC-V Embedded GCC\bin\riscv-none-elf-gcc.exe"

if (Test-Path $path1) {
    $ToolchainPath = "C:\Users\ROG\Documents\xpack-riscv-none-elf-gcc-15.2.0-1\bin"
} elseif (Test-Path $path2) {
    $ToolchainPath = "C:\MounRiver\MounRiver_Studio\toolchain\RISC-V Embedded GCC\bin"
}

if (-not $ToolchainPath) {
    Write-Host "[ERROR] Toolchain not found!" -ForegroundColor Red
    exit 1
}
Write-Host "Using toolchain: $ToolchainPath" -ForegroundColor Gray

# 设置环境变量
$env:PATH = "$ToolchainPath;C:\mingw64\bin;$env:PATH"
$env:CROSS_COMPILE = "riscv-none-elf-"

# 项目目录
$ProjectDir = $PSScriptRoot
$BootloaderDir = "$ProjectDir\bootloader"
$OutputDir = "$ProjectDir\output"
$ToolsDir = "$ProjectDir\tools"

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "SlimeVR CH59X Complete Build Script" -ForegroundColor Green
Write-Host "Version: v$Version | Chip: $Chip" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan

#==============================================================================
# Step 1: 编译 Bootloader
#==============================================================================
Write-Host "`n[1/4] Compiling Bootloader..." -ForegroundColor Yellow
Push-Location $BootloaderDir
try {
    $chipLower = $Chip.ToLower()
    & make CHIP=$Chip PREFIX=riscv-none-elf- 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Bootloader compilation failed"
    }
    
    $bootHex = "output\bootloader_$chipLower.hex"
    if (Test-Path $bootHex) {
        $bootSize = (Get-Item "output\bootloader_$chipLower.bin").Length
        Write-Host "  [OK] Bootloader compiled ($bootSize bytes)" -ForegroundColor Green
        $BootHexPath = (Resolve-Path $bootHex).Path
    } else {
        throw "Bootloader HEX file not found"
    }
} catch {
    Write-Host "  [ERROR] $_" -ForegroundColor Red
    Pop-Location
    exit 1
} finally {
    Pop-Location
}

#==============================================================================
# Step 2: 编译 Tracker
#==============================================================================
Write-Host "`n[2/4] Compiling Tracker..." -ForegroundColor Yellow
Push-Location $ProjectDir
try {
    $output = & make TARGET=tracker CHIP=$Chip VERSION=$Version CROSS_COMPILE=riscv-none-elf- all 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host $output -ForegroundColor Red
        throw "Tracker compilation failed"
    }
    
    $trackerHex = "output\tracker_${Chip}_v${Version}.hex"
    if (Test-Path $trackerHex) {
        Write-Host "  [OK] Tracker compiled" -ForegroundColor Green
        $TrackerHexPath = (Resolve-Path $trackerHex).Path
    } else {
        throw "Tracker HEX file not found"
    }
} catch {
    Write-Host "  [ERROR] $_" -ForegroundColor Red
    Pop-Location
    exit 1
} finally {
    Pop-Location
}

#==============================================================================
# Step 3: 编译 Receiver
#==============================================================================
Write-Host "`n[3/4] Compiling Receiver..." -ForegroundColor Yellow
Push-Location $ProjectDir
try {
    $output = & make TARGET=receiver CHIP=$Chip VERSION=$Version CROSS_COMPILE=riscv-none-elf- all 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host $output -ForegroundColor Red
        throw "Receiver compilation failed"
    }
    
    $receiverHex = "output\receiver_${Chip}_v${Version}.hex"
    if (Test-Path $receiverHex) {
        Write-Host "  [OK] Receiver compiled" -ForegroundColor Green
        $ReceiverHexPath = (Resolve-Path $receiverHex).Path
    } else {
        throw "Receiver HEX file not found"
    }
} catch {
    Write-Host "  [ERROR] $_" -ForegroundColor Red
    Pop-Location
    exit 1
} finally {
    Pop-Location
}

#==============================================================================
# Step 4: 合并 HEX 文件
#==============================================================================
Write-Host "`n[4/4] Merging Bootloader with HEX files..." -ForegroundColor Yellow

$mergeTool = "$ToolsDir\merge_hex.py"
if (-not (Test-Path $mergeTool)) {
    Write-Host "  [ERROR] Merge tool not found: $mergeTool" -ForegroundColor Red
    exit 1
}

# 合并 Tracker
if ($TrackerHexPath -and $BootHexPath) {
    $trackerCombined = "$OutputDir\tracker_${Chip}_v${Version}_combined.hex"
    & python $mergeTool $BootHexPath $TrackerHexPath -o $trackerCombined
    if (Test-Path $trackerCombined) {
        $size = [math]::Round((Get-Item $trackerCombined).Length/1KB, 2)
        Write-Host "  [OK] Tracker merged: tracker_${Chip}_v${Version}_combined.hex ($size KB)" -ForegroundColor Green
    }
}

# 合并 Receiver
if ($ReceiverHexPath -and $BootHexPath) {
    $receiverCombined = "$OutputDir\receiver_${Chip}_v${Version}_combined.hex"
    & python $mergeTool $BootHexPath $ReceiverHexPath -o $receiverCombined
    if (Test-Path $receiverCombined) {
        $size = [math]::Round((Get-Item $receiverCombined).Length/1KB, 2)
        Write-Host "  [OK] Receiver merged: receiver_${Chip}_v${Version}_combined.hex ($size KB)" -ForegroundColor Green
    }
}

#==============================================================================
# 显示摘要
#==============================================================================
Write-Host "`n==============================================" -ForegroundColor Cyan
Write-Host "Build Complete!" -ForegroundColor Green
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "`nCombined HEX files (with Bootloader):" -ForegroundColor Yellow
Get-ChildItem -Path $OutputDir -Filter "*_combined.hex" | ForEach-Object {
    $size = [math]::Round($_.Length/1KB, 2)
    Write-Host "  - $($_.Name) ($size KB)" -ForegroundColor Green
}
Write-Host "`nAll firmware files:" -ForegroundColor Yellow
Get-ChildItem -Path $OutputDir | Where-Object { $_.Extension -in @('.bin','.hex','.uf2') } | 
    Sort-Object Extension, Name | ForEach-Object {
        $size = [math]::Round($_.Length/1KB, 2)
        $type = if ($_.Name -like "*_combined*") { "[MERGED]" }
                elseif ($_.Extension -eq ".uf2") { "[UF2]" }
                elseif ($_.Extension -eq ".hex") { "[HEX]" }
                else { "[BIN]" }
        Write-Host "  $type $($_.Name) - $size KB"
    }
Write-Host "`nLocation: $OutputDir" -ForegroundColor Gray
Write-Host "==============================================" -ForegroundColor Cyan
