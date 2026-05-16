$ErrorActionPreference = "Stop"

$serviceName = "Practice2Service"
$displayName = "Practice2 Service"

$root = Split-Path -Parent $PSScriptRoot

$possiblePaths = @(
    (Join-Path -Path $root -ChildPath "x64\Debug\Practice2.exe"),
    (Join-Path -Path $root -ChildPath "Practice2\x64\Debug\Practice2.exe"),
    (Join-Path -Path $root -ChildPath "x64\Release\Practice2.exe"),
    (Join-Path -Path $root -ChildPath "Practice2\x64\Release\Practice2.exe")
)

$serviceExe = $possiblePaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $serviceExe) {
    Write-Host "Practice2.exe was not found. Build Practice2 project first." -ForegroundColor Red
    Write-Host "Checked paths:"
    $possiblePaths | ForEach-Object { Write-Host $_ }
    exit 1
}

Write-Host "Service exe found:"
Write-Host $serviceExe

$existing = Get-Service -Name $serviceName -ErrorAction SilentlyContinue

if ($existing) {
    Write-Host "Service already exists. Removing old service..."

    $guiProcesses = Get-Process -Name "Practice2Gui" -ErrorAction SilentlyContinue
    if ($guiProcesses) {
        Write-Host "Stopping Practice2Gui.exe process..."
        $guiProcesses | Stop-Process -Force
        Start-Sleep -Seconds 1
    }

    $serviceProcesses = Get-Process -Name "Practice2" -ErrorAction SilentlyContinue
    if ($serviceProcesses) {
        Write-Host "Stopping Practice2.exe process..."
        $serviceProcesses | Stop-Process -Force
        Start-Sleep -Seconds 2
    }

    sc.exe delete $serviceName | Out-Host
    Start-Sleep -Seconds 2
}

Write-Host "Creating service..."

sc.exe create $serviceName `
    binPath= "`"$serviceExe`"" `
    DisplayName= "$displayName" `
    start= demand | Out-Host

Write-Host "Starting service..."

Start-Service -Name $serviceName

Start-Sleep -Seconds 2

sc.exe query $serviceName

Write-Host "Service installed and started successfully." -ForegroundColor Green