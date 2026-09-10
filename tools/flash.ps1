# Flash an SP01 ESP32-S3 bundle from Windows PowerShell.
#
#   .\flash.ps1 -Port COM3
#
# The quotes around "@flash_args" are required. On PowerShell, @ is the
# splatting operator: an unquoted @flash_args expands to an undefined variable
# and is dropped before esptool sees it. esptool then connects, configures the
# flash, resets the chip and exits reporting success, having written nothing.

param([Parameter(Mandatory = $true)][string]$Port)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

if (Test-Path .\GIT_SHA) {
    $sha = (Get-Content .\GIT_SHA).Trim()
    Write-Host "Bundle commit: $sha"
}

python -m esptool --chip esp32s3 -p $Port write-flash "@flash_args"

Write-Host ""
Write-Host "Flashed. Confirm the running image matches the commit above:"
Write-Host "  python tools\g1_usb_probe.py --port $Port --seconds 30"
Write-Host "A successful flash report is not evidence that the app partition changed."
