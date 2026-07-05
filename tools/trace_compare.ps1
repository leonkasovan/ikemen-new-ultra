<#
.SYNOPSIS
    SSZ Native Conversion Trace Comparison Pipeline
    
.DESCRIPTION
    Compares two IKEMEN trace logs (baseline vs native) and generates a structured
    parity report identifying function-level behavioral differences.

    The script:
      1. Extracts all [TRACE] entries from both logs
      2. Computes unique function sets and call counts
      3. Identifies functions only in one build (additions/removals)
      4. Computes count deltas for shared functions with ratio
      5. Generates a Markdown parity report
      6. Flags anomalies (>5% count ratio deviation after normalization)
      7. Optionally auto-normalizes by frame count (Flip ratio)

.PARAMETER BaselineLog
    Path to the baseline (SSZ-only) trace log file.
    Default: install/trace_method1_baseline.log

.PARAMETER NativeLog
    Path to the native build trace log file.
    Default: install/trace_method1_native.log

.PARAMETER Output
    Path to the generated parity report.
    Default: docs/trace_parity_report.md

.PARAMETER LabelA
    Label for the baseline build (default: "Baseline (SSZ-only)")

.PARAMETER LabelB
    Label for the native build (default: "Native (SSZ=1)")

.PARAMETER TrimLines
    If specified, reads only the first N lines from each log (for quick tests).

.PARAMETER Normalize
    If set, normalizes counts by the Flip-frame ratio to account for
    different capture durations.

.PARAMETER Threshold
    Anomaly detection threshold as a decimal (default 0.15 = 15% deviation).
    Functions with |ratio - normalization_factor| > threshold are flagged.

.EXAMPLE
    # Compare Method 1 trace logs
    .\tools\trace_compare.ps1

.EXAMPLE
    # Compare two specific gameplay traces with normalization
    .\tools\trace_compare.ps1 -BaselineLog "install\trace_gameplay_A.log" `
        -NativeLog "install\trace_gameplay_B.log" `
        -LabelA "SDL Disabled" -LabelB "All Native" -Normalize

.EXAMPLE
    # Quick check: only first 5000 lines
    .\tools\trace_compare.ps1 -TrimLines 5000
#>

param(
    [string]$BaselineLog = "install/trace_method1_baseline.log",
    [string]$NativeLog   = "install/trace_method1_native.log",
    [string]$Output      = "docs/trace_parity_report.md",
    [string]$LabelA      = "Baseline (SSZ-only)",
    [string]$LabelB      = "Native (SSZ=1)",
    [int]    $TrimLines  = 0,
    [switch] $Normalize,
    [float]  $Threshold  = 0.15
)

# ── Helper: extract function counts from a trace log ──
function Get-TraceCounts {
    param([string]$Path, [string]$Label, [int]$Trim)

    if (-not (Test-Path $Path)) {
        Write-Warning "File not found: $Path"
        return $null
    }

    $maxVal = [int]::MaxValue
    if ($Trim -gt 0) { $lines = Get-Content -Path $Path -TotalCount $Trim } else { $lines = Get-Content -Path $Path }
    $totalLines = $lines.Count

    # Extract function names from [TRACE] FuncName
    $traceLines = $lines | Select-String -Pattern "\[TRACE\] " | ForEach-Object {
        $_ -replace '.*\[TRACE\] ', ''
    }

    $unique = $traceLines | Sort-Object -Unique

    # Count per function
    $counts = $traceLines | Group-Object | ForEach-Object {
        [PSCustomObject]@{
            Function = $_.Name
            Count    = $_.Count
        }
    } | Sort-Object Count -Descending

    return [PSCustomObject]@{
        Label       = $Label
        Path        = $Path
        TotalLines  = $totalLines
        TraceLines  = $traceLines.Count
        UniqueFuncs = $unique.Count
        Functions   = $counts
        FuncSet     = $unique
    }
}

# ── Helper: write colored status ──
function Write-Status {
    param([string]$Message, [string]$Color = "Gray")
    Write-Host "  $Message" -ForegroundColor $Color
}

