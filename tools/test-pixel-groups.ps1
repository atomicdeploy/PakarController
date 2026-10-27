$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern firmware-hardware -Summary | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native/pixel_group_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskOutput) | Out-Null
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I "$taskRoot/libraries/BoardCore/src" "$taskRoot/tests/pixel_group_layout_test.cpp" -o $taskOutput
if($LASTEXITCODE){throw 'Pixel group topology compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Pixel group topology assertions failed'}
$taskJson=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino/libraries/ArduinoJson/src'
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I "$taskRoot/tests/correction_stubs" -I "$taskRoot/tests/access_stubs" -I $taskJson -I "$taskRoot/libraries/BoardCore/src" -I "$taskRoot/firmware/esp/src" "$taskRoot/tests/pixel_group_store_test.cpp" -o $taskOutput
if($LASTEXITCODE){throw 'Pixel group persistence compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'Pixel group persistence assertions failed'}
foreach($taskRole in @(1,2)){
  & g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-DPAKAR_GAME_MODE=$taskRole" -I "$taskRoot/tests/pixel_stubs" -I "$taskRoot/libraries/BoardCore/src" -I "$taskRoot/firmware/esp/src" "$taskRoot/tests/pixel_group_renderer_test.cpp" "$taskRoot/firmware/esp/src/LedGrid.cpp" -o $taskOutput
  if($LASTEXITCODE){throw "Pixel group role $taskRole renderer compilation failed"}
  & $taskOutput
  if($LASTEXITCODE){throw "Pixel group role $taskRole renderer assertions failed"}
}
& g++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -I "$taskRoot/tests/access_stubs" -I "$taskRoot/firmware/esp/src" "$taskRoot/tests/usb_snapshot_projection_test.cpp" -o $taskOutput
if($LASTEXITCODE){throw 'USB optional artwork projection compilation failed'}
& $taskOutput
if($LASTEXITCODE){throw 'USB optional artwork projection assertions failed'}
& node "$taskRoot/tests/pixel_group_routes_test.mjs"
if($LASTEXITCODE){throw 'Pixel group route integration assertions failed'}
