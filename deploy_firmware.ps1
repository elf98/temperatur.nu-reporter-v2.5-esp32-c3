# Deployment script för Temperatur.nu Reporter firmware
#
# Användning:
#   1. Bumpa FW_VERSION i temperatur_reporter_esp32c3.ino
#   2. Arduino IDE: Sketch -> Export Compiled Binary (Ctrl+Alt+S)
#   3. Kör: .\deploy_firmware.ps1
#
# Skriptet:
#   - Läser version från .ino
#   - Hittar .ino.bin från senaste build
#   - Laddar upp som firmware-<version>.bin
#   - Uppdaterar version.txt
#
# Enheter ute hämtar uppdateringen vid nästa OTA-check.

$ErrorActionPreference = "Stop"

$Server     = "erik@159.89.22.162"
$RemoteBase = "/var/www/report.temperatur.nu/firmware/esp32c3"
$LocalDir   = $PSScriptRoot
$InoFile    = Join-Path $LocalDir "temperatur_reporter_esp32c3.ino"

function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info    { Write-Host $args -ForegroundColor Cyan }
function Write-Err     { Write-Host $args -ForegroundColor Red }

# 1. Läs FW_VERSION från .ino
$versionLine = (Select-String -Path $InoFile -Pattern '^\s*#define\s+FW_VERSION\s+"([^"]+)"' | Select-Object -First 1)
if (-not $versionLine) {
    Write-Err "Kunde inte hitta FW_VERSION i $InoFile"
    exit 1
}
$Version = $versionLine.Matches.Groups[1].Value
Write-Info "FW_VERSION = $Version"

# 2. Hitta build-output (.ino.bin)
# Arduino IDE 2.x lägger Export Compiled Binary i sketch-mappen som "build/<board>/<sketch>.ino.bin"
# eller direkt i sketch-mappen beroende på inställning.
$BinCandidates = @(
    (Join-Path $LocalDir "build/esp32.esp32.esp32c3/temperatur_reporter_esp32c3.ino.bin"),
    (Join-Path $LocalDir "temperatur_reporter_esp32c3.ino.esp32c3.bin"),
    (Join-Path $LocalDir "temperatur_reporter_esp32c3.ino.bin")
)
$BinFile = $BinCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $BinFile) {
    Write-Err "Kunde inte hitta .ino.bin. Kör Sketch -> Export Compiled Binary först."
    Write-Info "Letade i:"
    $BinCandidates | ForEach-Object { Write-Info "  $_" }
    exit 1
}
Write-Info "Bin-fil:    $BinFile"
Write-Info "Storlek:    $([math]::Round((Get-Item $BinFile).Length / 1024, 1)) kB"

# 3. Confirm
Write-Host ""
Write-Host "Server:     $Server" -ForegroundColor Yellow
Write-Host "Path:       $RemoteBase" -ForegroundColor Yellow
Write-Host "Filnamn:    firmware-$Version.bin" -ForegroundColor Yellow
Write-Host "Och uppdaterar version.txt -> $Version" -ForegroundColor Yellow
Write-Host ""
$confirm = Read-Host "Fortsätta? (y/N)"
if ($confirm -ne "y") {
    Write-Info "Avbryter."
    exit 0
}

# 4. Upload bin
$RemoteBin = "$RemoteBase/firmware-$Version.bin"
Write-Info "`nLaddar upp bin..."
scp $BinFile "${Server}:${RemoteBin}"
if ($LASTEXITCODE -ne 0) {
    Write-Err "Upload misslyckades"
    exit 1
}

# 5. Uppdatera version.txt atomiskt (skriv via temp + mv)
Write-Info "Uppdaterar version.txt..."
ssh $Server "echo '$Version' > $RemoteBase/version.txt.tmp && mv $RemoteBase/version.txt.tmp $RemoteBase/version.txt && chmod 644 $RemoteBase/version.txt $RemoteBin"
if ($LASTEXITCODE -ne 0) {
    Write-Err "version.txt-uppdatering misslyckades"
    exit 1
}

# 6. Verifiera
Write-Info "`nVerifierar..."
$serverVersion = (Invoke-WebRequest -UseBasicParsing "http://report.temperatur.nu/firmware/esp32c3/version.txt").Content.Trim()
Write-Info "Servern rapporterar version: $serverVersion"

if ($serverVersion -eq $Version) {
    Write-Success "`n=== DEPLOYMENT KLAR ==="
    Write-Info "Enheter ute hämtar version $Version vid nästa OTA-check."
} else {
    Write-Err "Version-mismatch på server: '$serverVersion' (förväntade '$Version')"
    exit 1
}
