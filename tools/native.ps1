param(
    [ValidateSet('build', 'run', 'test', 'generate')]
    [string]$Action = 'run',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ProgramArgs
)
$ErrorActionPreference = 'Stop'
$taskRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskBuild = Join-Path $taskRoot 'build'
& cmake -S $taskRoot -B $taskBuild -DQRP_BUILD_DESKTOP=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败' }
& cmake --build $taskBuild --config $Configuration --parallel 8
if ($LASTEXITCODE -ne 0) { throw 'C++ 构建失败' }
if ($Action -eq 'build') { exit 0 }
if ($Action -eq 'test') {
    & ctest --test-dir $taskBuild -C $Configuration --output-on-failure
} else {
    $taskName = if ($Action -eq 'generate') { 'qrp_generate' } else { 'qrp_workbench' }
    $taskExe = Join-Path $taskBuild "$Configuration/$taskName.exe"
    & $taskExe @ProgramArgs
}
if ($LASTEXITCODE -ne 0) { throw "原生入口执行失败：$LASTEXITCODE" }