Write-Host "╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   SSZ Native Trace Comparison Pipeline          ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ── Step 1: Parse both logs ──
Write-Host "── Step 1: Parsing trace logs ──" -ForegroundColor Cyan

$baseline = Get-TraceCounts -Path $BaselineLog -Label $LabelA -Trim $TrimLines
$native   = Get-TraceCounts -Path $NativeLog   -Label $LabelB -Trim $TrimLines

if (-not $baseline -or -not $native) {
    Write-Error "Cannot proceed — one or both trace logs are missing."
    exit 1
}

Write-Status "$($baseline.Label): $($baseline.TotalLines) lines, $($baseline.TraceLines) traces, $($baseline.UniqueFuncs) unique functions" "Green"
Write-Status "$($native.Label):   $($native.TotalLines) lines, $($native.TraceLines) traces, $($native.UniqueFuncs) unique functions" "Green"

# ── Step 2: Compare function sets ──
Write-Host ""
Write-Host "── Step 2: Comparing function sets ──" -ForegroundColor Cyan

$onlyInA = $baseline.FuncSet | Where-Object { $_ -notin $native.FuncSet }
$onlyInB = $native.FuncSet   | Where-Object { $_ -notin $baseline.FuncSet }
$shared  = $baseline.FuncSet | Where-Object { $_ -in $native.FuncSet } | Sort-Object

Write-Status "Functions only in $($baseline.Label): $(if ($onlyInA) { $onlyInA.Count } else { 0 })" $(if ($onlyInA) { "Yellow" } else { "Green" })
Write-Status "Functions only in $($native.Label):   $(if ($onlyInB) { $onlyInB.Count } else { 0 })" $(if ($onlyInB) { "Red" } else { "Green" })
Write-Status "Shared functions: $($shared.Count)" "Green"

# ── Step 3: Compute normalization factor (Flip ratio) ──
Write-Host ""
Write-Host "── Step 3: Computing deltas ──" -ForegroundColor Cyan

$flipA = ($baseline.Functions | Where-Object { $_.Function -eq "Flip" }).Count
$flipB = ($native.Functions   | Where-Object { $_.Function -eq "Flip" }).Count

$normFactor = 1.0
if ($Normalize -and $flipA -gt 0 -and $flipB -gt 0) {
    $normFactor = [float]$flipA / [float]$flipB
    Write-Status "Normalization ON: Flip ratio = $flipA / $flipB = $($normFactor.ToString('F3'))" "Cyan"
    Write-Status "  (Native counts multiplied by $($normFactor.ToString('F3')) for comparison)" "Gray"
} elseif ($flipA -gt 0 -and $flipB -gt 0) {
    $normFactor = [float]$flipA / [float]$flipB
    Write-Status "Flip ratio: $flipA / $flipB = $($normFactor.ToString('F3')) (use -Normalize to apply)" "Gray"
} else {
    Write-Status "Flip not found in one or both logs — cannot normalize" "Yellow"
}

# Build comparison table
$comparison = foreach ($func in $shared) {
    $countA = ($baseline.Functions | Where-Object { $_.Function -eq $func }).Count
    $countB = ($native.Functions   | Where-Object { $_.Function -eq $func }).Count

    $normB = [math]::Round($countB * $normFactor)
    $delta = $countB - $countA
    $pct = if ($countA -gt 0) { [math]::Round(100.0 * $countB / $countA) } else { 0 }
    $normPct = if ($countA -gt 0 -and $Normalize) { [math]::Round(100.0 * ($countB * $normFactor) / $countA) } else { $pct }

    # Detect anomaly: significant deviation from expected ratio
    $anomaly = $false
    if ($Normalize -and $countA -gt 0 -and $countB -gt 0) {
        $ratio = [float]$countB / [float]$countA
        $deviation = [math]::Abs($ratio - (1.0 / $normFactor))
        if ($deviation -gt $Threshold) { $anomaly = $true }
    }

    [PSCustomObject]@{
        Function    = $func
        CountA       = $countA
        CountB       = $countB
        NormalizedB  = $normB
        Delta        = $delta
        Ratio        = "$pct%"
        NormRatio    = "$normPct%"
        Anomaly      = $anomaly
    }
}

