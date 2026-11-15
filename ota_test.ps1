# OTA-test av en reporter (lopande band): flashar INTE, utan later enheten uppdatera sig sjalv.
#
#   .\ota_test.ps1 -Port COM7 -Ssid "natet" -Pass "losen" [-Keep]
#
# 1. Laser MAC/PIN fran chippet, raderar NVS och startar om, sa att portalen tnu_report oppnas.
# 2. Ansluter PC:ns WiFi-kort till portalen och skickar in WiFi-uppgifterna via /wifisave.
# 3. Laser serial: enheten ska hitta senaste versionen pa OTA-kanalen, hamta den, starta om och
#    rapportera (HTTP 200) med den versionen.
# 4. Raderar NVS igen (om inte -Keep) sa enheten startar portalen hos kunden.
# Skriver en resultatrad till flash_log.csv (gitignorerad), som flash_test.ps1.

param(
    [Parameter(Mandatory)] [string]$Port,
    [Parameter(Mandatory)] [string]$Ssid,
    [Parameter(Mandatory)] [string]$Pass,
    [switch]$Keep,
    [int]$TimeoutSec = 180
)

$ErrorActionPreference = "Continue"
$Arduino = Join-Path $env:LOCALAPPDATA "Arduino15/packages/esp32"
$Esptool = Get-ChildItem (Join-Path $Arduino "tools/esptool_py") -Directory |
           Sort-Object { [version]$_.Name } | Select-Object -Last 1 |
           ForEach-Object { Join-Path $_.FullName "esptool.exe" }
$ApSsid  = "tnu_report"
$ApIp    = "8.8.8.8"
$LogFile = Join-Path $PSScriptRoot "flash_log.csv"
$Target  = (Invoke-WebRequest -UseBasicParsing "http://report.temperatur.nu/firmware/esp32c3-nofs/version.txt").Content.Trim()

function Step($msg) { Write-Host ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $msg) -ForegroundColor Cyan }

# --- 1. Chip-info, radera NVS, starta om ---
Step "Laser chip-info pa $Port (mal: $Target)..."
$chipMac = ""; $chipPin = ""
$info = & $Esptool --chip esp32c3 --port $Port --baud 921600 --before default-reset --after no-reset read-mac 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) { Write-Host "RESULTAT $Port KONTAKTFEL"; exit 1 }
if ($info -match 'MAC:\s+([0-9a-fA-F:]{17})') {
    $chipMac = $Matches[1].ToUpper(); $b = $chipMac.Split(':')
    $chipPin = [string]([Convert]::ToInt32($b[3] + $b[4] + $b[5], 16))
}
& $Esptool --chip esp32c3 --port $Port --baud 921600 --before no-reset --after hard-reset erase-region 0x9000 0x5000 2>&1 | Out-Null

# --- 2+3. Serial + portal ---
function Open-Port {
    for ($i = 0; $i -lt 20; $i++) {
        try { $p = New-Object System.IO.Ports.SerialPort $Port, 115200; $p.DtrEnable = $true; $p.Open(); return $p }
        catch { Start-Sleep -Milliseconds 500 }
    }
    return $null
}
$sp = Open-Port
if (-not $sp) { Write-Host "RESULTAT $Port PORTFEL"; exit 1 }

$buf = ""; $sent = $false; $result = "TIMEOUT"; $startFw = ""; $endFw = ""
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
    # Porten forsvinner en stund nar enheten startar om efter OTA
    if (-not $sp -or -not $sp.IsOpen) { $sp = Open-Port; if (-not $sp) { break } }
    try { $buf += $sp.ReadExisting() } catch { try { $sp.Close() } catch {}; $sp = $null; continue }

    if (-not $startFw -and $buf -match 'Reporter v([\d.]+)') { $startFw = $Matches[1] }
    $portalSeen = $buf -match 'AP IP address|Config Portal Running|StartAP'
    if (-not $sent -and ($portalSeen -or $sw.Elapsed.TotalSeconds -gt 12)) {
        Step "Ansluter PC:ns WiFi till $ApSsid..."
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
        for ($t = 0; $t -lt 8 -and -not $ok; $t++) {
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
        Step "WiFi skickat, vantar pa OTA och rapport..."
        $sent = $true
    }
    # Senaste versionen som enheten rapporterat med
    $m = [regex]::Matches($buf, 'fw=([\d.]+)&[^\r\n]*[\r\n]+HTTP (\d{3})')
    if ($m.Count -gt 0) {
        $last = $m[$m.Count - 1]; $endFw = $last.Groups[1].Value
        if ($last.Groups[2].Value -ne '200') { $result = "HTTP_" + $last.Groups[2].Value; break }
        if ($endFw -eq $Target) { $result = "OK"; break }
    }
    if ($buf -match 'OTA failed') { $result = "OTA_FEL"; break }
    Start-Sleep -Milliseconds 200
}
Start-Sleep -Milliseconds 300
if ($sp) { try { $buf += $sp.ReadExisting() } catch {}; try { $sp.Close() } catch {} }

$n = if ($buf -match '&n=(\d)') { $Matches[1] } else { "?" }
$t = if ($buf -match '&t=(-?[\d.]+)') { $Matches[1] } else { "" }
if ($result -eq "OK" -and ($t -match '^-127|^85\.0')) { $result = "GIVARFEL" }
$ota = if ($buf -match 'OTA: ny version') { "ota" } else { "ingen-ota" }

# --- 4. Rensa NVS ---
if (-not $Keep) {
    & $Esptool --chip esp32c3 --port $Port --baud 921600 --before default-reset --after hard-reset erase-region 0x9000 0x5000 2>&1 | Out-Null
}
$sec = [int]$sw.Elapsed.TotalSeconds
# Samma kolumner som flash_test.ps1: tid;fw;pin;mac;givare;temp;resultat;steg
if (-not (Test-Path $LogFile)) { "tid;fw;pin;mac;givare;temp;resultat;steg" | Set-Content $LogFile -Encoding ASCII }
"{0:yyyy-MM-dd HH:mm};{1};{2};{3};{4};{5};{6};OTA {7}->{8}" -f (Get-Date), $endFw, $chipPin, $chipMac, $n, $t, $result, $startFw, $endFw | Add-Content $LogFile -Encoding ASCII
Write-Host "RESULTAT $Port PIN $chipPin MAC $chipMac $startFw->$endFw ($ota) givare $n temp $t ${sec}s $result"
