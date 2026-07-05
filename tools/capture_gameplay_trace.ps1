# capture_gameplay_trace.ps1 — Method 2 A/B gameplay trace comparison
#
# Captures gameplay traces in two configurations:
#   A: SDL modules DISABLED (SSZ script path) — baseline
#   B: All native enabled                    — comparison
#
# Then diffs the unique TRACE function counts to find behavioral differences.
#
# Usage:
#   cd C:\Projects\ikemen-new-ultra\install
#   ..\tools\capture_gameplay_trace.ps1
#
# Requires: w64devkit in PATH, window focus on the game window during capture.

$ErrorActionPreference = "Continue"

$BuildDir = "C:\Projects\ikemen-new-ultra"
$InstallDir = "$BuildDir\install"
$CaptureSeconds = 25           # How long to let the game run before killing
$StartupDelay = 8              # Wait for game to boot before sending keys

# Key sequence: navigate menus and start a match
# Title → ENTER → Down(select) → ENTER(fight) → wait
$KeySequence = @(
    @('{ENTER}', 500),     # Title screen: press start
    @('{DOWN}', 300),      # Move down once
    @('{ENTER}', 800),     # Select arcade mode
    @('{ENTER}', 500),     # Character select screen: confirm
    @('{ENTER}', 500),     # Confirm stage
    @('{ENTER}', 300),     # Skip any pre-fight screen
    @('{ENTER}', 300),
    @('{ENTER}', 300),
    @('{ENTER}', 300)
)

# ---- Step 1: Build with SDL disabled (trace method A) ----
Write-Host "=== Step 1: Building with SDL DISABLED (baseline) ===" -ForegroundColor Cyan

Push-Location $BuildDir
$env:PATH = "C:\x86devkit\bin;$env:PATH"

& make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 `
    IKEMEN_NATIVE_SDLPLUGIN_LIB=0 IKEMEN_NATIVE_SDLEVENT_LIB=0 `
    CONFIG=Debug clean all install 2>&1 | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED (SDL disabled)" -ForegroundColor Red
    Pop-Location
    exit 1
}
Write-Host "Build OK. Starting game..." -ForegroundColor Gray

# Start game with trace capture
$pA = Start-Process -FilePath ".\install\ikemen-debug.exe" -WorkingDirectory $InstallDir `
    -RedirectStandardOutput "trace_gameplay_A.log" `
    -RedirectStandardError "trace_gameplay_A_stderr.log" -NoNewWindow -PassThru

Start-Sleep -Seconds $StartupDelay

# Send keyboard input sequence
try {
    Add-Type -AssemblyName System.Windows.Forms
    # Focus the game window
    $wshell = New-Object -ComObject wscript.shell
    $wshell.AppActivate("ikemen")
    Start-Sleep -Milliseconds 500

    foreach ($key in $KeySequence) {
        [System.Windows.Forms.SendKeys]::SendWait($key[0])
        Start-Sleep -Milliseconds $key[1]
    }
} catch {
    Write-Host "SendKeys warning: $_" -ForegroundColor Yellow
    Write-Host "Manual input required — focus the game window and navigate menus" -ForegroundColor Yellow
}

Start-Sleep -Seconds ($CaptureSeconds - $StartupDelay)

if (!$pA.HasExited) {
    $pA.Kill()
}
Write-Host "Trace A captured ($($pA.ExitCode))" -ForegroundColor Gray

# ---- Step 2: Build with all native (trace method B) ----
Write-Host "=== Step 2: Building with ALL NATIVE (comparison) ===" -ForegroundColor Cyan

