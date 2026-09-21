# Laptop launcher (Windows). Examples:
#   .\run.ps1 -Sim                                   # dashboard only, no robot
#   .\run.ps1 -Camera http://192.168.4.3:8080/video  # phone running IP Webcam
#   .\run.ps1 -Camera push -EnableMotors -DemoForward -AllowTurns
param(
    [switch]$Sim,
    [string]$Camera = "push",
    [string]$RobotUrl = "http://192.168.4.1",
    [switch]$EnableMotors,
    [switch]$DemoForward,
    [switch]$AllowTurns,
    [int]$Port = 8642
)
$py = Join-Path $PSScriptRoot "..\.venv\Scripts\python.exe"
$bridgeArgs = @("$PSScriptRoot\fruitfly_bridge.py", "--camera", $Camera, "--robot-url", $RobotUrl, "--dashboard-port", $Port)
if ($Sim) { $bridgeArgs += "--sim-robot" }
if ($EnableMotors) { $bridgeArgs += "--enable-motors" }
if ($DemoForward) { $bridgeArgs += "--demo-forward" }
if ($AllowTurns) { $bridgeArgs += "--allow-turns" }
& $py @bridgeArgs
