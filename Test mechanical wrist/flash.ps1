param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9]+$')][string]$SerialNumber,
    [string]$ProgrammerPath = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$image = Join-Path $PSScriptRoot 'build\wrist-bench.elf'
if (-not (Test-Path -LiteralPath $image)) { throw 'Run .\build.ps1 first.' }
$arguments = @('-c', 'port=SWD', "sn=$SerialNumber", '-w', $image, '-v', '-rst')
if ($DryRun) { Write-Output $ProgrammerPath; Write-Output $arguments; return }
& $ProgrammerPath @arguments
if ($LASTEXITCODE -ne 0) { throw "Programming failed ($LASTEXITCODE)." }
