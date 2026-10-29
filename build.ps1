param(
    [switch]$Rebuild,
    [switch]$Clean,
    [string]$MakePath = $env:MAKE
)
$ErrorActionPreference = 'Stop'
if ($Clean -and $Rebuild) { throw 'Choose -Clean or -Rebuild.' }
if (-not $MakePath) {
    $fallback = 'C:\Program Files (x86)\GnuWin32\bin\make.exe'
    if (Test-Path -LiteralPath $fallback) { $MakePath = $fallback }
    else {
        $command = Get-Command make, mingw32-make -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $command) { throw 'GNU Make must be installed and available in PATH.' }
        $MakePath = $command.Source
    }
}
if (-not (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)) { throw 'ARM GNU toolchain must be in PATH.' }
if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw 'Python 3.10 or newer must be in PATH.' }
$target = if ($Clean) { 'clean' } elseif ($Rebuild) { 'rebuild' } else { 'all' }
Push-Location $PSScriptRoot
try {
    & $MakePath $target
    if ($LASTEXITCODE -ne 0) { throw "Firmware build failed (exit $LASTEXITCODE)." }
} finally { Pop-Location }
if (-not $Clean) { Write-Host 'Built: build/bench/motor-bench.elf, motor-bench.hex, motor-bench.bin' }
