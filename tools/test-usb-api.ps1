param([string] $StatusCapture)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern remote-ota -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/usb_api_request_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'libraries/BoardCore/src') (Join-Path $taskRoot 'tests/usb_api_request_test.cpp') -o $taskOutput
if($LASTEXITCODE){throw 'USB API test compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'USB API assertions failed'}
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
$taskReceiptOutput=Join-Path $taskRoot 'build/native/diagnostic_receipt_test.exe'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'tests/access_stubs') -I $taskJson -I (Join-Path $taskRoot 'firmware/esp/src') (Join-Path $taskRoot 'tests/diagnostic_receipt_test.cpp') -o $taskReceiptOutput
if($LASTEXITCODE){throw 'USB diagnostic receipt compilation failed'}
if($StatusCapture){& $taskReceiptOutput $StatusCapture}else{& $taskReceiptOutput}
if($LASTEXITCODE){throw 'USB diagnostic receipt assertions failed'}
