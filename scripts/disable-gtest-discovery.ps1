# PowerShell script to disable gtest_discover_tests in all test CMakeLists.txt files
# and replace with manual test registration

param(
    [string]$TestsDir = "tests",
    [switch]$DryRun
)

Write-Host "Disabling gtest_discover_tests in test modules..." -ForegroundColor Green

# List of test modules to process
$TestModules = @(
    "io",
    "memory",
    "search",
    "image",
    "web",
    "system",
    "serial",
    "connection",
    "async",
    "utils"
)

# Function to process a CMakeLists.txt file
function Disable-GTestDiscovery {
    param(
        [string]$FilePath,
        [string]$TestName
    )

    if (-not (Test-Path $FilePath)) {
        Write-Warning "File not found: $FilePath"
        return $false
    }

    $content = Get-Content $FilePath -Raw

    # Check if gtest_discover_tests is present and not already commented
    if ($content -match "(?<!#\s*)gtest_discover_tests\($TestName") {
        Write-Host "Processing: $FilePath" -ForegroundColor Yellow

        # Pattern to match gtest_discover_tests block
        $pattern = "(?m)^(\s*)gtest_discover_tests\($TestName\s*\n(.*?\n)*?\s*\)"

        # Replacement with commented version and manual registration
        $replacement = @"
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests($TestName
#     WORKING_DIRECTORY `${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES
#     LABELS "$($TestName.Replace('atom_', '').Replace('_tests', ''))"
#     TIMEOUT 300
# )

# Manual test registration as workaround
add_test(NAME $TestName
    COMMAND $TestName
    WORKING_DIRECTORY `${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties($TestName PROPERTIES
    LABELS "$($TestName.Replace('atom_', '').Replace('_tests', ''))"
    TIMEOUT 300
)
"@

        if ($DryRun) {
            Write-Host "  [DRY RUN] Would replace gtest_discover_tests with manual registration" -ForegroundColor Cyan
            return $true
        }

        # Perform replacement
        $newContent = $content -replace $pattern, $replacement

        if ($newContent -ne $content) {
            Set-Content -Path $FilePath -Value $newContent -NoNewline
            Write-Host "  ✓ Updated: $FilePath" -ForegroundColor Green
            return $true
        } else {
            Write-Warning "  Pattern not matched in: $FilePath"
            return $false
        }
    } else {
        Write-Host "  Skipping (already processed or not found): $FilePath" -ForegroundColor Gray
        return $false
    }
}

# Process each test module
$TotalProcessed = 0
foreach ($Module in $TestModules) {
    $cmakeFile = Join-Path $TestsDir "$Module\CMakeLists.txt"
    $testName = "atom_$($Module)_tests"

    if (Disable-GTestDiscovery -FilePath $cmakeFile -TestName $testName) {
        $TotalProcessed++
    }
}

Write-Host "`nProcessing complete. Total files updated: $TotalProcessed" -ForegroundColor Green

if ($DryRun) {
    Write-Host "[DRY RUN MODE] No files were actually modified." -ForegroundColor Yellow
}
