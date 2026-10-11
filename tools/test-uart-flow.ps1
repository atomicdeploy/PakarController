[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern uart-flow-control -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/serial_link_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
foreach($taskFlags in @(@('-DPAKAR_USE_BUSY_WIRE=1','-DPAKAR_UART_BUFFERED=0'),
    @('-DPAKAR_USE_BUSY_WIRE=0','-DPAKAR_UART_BUFFERED=0'),
    @('-DPAKAR_USE_BUSY_WIRE=0','-DPAKAR_UART_BUFFERED=1','-DESP32'))){
  & g++ -std=c++17 -Wall -Wextra -Werror @taskFlags -I (Join-Path $taskRoot 'tests/serial_stubs') -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/serial_link_test.cpp') -o $taskOutput
  if($LASTEXITCODE){throw 'SerialLink profile compilation failed'}
  & $taskOutput
  if($LASTEXITCODE){throw 'SerialLink profile assertions failed'}
}
