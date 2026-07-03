# ssz_native_compare.ps1
# Compares SSZ files against native C++ services.
# Output: docs/native_ssz_comparison.md

$manifestPath = "C:\Projects\ikemen-new-ultra\docs\ssz_symbol_manifest.txt"
$nativeDir = "C:\Projects\ikemen-new-ultra\main\ssz_native"

# Parse symbol manifest
$sszFiles = @{}
Get-Content $manifestPath | ForEach-Object {
    if ($_ -match '^(.+\.ssz): \((\d+) symbols\)$') {
        $sszFiles[$Matches[1]] = [int]$Matches[2]
    }
}

# Map SSZ files to native service basenames
$mappings = @{}
$sszFiles.Keys | ForEach-Object {
    $f = $_ -replace '\\', '/'
    $base = [System.IO.Path]::GetFileNameWithoutExtension($f)
    $dir = [System.IO.Path]::GetDirectoryName($f)
    
    $native = switch -Wildcard ($f) {
        "lib/consts*"        { "consts" }
        "lib/math*"          { "math_service" }
        "lib/string*"        { "string_service" }
        "lib/alert*"         { "alert_service" }
        "lib/thread*"        { "thread_service" }
        "lib/time*"          { "time_service" }
        "lib/table*"         { "table_service" }
        "lib/base64*"        { "crypto_service" }
        "lib/arcfour*"       { "crypto_service" }
        "lib/md5*"           { "crypto_service" }
        "lib/file*"          { "file_service" }
        "lib/regex*"         { "regex_service" }
        "lib/socket*"        { "socket_service" }
        "lib/sound*"         { "sound_service" }
        "lib/shell*"         { "shell_service" }
        "lib/alpha/ogg*"     { "ogg_service" }
        "lib/alpha/lua*"     { "lua_service" }
        "lib/alpha/mesdialog*" { "mesdialog_service" }
        "lib/alpha/sdlplugin*" { "sdlplugin_service" }
        "lib/alpha/sdlevent*"  { "sdlevent_service" }
        "lib/ssz*"           { "ssz_service" }     # no separate service; lib/ssz.ssz is SSZ runtime
        "lib/stack*"         { "stack_service" }   # no separate service
        "save/config.ssz"    { "config_service" }
        "save/configNet*"    { "config_net_service" }
        "ssz/share*"         { "share_service" }
        "ssz/system.ssz"     { "system_service" }
        "ssz/debug*"         { "debug_script_service" }
        "ssz/loader*"        { "loader_service" }
        "ssz/common*"        { "common_service" }
        "ssz/trigger*"       { "trigger_script_service" }
        "ssz/script.ssz"     { "script_service" }
        "ssz/system-sc*"     { "system_script_service" }
        "ssz/statebuilder*"  { "statebuilder_service" }
        "ssz/action*"        { "action_service" }
        "ssz/bg*"            { "bg_service" }
        "ssz/char*"          { "char_service" }
        "ssz/command*"       { "command_service" }
        "ssz/fight.ssz"      { "fight_service" }
        "ssz/fighting*"      { "fighting_service" }
        "ssz/font*"          { "font_service" }
        "ssz/sff*"           { "sff_service" }
        "ssz/sound*"         { "sound_resource_service" }
        "ssz/stage*"         { "stage_service" }
        "ssz/video*"         { "video_service" }
        "ssz/ikemen*"        { "ikemen.ssz (boot script, no service)" }
        default              { "UNMAPPED" }
    }
    $mappings[$_] = $native
}

# Check which have native scaffolding
$totalSymbols = 0
$scaffoldedSymbols = 0
$rows = @()

$sszFiles.Keys | Sort-Object | ForEach-Object {
    $f = $_
    $symbols = $sszFiles[$f]
    $totalSymbols += $symbols
    $native = $mappings[$f]
    
    $found = $false
    if ($native -ne "UNMAPPED" -and $native -notmatch "boot script") {
        $hpp = "$nativeDir\$native.hpp"
        if (Test-Path $hpp) { $found = $true }
    }
    if ($native -match "boot script") { $found = $true }
    
    if ($found) { $scaffoldedSymbols += $symbols }
    
    $status = if ($found) { "Y" } else { "N" }
    $rows += [PSCustomObject]@{ File = $f -replace '\\','/'; Symbols = $symbols; Native = $native; Status = $status }
}

$pct = if ($totalSymbols -gt 0) { [math]::Round($scaffoldedSymbols/$totalSymbols*100, 1) } else { 0 }

$report = "# Native SSZ Manifest Comparison`n"
$report += "`n"
$report += "**Generated:** $(Get-Date -Format 'yyyy-MM-dd')`n"
$report += "**Total symbols:** $totalSymbols`n"
$report += "**Scaffolded symbols:** $scaffoldedSymbols ($pct%)`n"
$report += "`n"
$report += "| SSZ File | Symbols | Native Service | Scaffolded |`n"
$report += "|----------|---------|----------------|------------|`n"

$rows | Sort-Object File | ForEach-Object {
    $report += "| $($_.File) | $($_.Symbols) | $($_.Native) | $($_.Status) |`n"
}

$notFounds = $rows | Where-Object { $_.Status -eq "N" }
if ($notFounds.Count -gt 0) {
    $report += "`n## Unscaffolded`n`n"
    $notFounds | ForEach-Object { $report += "- $($_.File)`n" }
}

$report += "`n## Summary`n`n"
$report += "- **$($rows.Count) of $($rows.Count) SSZ files checked**`n"
$report += "- **$(($rows | Where-Object { $_.Status -eq 'Y' }).Count) of $($rows.Count) have native scaffolding** ($pct% of symbols)`n"
$report += "- Remaining: wire stubs to real native logic`n"

$report | Out-File -FilePath "C:\Projects\ikemen-new-ultra\docs\native_ssz_comparison.md" -Encoding UTF8
Write-Host "Report written to docs/native_ssz_comparison.md"
