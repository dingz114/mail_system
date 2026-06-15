$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$DepsRoot = Join-Path (Split-Path -Parent $ProjectRoot) ".deps"
$MysqlEnv = Join-Path $DepsRoot "mysql"
$MysqlRoot = Join-Path $MysqlEnv "Library"
$MysqlData = Join-Path $DepsRoot "mysql-data"
$MysqldExe = Join-Path $MysqlRoot "bin\mysqld.exe"
$AppExe = Join-Path $ProjectRoot "build\MailSystem.exe"
$SchemaSql = Join-Path $ProjectRoot "sql\schema.sql"
$InitDatabase = Join-Path $ProjectRoot "scripts\init_database.py"

if (-not (Test-Path $MysqldExe)) {
    throw "mysqld.exe not found. Expected: $MysqldExe"
}
if (-not (Test-Path $AppExe)) {
    throw "MailSystem.exe not found. Run .\build.ps1 first."
}

$env:PATH = @(
    (Join-Path $MysqlRoot "bin"),
    $MysqlEnv,
    $env:PATH
) -join ";"

if (-not (Test-Path (Join-Path $MysqlData "mysql"))) {
    New-Item -ItemType Directory -Force $MysqlData | Out-Null
    & $MysqldExe --no-defaults --initialize-insecure `
        "--basedir=$($MysqlRoot -replace "\\", "/")" `
        "--datadir=$($MysqlData -replace "\\", "/")" `
        --log-error-verbosity=3
    if ($LASTEXITCODE -ne 0) {
        throw "MySQL data directory initialization failed with exit code $LASTEXITCODE."
    }
}

$listener = Get-NetTCPConnection -LocalPort 3306 -State Listen -ErrorAction SilentlyContinue |
    Select-Object -First 1

if (-not $listener) {
    Start-Process -FilePath $MysqldExe `
        -ArgumentList @(
            "--no-defaults",
            "--basedir=$($MysqlRoot -replace "\\", "/")",
            "--datadir=$($MysqlData -replace "\\", "/")",
            "--port=3306",
            "--bind-address=127.0.0.1",
            "--console"
        ) `
        -WorkingDirectory (Join-Path $MysqlRoot "bin") `
        -WindowStyle Hidden

    $ready = $false
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $client = New-Object Net.Sockets.TcpClient
        try {
            $client.Connect("127.0.0.1", 3306)
            $ready = $true
            break
        } catch {
        } finally {
            $client.Close()
        }
    }
    if (-not $ready) {
        throw "MySQL did not start on 127.0.0.1:3306."
    }
}

python -c "import pymysql" 2>$null
if ($LASTEXITCODE -ne 0) {
    python -m pip install --user PyMySQL
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install PyMySQL."
    }
}

python $InitDatabase $SchemaSql
if ($LASTEXITCODE -ne 0) {
    throw "Database initialization failed."
}

Start-Process -FilePath $AppExe -WorkingDirectory (Split-Path -Parent $AppExe)