# ── Step 4: Generate Markdown report ──
Write-Host ""
Write-Host "── Step 4: Generating report ──" -ForegroundColor Cyan

$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm"
$report = @"
# Trace Parity Report

**Generated:** $timestamp  
**Baseline:** `$BaselineLog ($($baseline.TotalLines) lines)  
**Native:**   `$NativeLog ($($native.TotalLines) lines)  
**Normalization:** $(if ($Normalize) { "ON (Flip ratio: $($normFactor.ToString('F3')))" } else { "OFF" })

---

## Summary

| Metric | $($baseline.Label) | $($native.Label) | Verdict |
|--------|-------------------|----------------|---------|
| Total log lines | $($baseline.TotalLines) | $($native.TotalLines) | — |
| **[TRACE]** lines | $($baseline.TraceLines) | $($native.TraceLines) | — |
| Unique TRACE functions | $($baseline.UniqueFuncs) | $($native.UniqueFuncs) | $(if ($baseline.UniqueFuncs -eq $native.UniqueFuncs) { "✅ Match" } else { "⚠️ Mismatch" }) |
| Functions only in baseline | $(if ($onlyInA) { $onlyInA.Count } else { 0 }) | — | $(if (-not $onlyInA) { "✅ None" } else { "⚠️ Present" }) |
| Functions only in native | — | $(if ($onlyInB) { $onlyInB.Count } else { 0 }) | $(if (-not $onlyInB) { "✅ None" } else { "⚠️ Present" }) |
| Shared functions | $($shared.Count) | $($shared.Count) | ✅ |

"@

# Functions only in baseline
if ($onlyInA) {
    $report += @"

### ⚠️ Functions Removed (only in baseline)

| Function |
|----------|
"@
    foreach ($f in ($onlyInA | Sort-Object)) {
        $report += "`n| `$f |"
    }
} else {
    $report += @"

### ✅ No Removed Functions

All functions present in the baseline also appear in the native build.

"@
}

# Functions only in native
if ($onlyInB) {
    $report += @"

### ⚠️ Functions Added (only in native)

| Function |
|----------|
"@
    foreach ($f in ($onlyInB | Sort-Object)) {
        $report += "`n| `$f |"
    }
} else {
    $report += @"

### ✅ No Added Functions

No new functions appear in the native build.

"@
}

# Full comparison table
$report += @"

---

## Full Trace Count Comparison

| Function | $($baseline.Label) | $($native.Label) | Delta | Ratio |$(if ($Normalize) { " Norm Ratio | Anomaly |" } else { "") }
|----------|-------------------|----------------|-------|-------|$(if ($Normalize) { "-----------|---------|" } else { "")}
"@

foreach ($row in ($comparison | Sort-Object { [math]::Abs($_.Delta) } -Descending)) {
    $anomalyFlag = if ($Normalize -and $row.Anomaly) { " ⚠️" } else { "" }
    $deltaStr = if ($row.Delta -ge 0) { "+$($row.Delta)" } else { "$($row.Delta)" }
    
    if ($Normalize) {
        $report += "`n| $($row.Function) | $($row.CountA) | $($row.CountB) | $($deltaStr) | $($row.Ratio) | $($row.NormRatio) | $($anomalyFlag) |"
    } else {
        $report += "`n| $($row.Function) | $($row.CountA) | $($row.CountB) | $($deltaStr) | $($row.Ratio) |"
    }
}

