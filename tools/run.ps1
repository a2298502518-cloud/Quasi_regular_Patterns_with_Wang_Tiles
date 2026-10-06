param([Parameter(ValueFromRemainingArguments=$true)][string[]]$GenerationArgs)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$candidates = @('.venv/Scripts/python.exe','.codex-temp/joint-field-venv/Scripts/python.exe')
$interpreter = $candidates | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $interpreter) { throw '请先按 README 创建 .venv 并安装 tools/requirements.txt。' }
& $interpreter -X utf8 (Join-Path $PSScriptRoot 'motif_connection_study.py') @GenerationArgs
exit $LASTEXITCODE
