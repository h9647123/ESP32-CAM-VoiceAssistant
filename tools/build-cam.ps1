param(
    [string]$BuildPath = '',
    [string]$OutputPath = '',
    [int]$Jobs = 4,
    [switch]$UsbCdcOnBoot
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildPath) { $BuildPath = Join-Path $projectRoot '.build-draft2' }
if (-not $OutputPath) { $OutputPath = Join-Path $projectRoot 'validation\20261001-draft2\firmware' }
$validationPath = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $validationPath -Force | Out-Null
$fqbn = 'esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,USBMode=hwcdc,CDCOnBoot=default'
if ($UsbCdcOnBoot) { $fqbn = $fqbn.Replace('CDCOnBoot=default', 'CDCOnBoot=cdc') }
$cli = (Get-Command arduino-cli.exe -ErrorAction Stop).Source
$sketch = Join-Path $projectRoot 'VoiceAssistant'
$logPath = Join-Path $validationPath 'compile.log'
$arguments = @('compile', '--jobs', [string]$Jobs, '--fqbn', $fqbn,
    '--build-path', $BuildPath, '--output-dir', $OutputPath, $sketch)
@{
    cli = $cli
    arguments = $arguments
    sketch = $sketch
    build = $BuildPath
    output = $OutputPath
    started = (Get-Date).ToString('o')
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $validationPath 'build-command.json') -Encoding UTF8
# Stop immediately on a native compiler failure, while preserving the full log.
$previousErrorAction = $ErrorActionPreference
try {
    # Windows PowerShell 5 treats native stderr as ErrorRecord objects. Preserve
    # diagnostics and let the native exit code decide whether compilation failed.
    $ErrorActionPreference = 'Continue'
    & $cli @arguments *> $logPath
    $compileExit = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $previousErrorAction
}
Get-Content -LiteralPath $logPath -Tail 50
if ($compileExit -ne 0) { throw "CAM compile failed (exit $compileExit); see $logPath" }
Get-ChildItem -LiteralPath $OutputPath -Filter '*.bin' |
    Get-FileHash -Algorithm SHA256 | Select-Object Path, Hash |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $validationPath 'firmware-hashes.json') -Encoding UTF8
Write-Host "CAM build passed. Firmware: $OutputPath"
