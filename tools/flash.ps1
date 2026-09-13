# Fast flash for the SP01 ESP32-S3 Windows bundle.
# Usage:
#   .\flash.ps1
#   .\flash.ps1 -Port COM3

param([string]$Port = "")

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($Port)) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    if ($ports -contains "COM3") {
        $Port = "COM3"
    } elseif ($ports.Count -eq 1) {
        $Port = $ports[0]
    } else {
        try {
            $usb = Get-CimInstance Win32_SerialPort | Where-Object {
                $_.Name -match "USB Serial Device" -or $_.Description -match "USB Serial"
            }
            if (@($usb).Count -eq 1) { $Port = @($usb)[0].DeviceID }
        } catch {}
    }
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    throw "Cannot choose serial port automatically. Run .\flash.ps1 -Port COMx"
}

Write-Host "SP01 fast flash -> $Port"
if (Test-Path .\GIT_SHA) {
    $sha = (Get-Content .\GIT_SHA).Trim()
    Write-Host "Bundle commit: $sha"
}

python -m esptool version *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Installing esptool once..."
    python -m pip install --user esptool
}

python -m esptool --chip esp32s3 -p $Port write-flash "@flash_args"
if ($LASTEXITCODE -ne 0) { throw "Flash failed" }

Write-Host ""
Write-Host "FLASH OK: $Port"
Write-Host "Open serial console at 115200 to confirm the new build SHA/banner."
