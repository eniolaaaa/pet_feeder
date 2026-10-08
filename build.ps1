# Build (and optionally flash) the pet feeder project.
#   .\build.ps1                 debug build
#   .\build.ps1 -Release        release build
#   .\build.ps1 -Flash          debug build, then flash with LinkServer
#   .\build.ps1 -Clean          wipe the build folder first
param(
    [switch]$Release,
    [switch]$Flash,
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'

$tools  = Join-Path $env:USERPROFILE '.mcuxpressotools'
$sdk    = if ($env:MCUX_SDK_ROOT) { $env:MCUX_SDK_ROOT } else { "$env:USERPROFILE\Downloads\SDK_24_12_00_FRDM-MCXA153_FOR_MIC5_V10" }
$link   = (Get-ChildItem 'C:\NXP' -Directory -Filter 'LinkServer_*' | Sort-Object Name | Select-Object -Last 1).FullName

$env:ARMGCC_DIR = (Get-ChildItem $tools -Directory -Filter 'arm-gnu-toolchain-*-arm-none-eabi' | Select-Object -Last 1).FullName
$env:PATH = (Get-ChildItem $tools -Directory -Filter 'ninja-*' | Select-Object -Last 1).FullName + ";$env:PATH"

$type  = if ($Release) { 'release' } else { 'debug' }
$build = Join-Path $PSScriptRoot "build\$type"

if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

cmake -S "$PSScriptRoot\armgcc" -B $build -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$sdk/tools/cmake_toolchain_files/armgcc.cmake" `
    "-DSdkRootDirPath=$sdk" "-DCMAKE_BUILD_TYPE=$type" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if ($LASTEXITCODE) { exit $LASTEXITCODE }

cmake --build $build
if ($LASTEXITCODE) { exit $LASTEXITCODE }

if ($Flash) {
    & "$link\LinkServer.exe" flash MCXA153 load "$build\pet_feeder.elf"
    exit $LASTEXITCODE
}
