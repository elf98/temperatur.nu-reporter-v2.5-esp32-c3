# Flasha + testa en ny reporter (lopande band).
#
#   .\flash_test.ps1 -Port COM7 -Ssid "natet" -Pass "losen" [-Keep]
#
# 1. Laser MAC/PIN fran chippet, raderar NVS och flashar release-bygget i
#    build/esp32.esp32.esp32c3/ (bootloader, No FS-partitioner, boot_app0, app).
#    Har enheten redan exakt detta bygge (verify-flash) hoppas flashningen over.
# 2. Vantar pa portalen tnu_report, ansluter PC:ns WiFi-kort och skickar in
#    WiFi-uppgifterna via /wifisave.
# 3. Laser serial tills enheten rapporterat (HTTP 200) eller timeout.
#    Hittad givare som ger -127/85 = GIVARFEL.
# 4. Raderar NVS igen (om inte -Keep) sa enheten startar portalen hos kunden.
# Skriver en resultatrad till flash_log.csv bredvid skriptet (gitignorerad).

param(
    [Parameter(Mandatory)] [string]$Port,
    [Parameter(Mandatory)] [string]$Ssid,
    [Parameter(Mandatory)] [string]$Pass,
    [switch]$Keep,
    [int]$TimeoutSec = 150
)

$ErrorActionPreference = "Stop"
$Build   = Join-Path $PSScriptRoot "build/esp32.esp32.esp32c3"
$Name    = "temperatur_reporter_esp32c3.ino"
$Arduino = Join-Path $env:LOCALAPPDATA "Arduino15/packages/esp32"
$Esptool = Get-ChildItem (Join-Path $Arduino "tools/esptool_py") -Directory |
           Sort-Object { [version]$_.Name } | Select-Object -Last 1 |
           ForEach-Object { Join-Path $_.FullName "esptool.exe" }
$BootApp0 = Get-ChildItem (Join-Path $Arduino "hardware/esp32") -Directory |
            Sort-Object { [version]$_.Name } | Select-Object -Last 1 |
            ForEach-Object { Join-Path $_.FullName "tools/partitions/boot_app0.bin" }
$ApSsid  = "tnu_report"
$ApIp    = "8.8.8.8"
$LogFile = Join-Path $PSScriptRoot "flash_log.csv"

function Step($msg) { Write-Host ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $msg) -ForegroundColor Cyan }

# --- Kontrollera bygget ---
$opts = Get-Content (Join-Path $Build "build.options.json") -Raw
if ($opts -notmatch 'CDCOnBoot=cdc' -or $opts -notmatch 'PartitionScheme=no_fs') {
    throw "Bygget har inte CDCOnBoot=cdc + PartitionScheme=no_fs"
}
$fw = (Select-String -Path (Join-Path $PSScriptRoot "temperatur_reporter_esp32c3.ino") -Pattern '#define\s+FW_VERSION\s+"([^"]+)"').Matches[0].Groups[1].Value

# --- 0. Chip-info: MAC och PIN direkt fran chippet (PIN = sista 3 MAC-bytes, som getChipId()) ---
Step "Laser chip-info pa $Port..."
$chipMac = ""; $chipPin = ""
$ErrorActionPreference = "Continue"   # PS 5.1: stderr fran native + 2>&1 blir annars terminerande fel
$info = & $Esptool --chip esp32c3 --port $Port --baud 921600 --before default-reset --after no-reset read-mac 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) { throw "Kunde inte kontakta enheten pa $Port" }
if ($info -match 'MAC:\s+([0-9a-fA-F:]{17})') {
    $chipMac = $Matches[1].ToUpper()
    $b = $chipMac.Split(':')
    $chipPin = [string]([Convert]::ToInt32($b[3] + $b[4] + $b[5], 16))
    Step "MAC $chipMac  PIN $chipPin"
}

# --- 1. Flasha (hoppa over om enheten redan har exakt detta bygge) ---
Step "Jamfor enhetens flash med bygget $fw..."
& $Esptool --chip esp32c3 --port $Port --baud 921600 --before no-reset --after no-reset verify-flash `
    --flash-mode keep --flash-freq keep --flash-size keep `
    0x8000  (Join-Path $Build "$Name.partitions.bin") `
    0x10000 (Join-Path $Build "$Name.bin") 2>&1 | Out-Null
$ErrorActionPreference = "Stop"
$alreadyFlashed = ($LASTEXITCODE -eq 0)

Step "Raderar NVS..."
& $Esptool --chip esp32c3 --port $Port --baud 921600 --before no-reset --after no-reset erase-region 0x9000 0x5000 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "erase-region misslyckades" }

if ($alreadyFlashed) {
    Step "Redan flashad med $fw - hoppar over flashning, startar om for test..."
    & $Esptool --chip esp32c3 --port $Port --before no-reset --after hard-reset read-mac | Out-Null
} else {
    Step "Flashar $fw (ca 10 s)..."
    & $Esptool --chip esp32c3 --port $Port --baud 921600 --before no-reset --after hard-reset write-flash -z `
        --flash-mode keep --flash-freq keep --flash-size keep `
        0x0     (Join-Path $Build "$Name.bootloader.bin") `
        0x8000  (Join-Path $Build "$Name.partitions.bin") `
        0xe000  $BootApp0 `
        0x10000 (Join-Path $Build "$Name.bin") | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "write-flash misslyckades" }
}

