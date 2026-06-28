# instrument_trace.ps1
# Adds SSZ_TRACE("FuncName") to all extern "C" bridge wrappers in bridge.cpp and ssz.cpp
# that don't already have it.

param(
    [string]$File = "",
    [switch]$DryRun
)

function Add-Trace {
    param([string]$Path)

    Write-Host "Processing: $Path" -ForegroundColor Cyan
    $lines = Get-Content -Path $Path
    $allText = $lines -join "`n"

    # Pass 1: Find all { line indices that follow a bridge wrapper signature
    $braceIndices = [System.Collections.Generic.List[int]]::new()
    $braceNames = [System.Collections.Generic.List[string]]::new()

    function Skip-Junk-Lines([string[]]$lines, [int]$start) {
        $j = $start
        while ($j -lt $lines.Count) {
            $l = $lines[$j]
            if ($l -match '^\s*$') { $j++; continue }       # empty
            if ($l -match '^\s*//') { $j++; continue }       # comment
            break
        }
        return $j
    }

    $i = 0
    while ($i -lt $lines.Count) {
        $line = $lines[$i]

        # Match: "extern "C" ... SSZ_STDCALL FuncName(..."
        if ($line -match '^extern "C".+SSZ_STDCALL\s+(\w+)\s*\(') {
            $funcName = $Matches[1]

            # Check if SSZ_TRACE("FuncName") already exists anywhere in file
            $traceExists = $allText.Contains('SSZ_TRACE("' + $funcName + '")')

            if ($line -match '\)\s*$') {
                # Single-line signature: ) on same line
                $j = Skip-Junk-Lines $lines ($i + 1)
                if ($j -lt $lines.Count -and $lines[$j] -match '^\s*\{\s*$') {
                    if (-not $traceExists) {
                        $braceIndices.Add($j)
                        $braceNames.Add($funcName)
                    }
                }
                $i = $j
            } else {
                # Multi-line signature: find the ) line
                $j = $i + 1
                while ($j -lt $lines.Count -and $lines[$j] -notmatch '\)\s*$') {
                    $j++
                }
                if ($j -lt $lines.Count) {
                    $k = Skip-Junk-Lines $lines ($j + 1)
                    if ($k -lt $lines.Count -and $lines[$k] -match '^\s*\{\s*$') {
                        if (-not $traceExists) {
                            $braceIndices.Add($k)
                            $braceNames.Add($funcName)
                        }
                    }
                    $i = $k
                } else {
                    $i++
                }
            }
        } else {
            $i++
        }
    }

    Write-Host "Found $($braceIndices.Count) functions to instrument" -ForegroundColor Cyan

    if ($DryRun) {
        Write-Host "DRY RUN - no changes made" -ForegroundColor Yellow
        return $braceIndices.Count
    }

    # Pass 2: Insert SSZ_TRACE after each { line (reverse order to preserve indices)
    for ($n = $braceIndices.Count - 1; $n -ge 0; $n--) {
        $idx = $braceIndices[$n]
        $name = $braceNames[$n]
        # Use indentation from the first body line after {
        $bodyLine = if ($idx + 1 -lt $lines.Length) { $lines[$idx + 1] } else { "" }
        $indent = if ($bodyLine -match '^(\s+)') { $Matches[1] } else { "    " }
        $traceLine = $indent + 'SSZ_TRACE("' + $name + '");'

        $lines = $lines[0..$idx] + @($traceLine) + $lines[($idx+1)..($lines.Length-1)]
    }

    if ($braceIndices.Count -gt 0) {
        Set-Content -Path $Path -Value ($lines -join "`r`n") -NoNewline
        Add-Content -Path $Path -Value ""
        Write-Host "  -> $Path MODIFIED ($($braceIndices.Count) traces added)" -ForegroundColor Yellow
    } else {
        Write-Host "  -> $Path unchanged" -ForegroundColor Gray
    }

    return $braceIndices.Count
}

$total = 0
if ($File) {
    $total += Add-Trace -Path $File
} else {
    $base = "C:\Projects\ikemen-new-ultra\main\ssz"
    $total += Add-Trace -Path "$base\bridge.cpp"
    $total += Add-Trace -Path "$base\ssz.cpp"
}

Write-Host "Total: $total traces added" -ForegroundColor Green
