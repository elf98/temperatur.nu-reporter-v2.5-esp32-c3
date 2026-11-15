# Lagger ut release-bygget for webbflash-sidan https://www.temperatur.nu/reporter/flasha/
#
#   .\webflash_deploy.ps1
#
# Kors efter att OTA-releasen ar ute (samma bygge i build/esp32.esp32.esp32c3/). Skriptet:
#   - laser FW_VERSION ur .ino och kontrollerar bygget (CDCOnBoot=cdc + PartitionScheme=no_fs)
#   - laddar upp bootloader, partitionstabell, boot_app0 och app med versionen i filnamnet
#   - skriver manifest.json atomiskt (sidan visar versionen ur manifestet)
# ESP Web Tools raderar hela flashminnet fore en ny installation (new_install_prompt_erase = false),
# sa sparat WiFi forsvinner och enheten startar med portalen. boot_app0 pa 0xe000 ger tom otadata,
# sa enheten startar fran ota_0 dar appen laggs.

$ErrorActionPreference = "Stop"
$Server     = "erik@185.189.50.240"
$RemoteBase = "/var/www/temperatur.nu/reporter/flasha"
$Build      = Join-Path $PSScriptRoot "build/esp32.esp32.esp32c3"
$Name       = "temperatur_reporter_esp32c3.ino"

$Version = (Select-String -Path (Join-Path $PSScriptRoot "$Name") -Pattern '^\s*#define\s+FW_VERSION\s+"([^"]+)"').Matches[0].Groups[1].Value
$opts = Get-Content (Join-Path $Build "build.options.json") -Raw
if ($opts -notmatch 'CDCOnBoot=cdc' -or $opts -notmatch 'PartitionScheme=no_fs') { throw "Bygget har inte CDCOnBoot=cdc + PartitionScheme=no_fs" }

# Bygget ska vara samma som ligger pa OTA-kanalen for versionen
$bin = Join-Path $Build "$Name.bin"
if (-not (Select-String -Path $bin -Pattern ([regex]::Escape($Version)) -Encoding Default -Quiet)) { throw "Bygget i build/ innehaller inte versionen $Version - bygg om forst" }

$parts = @(
    @{ src = "$Name.bootloader.bin"; dst = "bootloader-$Version.bin"; offset = 0 },
    @{ src = "$Name.partitions.bin"; dst = "partitions-$Version.bin"; offset = 32768 },
    @{ src = "boot_app0.bin";        dst = "boot_app0-$Version.bin";  offset = 57344 },
    @{ src = "$Name.bin";            dst = "firmware-$Version.bin";   offset = 65536 }
)
foreach ($p in $parts) { if (-not (Test-Path (Join-Path $Build $p.src))) { throw "Saknar $($p.src) i build/" } }

Write-Host "Laddar upp $Version till $RemoteBase/firmware ..."
ssh $Server "mkdir -p $RemoteBase/firmware"
if ($LASTEXITCODE -ne 0) { throw "Kunde inte skapa katalogen" }
foreach ($p in $parts) {
    scp -q (Join-Path $Build $p.src) "${Server}:$RemoteBase/firmware/$($p.dst)"
    if ($LASTEXITCODE -ne 0) { throw "Uppladdning misslyckades: $($p.dst)" }
}

$manifest = [ordered]@{
    name    = "Temperatur.nu Reporter"
    version = $Version
    new_install_prompt_erase = $false
    builds  = @(@{
        chipFamily = "ESP32-C3"
        parts = @($parts | ForEach-Object { [ordered]@{ path = "firmware/$($_.dst)"; offset = $_.offset } })
    })
} | ConvertTo-Json -Depth 6
$tmp = Join-Path $env:TEMP "tnu_webflash_manifest.json"
[System.IO.File]::WriteAllText($tmp, $manifest, (New-Object System.Text.UTF8Encoding $false))
scp -q $tmp "${Server}:$RemoteBase/manifest.json.tmp"
ssh $Server "mv $RemoteBase/manifest.json.tmp $RemoteBase/manifest.json && chmod 644 $RemoteBase/manifest.json $RemoteBase/firmware/*-$Version.bin"
if ($LASTEXITCODE -ne 0) { throw "Kunde inte skriva manifest.json" }

$live = (Invoke-WebRequest -UseBasicParsing "https://www.temperatur.nu/reporter/flasha/manifest.json?v=$Version").Content | ConvertFrom-Json
if ($live.version -ne $Version) { throw "Manifestet pa servern sager $($live.version), forvantade $Version" }
Write-Host "KLAR: webbflash-sidan flashar nu $Version" -ForegroundColor Green