# Anomalies section
if ($Normalize) {
    $anomalies = $comparison | Where-Object { $_.Anomaly } | Sort-Object { [math]::Abs($_.Delta) } -Descending
    if ($anomalies) {
        $report += @"

---

### ⚠️ Anomalies Detected

Functions with normalized count ratio deviating >$($Threshold * 100)% from the Flip frame ratio:

| Function | Baseline | Native | Normalized | Actual Ratio | Expected Ratio |
|----------|----------|--------|------------|--------------|----------------|
"@
        foreach ($a in $anomalies) {
            $expectedRatio = 1.0 / $normFactor
            $actualRatio = if ($a.CountA -gt 0) { [math]::Round([float]$a.CountB / [float]$a.CountA, 3) } else { "N/A" }
            $report += "`n| $($a.Function) | $($a.CountA) | $($a.CountB) | $($a.NormalizedB) | $($actualRatio) | $($expectedRatio.ToString('F3')) |"
        }
    } else {
        $report += @"

### ✅ No Anomalies

All function count ratios are within the $($Threshold * 100)% threshold of the expected Flip frame ratio.

"@
    }
}

# Boot-time functions (exact match expected)
$exactFuncs = $comparison | Where-Object {
    $_.Function -in @(
        'Register', 'RegexSearch', 'NewRegex', 'DeleteRegex',
        'AsciiToLocal', 'DecodePNG8', 'SoftFill', 'SetVolume',
        'LuaInit', 'TickCount', 'UnixTime', 'NewCompiler',
        'DeleteCompiler', 'CompilerRun', 'CompilerCompileString',
        'End', 'MemMarkBefore', 'MemMarkAfter',
        'FullScreenExclusive', 'FullScreen', 'CursorShow',
        'AspectRatio', 'WindowType', 'SetOpacity', 'RendererInit',
        'NewState', 'Close', 'RunFile', 'GetSharedString',
        'SetSharedString', 'VeryUnsafeCopy', 'ToBoolean',
        'IsBoolean', 'MemMarkBefore', 'MemMarkAfter'
    )
}
$exactMismatches = $exactFuncs | Where-Object { $_.CountA -ne $_.CountB }

if ($exactMismatches) {
    $report += @"

---

### ⚠️ Boot-Time Function Count Mismatches

These functions should have identical counts in both builds (discrete boot events):

| Function | Baseline | Native | Delta |
|----------|----------|--------|-------|
"@
    foreach ($m in $exactMismatches) {
        $report += "`n| $($m.Function) | $($m.CountA) | $($m.CountB) | $($m.Delta) |"
    }
} else {
    $report += @"

### ✅ Boot-Time Functions Match

All discrete boot-event functions have identical counts between builds.

"@
}

# ── Write the report ──
$report | Out-File -FilePath $Output -Encoding UTF8

Write-Status "Report written to $Output" "Green"

# ── Step 5: Show terminal summary ──
Write-Host ""
Write-Host "╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   Results                                        ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan

Write-Host ""
Write-Host "Function Set Parity: " -NoNewline
if (-not $onlyInA -and -not $onlyInB) {
    Write-Host "✅ PERFECT — 0 additions, 0 removals" -ForegroundColor Green
} else {
    Write-Host "⚠️ $(if ($onlyInA) { "$($onlyInA.Count) removed, " })$(if ($onlyInB) { "$($onlyInB.Count) added" })" -ForegroundColor Yellow
}

$sharedExact = $comparison | Where-Object { $_.CountA -eq $_.CountB } | Measure-Object | Select-Object -ExpandProperty Count
$sharedDiff = $comparison | Where-Object { $_.CountA -ne $_.CountB } | Measure-Object | Select-Object -ExpandProperty Count

Write-Host "Count Parity: " -NoNewline
if ($sharedDiff -eq 0) {
    Write-Host "✅ PERFECT — all $sharedExact shared functions have identical counts" -ForegroundColor Green
} else {
    Write-Host "⚠️ $sharedExact exact matches, $sharedDiff with deltas" -ForegroundColor Yellow
    if ($Normalize) {
        $anomalyCount = ($comparison | Where-Object { $_.Anomaly }).Count
        if ($anomalyCount -eq 0) {
            Write-Host "  (but all within normalization threshold after Flip adjustment)" -ForegroundColor Green
        } else {
            Write-Host "  ($anomalyCount anomalies beyond normalization threshold)" -ForegroundColor Red
        }
    }
}

Write-Host ""
Write-Host "Report saved to: $Output" -ForegroundColor Cyan
