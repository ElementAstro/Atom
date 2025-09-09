# PowerShell wrapper to diagnose memory test issues
param(
    [switch]$Verbose
)

Write-Host "Memory Test Diagnostic Wrapper" -ForegroundColor Green
Write-Host "===============================" -ForegroundColor Green

$TestExe = ".\build\tests\memory\atom_memory.test.exe"

# Check if executable exists
if (-not (Test-Path $TestExe)) {
    Write-Host "ERROR: Test executable not found: $TestExe" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Test executable found: $TestExe" -ForegroundColor Green

# Check file properties
$FileInfo = Get-Item $TestExe
Write-Host "File size: $($FileInfo.Length) bytes" -ForegroundColor Cyan
Write-Host "Last modified: $($FileInfo.LastWriteTime)" -ForegroundColor Cyan

# Check DLL dependencies in the same directory
$TestDir = Split-Path $TestExe -Parent
$DllFiles = Get-ChildItem -Path $TestDir -Filter "*.dll"
Write-Host "DLL files in test directory: $($DllFiles.Count)" -ForegroundColor Cyan
foreach ($dll in $DllFiles) {
    Write-Host "  - $($dll.Name)" -ForegroundColor Gray
}

# Try to run with timeout and capture output
Write-Host "Attempting to run test with 10-second timeout..." -ForegroundColor Yellow

try {
    $Job = Start-Job -ScriptBlock {
        param($ExePath)
        & $ExePath --gtest_list_tests 2>&1
    } -ArgumentList $TestExe
    
    $Result = Wait-Job -Job $Job -Timeout 10
    
    if ($Result) {
        $Output = Receive-Job -Job $Job
        Write-Host "SUCCESS: Test executed and returned output:" -ForegroundColor Green
        Write-Host $Output -ForegroundColor White
    } else {
        Write-Host "TIMEOUT: Test did not complete within 10 seconds" -ForegroundColor Red
        Stop-Job -Job $Job
        Remove-Job -Job $Job
    }
} catch {
    Write-Host "ERROR: Exception occurred while running test: $_" -ForegroundColor Red
}

# Try to get more information about the executable
Write-Host "Attempting to get executable information..." -ForegroundColor Yellow
try {
    $ProcessInfo = New-Object System.Diagnostics.ProcessStartInfo
    $ProcessInfo.FileName = $TestExe
    $ProcessInfo.Arguments = "--version"
    $ProcessInfo.UseShellExecute = $false
    $ProcessInfo.RedirectStandardOutput = $true
    $ProcessInfo.RedirectStandardError = $true
    $ProcessInfo.CreateNoWindow = $true
    
    $Process = New-Object System.Diagnostics.Process
    $Process.StartInfo = $ProcessInfo
    
    if ($Process.Start()) {
        $Process.WaitForExit(5000)  # 5 second timeout
        if ($Process.HasExited) {
            $StdOut = $Process.StandardOutput.ReadToEnd()
            $StdErr = $Process.StandardError.ReadToEnd()
            Write-Host "Process exited with code: $($Process.ExitCode)" -ForegroundColor Cyan
            if ($StdOut) { Write-Host "STDOUT: $StdOut" -ForegroundColor White }
            if ($StdErr) { Write-Host "STDERR: $StdErr" -ForegroundColor Yellow }
        } else {
            Write-Host "Process did not exit within timeout" -ForegroundColor Red
            $Process.Kill()
        }
    } else {
        Write-Host "Failed to start process" -ForegroundColor Red
    }
} catch {
    Write-Host "Exception in process execution: $_" -ForegroundColor Red
}

Write-Host "Memory test diagnostic completed." -ForegroundColor Green
