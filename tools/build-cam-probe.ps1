param(
    [string]$BuildPath = '',
    [string]$OutputPath = '',
    [switch]$UseUartSerial
)
$ErrorActionPreference = 'Stop'
$probeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildPath) { $BuildPath = Join-Path $probeRoot '.build-20261024-camera-bars-audit' }
if (-not $OutputPath) { $OutputPath = Join-Path $probeRoot 'validation\20261024-sccb-binary-audit\firmware' }
$probeEvidence = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $probeEvidence | Out-Null
$probeWrappers = @('SCCB_Init', 'SCCB_Read', 'SCCB_Write', 'i2c_new_master_bus', 'i2c_master_transmit', 'i2c_master_receive')
$probeLinkFlags = ($probeWrappers | ForEach-Object { '-Wl,--wrap=' + $_ }) -join ' '
$probeFqbn = 'esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,USBMode=hwcdc,CDCOnBoot=cdc'
if ($UseUartSerial) { $probeFqbn = $probeFqbn.Replace('CDCOnBoot=cdc', 'CDCOnBoot=default') }
$probeArguments = @('compile', '--jobs', '4', '--verbose', '--fqbn', $probeFqbn,
    '--build-path', $BuildPath, '--output-dir', $OutputPath,
    '--build-property', ('compiler.c.elf.extra_flags=' + $probeLinkFlags),
    (Join-Path $probeRoot 'diagnostics\CamProbe'))
@{ executable = (Get-Command arduino-cli.exe).Source; arguments = $probeArguments; flashes = $false } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $probeEvidence 'build-command.json') -Encoding UTF8
$probeLog = Join-Path $probeEvidence 'compile.log'
& arduino-cli.exe @probeArguments *> $probeLog
$probeCompileExit = $LASTEXITCODE
Get-Content -LiteralPath $probeLog -Tail 6
if ($probeCompileExit -ne 0) { throw "CamProbe compile failed: $probeCompileExit" }
