$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$DepsRoot = Join-Path (Split-Path -Parent $ProjectRoot) ".deps"
$QtRoot = Join-Path $DepsRoot "Qt\6.5.3\mingw_64"
$MingwRoot = Join-Path $DepsRoot "QtTools\Tools\mingw1120_64"
$MysqlRoot = Join-Path $DepsRoot "mysql\Library"
$OpenSslRoot = "D:\anaconda\Library"
$CMakeExe = Join-Path $env:APPDATA "Python\Python312\Scripts\cmake.exe"

if (-not (Test-Path $CMakeExe)) {
    throw "cmake.exe not found at $CMakeExe. Run: python -m pip install --user cmake"
}

function Invoke-Native {
    & $args[0] @($args | Select-Object -Skip 1)
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $($args -join ' ')"
    }
}

$env:PATH = @(
    (Join-Path $MingwRoot "bin"),
    (Join-Path $QtRoot "bin"),
    (Split-Path -Parent $CMakeExe),
    $env:PATH
) -join ";"

$QtRootArg = $QtRoot -replace "\\", "/"
$MingwGxxArg = (Join-Path $MingwRoot "bin\g++.exe") -replace "\\", "/"
$MingwMakeArg = (Join-Path $MingwRoot "bin\mingw32-make.exe") -replace "\\", "/"
$MysqlRootArg = $MysqlRoot -replace "\\", "/"
$MysqlDllArg = (Join-Path $MysqlRoot "lib\libmysql.dll") -replace "\\", "/"
$OpenSslRootArg = $OpenSslRoot -replace "\\", "/"

Invoke-Native $CMakeExe -S $ProjectRoot -B (Join-Path $ProjectRoot "build") `
    -G "MinGW Makefiles" `
    "-DCMAKE_PREFIX_PATH=$QtRootArg" `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_CXX_COMPILER=$MingwGxxArg" `
    "-DCMAKE_MAKE_PROGRAM=$MingwMakeArg" `
    "-DMYSQL_ROOT=$MysqlRootArg" `
    "-DMYSQL_DLL=$MysqlDllArg" `
    "-DOPENSSL_ROOT=$OpenSslRootArg"

Invoke-Native $CMakeExe --build (Join-Path $ProjectRoot "build") --config Release -j 4
