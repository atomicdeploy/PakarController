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
$taskOutput=Join-Path $taskRoot 'build/native/host_frame_ingress_test.exe'
foreach($taskHostPlatform in @('generic','esp8266')){
  $taskHostFlags=@()
  if($taskHostPlatform -eq 'esp8266'){$taskHostFlags=@('-DARDUINO_ARCH_ESP8266')}
  & g++ -std=c++17 -Wall -Wextra -Werror @taskHostFlags -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/host_frame_ingress_test.cpp') -o $taskOutput
  if($LASTEXITCODE){throw "Host ingress compilation failed ($taskHostPlatform)"}
  & $taskOutput
  if($LASTEXITCODE){throw "Host ingress assertions failed ($taskHostPlatform)"}
}
& node (Join-Path $taskRoot 'tests/host_frame_ingress_wiring_test.mjs')
if($LASTEXITCODE){throw 'Host ingress wiring assertions failed'}
$taskOutput=Join-Path $taskRoot 'build/native/avr_busy_input_test.exe'
foreach($taskPlatform in @('generic','avr-registers')){
  $taskFlags=@()
  if($taskPlatform -eq 'avr-registers'){$taskFlags=@('-D__AVR__')}
  & g++ -std=c++17 -Wall -Wextra -Werror @taskFlags -I (Join-Path $taskRoot 'tests/pc_stubs') (Join-Path $taskRoot 'tests/avr_busy_input_test.cpp') -o $taskOutput
  if($LASTEXITCODE){throw "AVR busy input compilation failed ($taskPlatform)"}
  & $taskOutput
  if($LASTEXITCODE){throw "AVR busy input assertions failed ($taskPlatform)"}
}