# --- 2+3. Las serial, skicka WiFi nar portalen ar uppe ---
Step "Vantar pa att enheten startar portalen..."
$sp = $null
for ($i = 0; $i -lt 20 -and -not $sp; $i++) {
    try { $sp = New-Object System.IO.Ports.SerialPort $Port, 115200; $sp.Open() }
    catch { $sp = $null; Start-Sleep -Milliseconds 500 }
}
if (-not $sp) { throw "Kunde inte oppna $Port efter flash" }

$buf = ""; $sent = $false; $result = "TIMEOUT"
$sw = [Diagnostics.Stopwatch]::StartNew()
try {
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        try { $buf += $sp.ReadExisting() } catch {}
        # Uppstartsutskriften kan missas om porten oppnas for sent efter reset -
        # forsok da ansluta anda efter 12 s (portalen startar inom ~1 s efter boot).
        $portalSeen = $buf -match 'AP IP address|Config Portal Running|StartAP'
        if (-not $sent -and ($portalSeen -or $sw.Elapsed.TotalSeconds -gt 12)) {
            if ($portalSeen) { Step "Portalen uppe, ansluter PC:ns WiFi till $ApSsid..." }
            else { Step "Ingen serieutskrift fran enheten - provar ansluta till $ApSsid anda..." }
            $xml = @"
<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
  <name>$ApSsid</name>
  <SSIDConfig><SSID><name>$ApSsid</name></SSID></SSIDConfig>
  <connectionType>ESS</connectionType><connectionMode>manual</connectionMode>
  <MSM><security><authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption></security></MSM>
</WLANProfile>
"@
            $xf = Join-Path $env:TEMP "tnu_report_profile.xml"
            Set-Content -Path $xf -Value $xml -Encoding ASCII
            netsh wlan add profile filename="$xf" | Out-Null
            $ok = $false
            for ($t = 0; $t -lt 6 -and -not $ok; $t++) {
                netsh wlan connect name=$ApSsid | Out-Null
                Start-Sleep -Seconds 4
                try { $buf += $sp.ReadExisting() } catch {}
                try {
                    $u = "http://$ApIp/wifisave?s=$([uri]::EscapeDataString($Ssid))&p=$([uri]::EscapeDataString($Pass))"
                    Invoke-WebRequest -UseBasicParsing -TimeoutSec 8 $u | Out-Null
                    $ok = $true
                } catch {}
            }
            netsh wlan disconnect | Out-Null
            netsh wlan delete profile name=$ApSsid | Out-Null
            if (-not $ok) { $result = "PORTAL_FAIL"; break }
            Step "WiFi-uppgifter skickade, vantar pa rapport..."
            $sent = $true
        }
        if ($buf -match 'Upload OK') { $result = "OK"; break }
        if ($buf -match 'HTTP (\d{3})' -and $Matches[1] -ne '200') { $result = "HTTP_" + $Matches[1]; break }
        Start-Sleep -Milliseconds 200
    }
    Start-Sleep -Milliseconds 300
    try { $buf += $sp.ReadExisting() } catch {}
} finally { $sp.Close() }

$pin  = if ($buf -match 'PIN\): (\d+)') { $Matches[1] } elseif ($chipPin) { $chipPin } else { "?" }
$mac  = if ($buf -match 'ESP MAC: ([0-9A-F:]+)') { $Matches[1] } elseif ($chipMac) { $chipMac } else { "?" }
$nsen = if ($buf -match 'Sensors: (\d+)') { $Matches[1] } else { "?" }
$temp = if ($buf -match '&t=(-?[\d.]+)') { $Matches[1] } else { "" }

# Givare hittad pa bussen men avlasningen misslyckades (-127 = svarar inte, 85 = power-on-varde)
if ($result -eq "OK") {
    foreach ($k in 't1', 't2') {
        if ($buf -match "&$k=(-?[\d.]+)") {
            $v = [double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture)
            if ($v -le -127 -or $v -eq 85) { $result = "GIVARFEL"; Step "Givarfel: $k = $v" }
        }
    }
}

# --- 4. Rensa WiFi ---
if (-not $Keep) {
    Step "Raderar WiFi-uppgifter (NVS)..."
    & $Esptool --chip esp32c3 --port $Port --before default-reset --after hard-reset erase-region 0x9000 0x5000 | Out-Null
    if ($LASTEXITCODE -ne 0) { $result += "+NVS_KVAR" }
}

if (-not (Test-Path $LogFile)) { "tid;fw;pin;mac;givare;temp;resultat" | Set-Content $LogFile -Encoding ASCII }
"{0:yyyy-MM-dd HH:mm};{1};{2};{3};{4};{5};{6}" -f (Get-Date), $fw, $pin, $mac, $nsen, $temp, $result | Add-Content $LogFile -Encoding ASCII

$color = if ($result -eq "OK") { "Green" } else { "Red" }
Write-Host ("RESULTAT: {0}  PIN {1}  MAC {2}  givare {3}  {4}" -f $result, $pin, $mac, $nsen, $temp) -ForegroundColor $color
if ($result -ne "OK") { Write-Host "--- serial ---"; Write-Host $buf }
