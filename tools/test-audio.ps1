[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'script-runtime.ps1')
& (Join-Path $PSScriptRoot 'consult-reuse.ps1') -Concern audio-attention -Gate | Out-Null
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOutput=Join-Path $taskRoot 'build/native'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskCommon=@('-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror','-I',(Join-Path $taskRoot 'libraries/BoardCore/src'))
$taskCases=@(
    @{Name='attention_cues_test';Sources=@('tests/attention_cues_test.cpp');Flags=@()},
    @{Name='tone_player_test';Sources=@('tests/tone_player_test.cpp','firmware/avr/TonePlayer.cpp','firmware/avr/BootMelody.cpp');Flags=@('-DF_CPU=16000000UL','-I',(Join-Path $taskRoot 'tests/tone_stubs'),'-I',(Join-Path $taskRoot 'firmware/avr'))},
    @{Name='pc_contract_test';Sources=@('tests/pc_contract_test.cpp','firmware/avr/PcControllerRuntime.cpp','firmware/avr/BoardSettings.cpp','firmware/avr/SevenSegments.cpp','libraries/BoardCore/src/pccontroller/MacroRing.cpp','libraries/BoardCore/src/pccontroller/Keys.cpp','libraries/BoardCore/src/pccontroller/ControllerEvents.cpp','libraries/BoardCore/src/pccontroller/AutomationStore.cpp');Flags=@('-I',(Join-Path $taskRoot 'tests/pc_stubs'),'-I',(Join-Path $taskRoot 'firmware/avr'))}
)
foreach($taskCase in $taskCases){
    $taskBinary=Join-Path $taskOutput ($taskCase.Name+'.exe')
    $taskArguments=$taskCommon+$taskCase.Flags+@($taskCase.Sources | ForEach-Object {Join-Path $taskRoot $_})+@('-o',$taskBinary)
    Write-PakarStep "Audio · $($taskCase.Name) · host doubles, no speaker/device"
    Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath 'g++' -Arguments $taskArguments
    Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath $taskBinary
    if($taskCase.Name -eq 'attention_cues_test'){
        Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath 'g++' -Arguments (@('-DARDUINO_ARCH_ESP8266','-pthread','-I',(Join-Path $taskRoot 'tests/atomic_stubs'))+$taskArguments)
        Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath $taskBinary
    }
}
Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath 'node' -Arguments @('tests/boot_audio_test.mjs')
Invoke-PakarNative -WorkingDirectory $taskRoot -FilePath 'node' -Arguments @('tests/esp_only_maintenance_test.mjs')
