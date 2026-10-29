# Manual operator utility. Never called by build.ps1 or the PC monitor.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9]+$')][string]$SerialNumber,
    [string]$ProgrammerPath = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$image = Join-Path $PSScriptRoot 'build\bench\motor-bench.elf'
if (-not (Test-Path -LiteralPath $image)) { throw 'Build the bench first with .\build.ps1.' }
$arguments = @('-c', 'port=SWD', "sn=$SerialNumber", '-w', $image, '-v', '-rst')
if ($DryRun) {
    Write-Host "Manual programming command: $ProgrammerPath"
    Write-Output $arguments
    return
}
if (-not (Test-Path -LiteralPath $ProgrammerPath)) { throw 'STM32CubeProgrammer CLI was not found.' }
& $ProgrammerPath @arguments
if ($LASTEXITCODE -ne 0) { throw "Programming failed (exit $LASTEXITCODE)." }
