# Test all built examples and capture results
# Usage: .\scripts\test_examples.ps1 [-TimeoutSeconds 30] [-Module "algorithm"]

param(
    [int]$TimeoutSeconds = 30,
    [string]$Module = "",
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"
$resultsDir = "build/example_test_results"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultsFile = "$resultsDir/test_results_$timestamp.json"
$summaryFile = "$resultsDir/test_summary_$timestamp.txt"

# Create results directory
New-Item -ItemType Directory -Force -Path $resultsDir | Out-Null

# Initialize results
$results = @{
    Timestamp = $timestamp
    TotalTests = 0
    Passed = 0
    Failed = 0
    Timeout = 0
    Crashed = 0
    Tests = @()
}

Write-Host "=== ATOM EXAMPLE TEST SUITE ===" -ForegroundColor Green
Write-Host "Timeout: $TimeoutSeconds seconds" -ForegroundColor Cyan
Write-Host "Results will be saved to: $resultsFile`n" -ForegroundColor Cyan

# Get all example executables
$exes = Get-ChildItem -Recurse build/example -Filter *.exe | 
    Where-Object { $_.Directory.Name -notmatch "CMakeFiles|CompilerId" } |
    Sort-Object Directory, Name

# Filter by module if specified
if ($Module) {
    $exes = $exes | Where-Object { $_.Directory.Name -eq $Module }
    Write-Host "Testing only module: $Module`n" -ForegroundColor Yellow
}

$results.TotalTests = $exes.Count

Write-Host "Found $($exes.Count) examples to test`n" -ForegroundColor Cyan

# Add Atom DLL directories to PATH
$atomDllDirs = Get-ChildItem -Recurse build/atom -Filter "*.dll" |
    Select-Object -ExpandProperty DirectoryName -Unique
$env:PATH = ($atomDllDirs -join ";") + ";" + $env:PATH
Write-Host "Added $($atomDllDirs.Count) DLL directories to PATH`n" -ForegroundColor Cyan

# Test each example
$testNumber = 0
foreach ($exe in $exes) {
    $testNumber++
    $moduleName = $exe.Directory.Name
    $exeName = $exe.Name
    $exePath = $exe.FullName
    
    Write-Host "[$testNumber/$($exes.Count)] Testing: $moduleName/$exeName" -ForegroundColor Yellow
    
    $testResult = @{
        Module = $moduleName
        Name = $exeName
        Path = $exePath
        Status = "UNKNOWN"
        ExitCode = $null
        Duration = 0
        Output = ""
        Error = ""
        Message = ""
    }
    
    $startTime = Get-Date
    
    try {
        # Create a job to run the executable with timeout
        $job = Start-Job -ScriptBlock {
            param($exePath, $workDir)
            Set-Location $workDir
            $process = Start-Process -FilePath $exePath -NoNewWindow -Wait -PassThru -RedirectStandardOutput "temp_stdout.txt" -RedirectStandardError "temp_stderr.txt"
            return @{
                ExitCode = $process.ExitCode
                StdOut = Get-Content "temp_stdout.txt" -Raw -ErrorAction SilentlyContinue
                StdErr = Get-Content "temp_stderr.txt" -Raw -ErrorAction SilentlyContinue
            }
        } -ArgumentList $exePath, $exe.DirectoryName
        
        # Wait for job with timeout
        $completed = Wait-Job -Job $job -Timeout $TimeoutSeconds
        
        if ($completed) {
            $jobResult = Receive-Job -Job $job
            $testResult.ExitCode = $jobResult.ExitCode
            $testResult.Output = $jobResult.StdOut
            $testResult.Error = $jobResult.StdErr
            
            if ($jobResult.ExitCode -eq 0) {
                $testResult.Status = "PASSED"
                $testResult.Message = "Executed successfully"
                $results.Passed++
                Write-Host "  ✓ PASSED (exit code: 0)" -ForegroundColor Green
            } else {
                $testResult.Status = "FAILED"
                $testResult.Message = "Non-zero exit code: $($jobResult.ExitCode)"
                $results.Failed++
                Write-Host "  ✗ FAILED (exit code: $($jobResult.ExitCode))" -ForegroundColor Red
            }
        } else {
            # Timeout occurred
            $testResult.Status = "TIMEOUT"
            $testResult.Message = "Execution exceeded $TimeoutSeconds seconds"
            $results.Timeout++
            Write-Host "  ⏱ TIMEOUT (exceeded $TimeoutSeconds seconds)" -ForegroundColor Magenta
            Stop-Job -Job $job
        }
        
        Remove-Job -Job $job -Force
        
    } catch {
        $testResult.Status = "CRASHED"
        $testResult.Message = $_.Exception.Message
        $testResult.Error = $_.Exception.ToString()
        $results.Crashed++
        Write-Host "  ✗ CRASHED: $($_.Exception.Message)" -ForegroundColor Red
    }
    
    $endTime = Get-Date
    $testResult.Duration = ($endTime - $startTime).TotalSeconds
    
    if ($Verbose -and $testResult.Output) {
        $outputStr = [string]$testResult.Output
        if ($outputStr.Length -gt 200) {
            Write-Host "  Output: $($outputStr.Substring(0, 200))..." -ForegroundColor Gray
        } else {
            Write-Host "  Output: $outputStr" -ForegroundColor Gray
        }
    }
    
    $results.Tests += $testResult
    
    # Clean up temp files
    Remove-Item -Path "$($exe.DirectoryName)/temp_stdout.txt" -ErrorAction SilentlyContinue
    Remove-Item -Path "$($exe.DirectoryName)/temp_stderr.txt" -ErrorAction SilentlyContinue
}

# Save results to JSON
$results | ConvertTo-Json -Depth 10 | Out-File -FilePath $resultsFile -Encoding UTF8

# Generate summary report
$summary = @"
=== ATOM EXAMPLE TEST SUMMARY ===
Timestamp: $timestamp
Total Tests: $($results.TotalTests)
Passed: $($results.Passed) ($([math]::Round($results.Passed / $results.TotalTests * 100, 2))%)
Failed: $($results.Failed) ($([math]::Round($results.Failed / $results.TotalTests * 100, 2))%)
Timeout: $($results.Timeout) ($([math]::Round($results.Timeout / $results.TotalTests * 100, 2))%)
Crashed: $($results.Crashed) ($([math]::Round($results.Crashed / $results.TotalTests * 100, 2))%)

=== FAILED TESTS ===
"@

$failedTests = $results.Tests | Where-Object { $_.Status -ne "PASSED" }
foreach ($test in $failedTests) {
    $summary += "`n[$($test.Status)] $($test.Module)/$($test.Name)"
    $summary += "`n  Message: $($test.Message)"
    if ($test.Error) {
        $errorStr = [string]$test.Error
        if ($errorStr.Length -gt 200) {
            $summary += "`n  Error: $($errorStr.Substring(0, 200))"
        } else {
            $summary += "`n  Error: $errorStr"
        }
    }
}

$summary | Out-File -FilePath $summaryFile -Encoding UTF8

# Display summary
Write-Host "`n=== TEST SUMMARY ===" -ForegroundColor Green
Write-Host "Total Tests: $($results.TotalTests)" -ForegroundColor Cyan
Write-Host "Passed: $($results.Passed) ($([math]::Round($results.Passed / $results.TotalTests * 100, 2))%)" -ForegroundColor Green
Write-Host "Failed: $($results.Failed) ($([math]::Round($results.Failed / $results.TotalTests * 100, 2))%)" -ForegroundColor Red
Write-Host "Timeout: $($results.Timeout) ($([math]::Round($results.Timeout / $results.TotalTests * 100, 2))%)" -ForegroundColor Magenta
Write-Host "Crashed: $($results.Crashed) ($([math]::Round($results.Crashed / $results.TotalTests * 100, 2))%)" -ForegroundColor Red

Write-Host "`nResults saved to:" -ForegroundColor Cyan
Write-Host "  JSON: $resultsFile" -ForegroundColor Gray
Write-Host "  Summary: $summaryFile" -ForegroundColor Gray

# Return exit code based on results
if ($results.Failed -gt 0 -or $results.Timeout -gt 0 -or $results.Crashed -gt 0) {
    exit 1
} else {
    exit 0
}

