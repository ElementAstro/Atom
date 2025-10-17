# Simple test runner for examples - runs each example directly with timeout
param(
    [int]$TimeoutSeconds = 10,
    [string]$Module = ""
)

$ErrorActionPreference = "Continue"
$resultsDir = "build/example_test_results"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

# Create results directory
New-Item -ItemType Directory -Force -Path $resultsDir | Out-Null

Write-Host "=== ATOM EXAMPLE SIMPLE TEST SUITE ===" -ForegroundColor Green
Write-Host "Timeout: $TimeoutSeconds seconds`n" -ForegroundColor Cyan

# Add Atom DLL directories to PATH
$atomDllDirs = Get-ChildItem -Recurse build/atom -Filter "*.dll" |
    Select-Object -ExpandProperty DirectoryName -Unique
$env:PATH = ($atomDllDirs -join ";") + ";" + $env:PATH

# Get all example executables
$exes = Get-ChildItem -Recurse build/example -Filter *.exe |
    Where-Object { $_.Directory.Name -notmatch "CMakeFiles|CompilerId" } |
    Sort-Object Directory, Name

if ($Module) {
    $exes = $exes | Where-Object { $_.Directory.Name -eq $Module }
}

Write-Host "Found $($exes.Count) examples to test`n" -ForegroundColor Cyan

$passed = 0
$failed = 0
$timeout = 0
$crashed = 0

$testNumber = 0
foreach ($exe in $exes) {
    $testNumber++
    $moduleName = $exe.Directory.Name
    $exeName = $exe.Name

    Write-Host "[$testNumber/$($exes.Count)] $moduleName/$exeName" -NoNewline

    try {
        # Start process with timeout
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $exe.FullName
        $psi.WorkingDirectory = $exe.DirectoryName
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true

        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $psi
        $process.Start() | Out-Null

        $finished = $process.WaitForExit($TimeoutSeconds * 1000)

        if ($finished) {
            if ($process.ExitCode -eq 0) {
                Write-Host " ✓ PASSED" -ForegroundColor Green
                $passed++
            } else {
                Write-Host " ✗ FAILED (exit: $($process.ExitCode))" -ForegroundColor Red
                $failed++
            }
        } else {
            Write-Host " ⏱ TIMEOUT" -ForegroundColor Magenta
            $timeout++
            $process.Kill()
        }

        $process.Dispose()

    } catch {
        Write-Host " ✗ CRASHED: $($_.Exception.Message)" -ForegroundColor Red
        $crashed++
    }
}

Write-Host "`n=== TEST SUMMARY ===" -ForegroundColor Green
Write-Host "Total: $($exes.Count)" -ForegroundColor Cyan
Write-Host "Passed: $passed ($([math]::Round($passed / $exes.Count * 100, 1))%)" -ForegroundColor Green
Write-Host "Failed: $failed ($([math]::Round($failed / $exes.Count * 100, 1))%)" -ForegroundColor Red
Write-Host "Timeout: $timeout ($([math]::Round($timeout / $exes.Count * 100, 1))%)" -ForegroundColor Magenta
Write-Host "Crashed: $crashed ($([math]::Round($crashed / $exes.Count * 100, 1))%)" -ForegroundColor Red

if ($failed -gt 0 -or $timeout -gt 0 -or $crashed -gt 0) {
    exit 1
} else {
    exit 0
}