& make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 `
    CONFIG=Debug clean all install 2>&1 | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED (all native)" -ForegroundColor Red
    Pop-Location
    exit 1
}
Write-Host "Build OK. Starting game..." -ForegroundColor Gray

$pB = Start-Process -FilePath ".\install\ikemen-debug.exe" -WorkingDirectory $InstallDir `
    -RedirectStandardOutput "trace_gameplay_B.log" `
    -RedirectStandardError "trace_gameplay_B_stderr.log" -NoNewWindow -PassThru

Start-Sleep -Seconds $StartupDelay

try {
    $wshell.AppActivate("ikemen")
    Start-Sleep -Milliseconds 500
    foreach ($key in $KeySequence) {
        [System.Windows.Forms.SendKeys]::SendWait($key[0])
        Start-Sleep -Milliseconds $key[1]
    }
} catch {
    Write-Host "SendKeys warning: $_" -ForegroundColor Yellow
}

Start-Sleep -Seconds ($CaptureSeconds - $StartupDelay)

if (!$pB.HasExited) {
    $pB.Kill()
}
Write-Host "Trace B captured ($($pB.ExitCode))" -ForegroundColor Gray

Pop-Location

# ---- Step 3: Extract and compare traces ----
Write-Host "`n=== Step 3: Trace Comparison ===" -ForegroundColor Cyan

# Extract unique function names with counts for both traces
$traceA = Select-String -Path "$InstallDir\trace_gameplay_A.log" -Pattern "\[TRACE\] " | 
    ForEach-Object { $_ -replace '.*\[TRACE\] ', '' } | 
    Group-Object | 
    Sort-Object Count -Descending

$traceB = Select-String -Path "$InstallDir\trace_gameplay_B.log" -Pattern "\[TRACE\] " | 
    ForEach-Object { $_ -replace '.*\[TRACE\] ', '' } | 
    Group-Object | 
    Sort-Object Count -Descending

# Save to files
$traceA | Format-Table Count, Name -AutoSize | Out-File "$InstallDir\trace_gameplay_A_counts.txt"
$traceB | Format-Table Count, Name -AutoSize | Out-File "$InstallDir\trace_gameplay_B_counts.txt"

# Find differences
$namesA = $traceA | ForEach-Object { $_.Name } | Sort-Object
$namesB = $traceB | ForEach-Object { $_.Name } | Sort-Object

$onlyInA = Compare-Object $namesA $namesB | Where-Object { $_.SideIndicator -eq '<=' }
$onlyInB = Compare-Object $namesA $namesB | Where-Object { $_.SideIndicator -eq '=>' }

Write-Host "`n--- Functions ONLY in baseline (SDL disabled) ---" -ForegroundColor Yellow
if ($onlyInA) { $onlyInA | ForEach-Object { Write-Host "  + $($_.InputObject)" -ForegroundColor Green } }
else { Write-Host "  (none)" -ForegroundColor Gray }

Write-Host "`n--- Functions ONLY in native (all enabled) ---" -ForegroundColor Yellow
if ($onlyInB) { $onlyInB | ForEach-Object { Write-Host "  + $($_.InputObject)" -ForegroundColor Red } }
else { Write-Host "  (none)" -ForegroundColor Gray }

# Show count differences for shared functions
Write-Host "`n--- Count Differences (top 20) ---" -ForegroundColor Yellow

$diffResults = @()
foreach ($itemA in $traceA) {
    $itemB = $traceB | Where-Object { $_.Name -eq $itemA.Name }
    if ($itemB) {
        $diff = $itemB.Count - $itemA.Count
        $pct = if ($itemA.Count -gt 0) { [math]::Round(($itemB.Count / $itemA.Count) * 100) } else { 0 }
        $diffResults += [PSCustomObject]@{
            Function = $itemA.Name
            CountA   = $itemA.Count
            CountB   = $itemB.Count
            Diff     = $diff
            Pct      = "$pct%"
        }
    }
}

$diffResults | Sort-Object { [math]::Abs($_.Diff) } -Descending | 
    Select-Object -First 20 | 
    Format-Table Function, CountA, CountB, Diff, Pct -AutoSize

Write-Host "`n=== Done ===" -ForegroundColor Cyan
Write-Host "Results saved to:" -ForegroundColor Gray
Write-Host "  install\trace_gameplay_A_counts.txt" -ForegroundColor Gray
Write-Host "  install\trace_gameplay_B_counts.txt" -ForegroundColor Gray
Write-Host "  install\trace_gameplay_A.log (full log)" -ForegroundColor Gray
Write-Host "  install\trace_gameplay_B.log (full log)" -ForegroundColor Gray
