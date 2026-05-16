$ErrorActionPreference = "Continue"

$serviceName = "Practice2Service"

Write-Host "Removing service..."

$service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue

if ($service) {
    Write-Host "Service found."

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

    Write-Host "Service removed." -ForegroundColor Green
}
else {
    Write-Host "Service was not found."
}