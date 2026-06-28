$p = Start-Process -FilePath ".\ikemen-debug.exe" -RedirectStandardOutput "trace_gameplay4.log" -RedirectStandardError "trace_gameplay4_stderr.log" -NoNewWindow -PassThru
Start-Sleep -Seconds 20

try {
    Add-Type -AssemblyName System.Windows.Forms
    $seq = @()
    for ($i = 0; $i -lt 30; $i++) {
        $seq += '{ENTER}'
        $seq += '{DOWN}'
        $seq += '{UP}'
        $seq += '{RIGHT}'
        $seq += '{LEFT}'
        $seq += ' '
    }
    foreach ($k in $seq) {
        [System.Windows.Forms.SendKeys]::SendWait($k)
        Start-Sleep -Milliseconds 200
    }
} catch {
    "SendKeys failed: $_"
}

Start-Sleep -Seconds 15

if ($p.HasExited) {
    "Process exited with code $($p.ExitCode)"
} else {
    $p.Kill()
    "Process killed after timeout"
}

$r = Get-Content "trace_gameplay4.log" | Select-String -Pattern "\[TRACE\]" | Group-Object | Select-Object Count, Name | Sort-Object Count -Descending
$r | Out-String | Write-Host
