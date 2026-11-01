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
# Kanal för Partition Scheme "No FS 4MB" (sedan 2.6.0). Gamla esp32c3/ är fryst
# på 2.5.0 för enheter med Default 4 MB-tabellen - deploya aldrig dit.
$RemoteBase = "/var/www/report.temperatur.nu/firmware/esp32c3-nofs"
$PublicBase = "http://report.temperatur.nu/firmware/esp32c3-nofs"
$AppSlotSize = 0x1F0000   # ota_0/ota_1 i no_fs.csv
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

# 2b. Kontrollera att bygget gjordes med No FS-tabellen (ota_0 = $AppSlotSize).
# En bin byggd med fel schema kan inte bootas på enheterna efter OTA.
$PartFile = $BinFile -replace '\.bin$', '.partitions.bin'
if (-not (Test-Path $PartFile)) {
    Write-Err "Hittar inte $PartFile - kan inte verifiera Partition Scheme."
    exit 1
}
$pb = [System.IO.File]::ReadAllBytes($PartFile)
$ota0Size = $null
for ($i = 0; $i + 32 -le $pb.Length; $i += 32) {
    if ($pb[$i] -ne 0xAA -or $pb[$i + 1] -ne 0x50) { break }
    if ($pb[$i + 2] -eq 0x00 -and $pb[$i + 3] -eq 0x10) {   # app, ota_0
        $ota0Size = [BitConverter]::ToUInt32($pb, $i + 8)
    }
}
if ($ota0Size -ne $AppSlotSize) {
    Write-Err ("Fel Partition Scheme: ota_0 = 0x{0:X}, förväntade 0x{1:X}. Välj 'No FS 4MB (2MB APP x2)' och exportera igen." -f $ota0Size, $AppSlotSize)
    exit 1
}
if ((Get-Item $BinFile).Length -gt $AppSlotSize) {
    Write-Err "Bin-filen är större än app-partitionen ($AppSlotSize byte)."
    exit 1
}
Write-Info "Partition:  No FS (ota_0 = 0x$('{0:X}' -f $ota0Size)) OK"

# 2c. USB CDC On Boot maste vara Enabled, annars tar UART0 GPIO20 och 1-Wire dor.
# IDE:n aterstaller kortinstallningarna vid karnbyte (hande 1/11 2026 med 2.6.0).
$OptsFile = Join-Path (Split-Path $BinFile) "build.options.json"
if (-not (Test-Path $OptsFile)) {
    Write-Err "Hittar inte $OptsFile - kan inte verifiera USB CDC On Boot."
    exit 1
}
if ((Get-Content $OptsFile -Raw) -notmatch 'CDCOnBoot=cdc') {
    Write-Err "USB CDC On Boot ar inte Enabled. Satt Tools -> USB CDC On Boot -> Enabled och exportera igen."
    exit 1
}
Write-Info "USB CDC:    Enabled OK"

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
ssh $Server "mkdir -p $RemoteBase && chmod 755 $RemoteBase"
if ($LASTEXITCODE -ne 0) {
    Write-Err "Kunde inte skapa $RemoteBase"
    exit 1
}
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
$serverVersion = (Invoke-WebRequest -UseBasicParsing "$PublicBase/version.txt").Content.Trim()
Write-Info "Servern rapporterar version: $serverVersion"

if ($serverVersion -eq $Version) {
    Write-Success "`n=== DEPLOYMENT KLAR ==="
    Write-Info "Enheter ute hämtar version $Version vid nästa OTA-check."
} else {
    Write-Err "Version-mismatch på server: '$serverVersion' (förväntade '$Version')"
    exit 1
}
