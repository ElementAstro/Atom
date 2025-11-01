# Fix remaining MSVC build issues

Write-Host "Fixing remaining compilation issues..." -ForegroundColor Cyan

# 1. Fix uuid.cpp - Windows-specific issues
Write-Host "1. Fixing uuid.cpp Windows-specific issues..." -ForegroundColor Yellow

$uuidCpp = "atom\utils\random\uuid.cpp"
if (Test-Path $uuidCpp) {
    $content = Get-Content $uuidCpp -Raw

    # Add Windows header for GetCurrentProcessId
    if ($content -notmatch "#include <process.h>") {
        $content = $content -replace '(#include <openssl/evp.h>)', "`$1`r`n#ifdef _WIN32`r`n#include <process.h>  // For _getpid on Windows`r`n#define getpid _getpid`r`n#endif"
    }

    # Fix max() macro conflict
    $content = $content -replace '\bmax\(', '(std::max)('

    # Fix uniform_int_distribution<uint8_t> (not allowed in MSVC)
    $content = $content -replace 'std::uniform_int_distribution<uint8_t>', 'std::uniform_int_distribution<unsigned int>'
    $content = $content -replace 'distribution\(gen\)', 'static_cast<uint8_t>(distribution(gen))'

    Set-Content $uuidCpp -Value $content -NoNewline
    Write-Host "  Fixed uuid.cpp" -ForegroundColor Green
}

# 2. Fix error_stack.cpp - std::ranges::contains not in C++20
Write-Host "2. Fixing error_stack.cpp std::ranges::contains..." -ForegroundColor Yellow

$errorStackCpp = "atom\utils\debug\error_stack.cpp"
if (Test-Path $errorStackCpp) {
    $content = Get-Content $errorStackCpp -Raw

    # Replace std::ranges::contains with std::find
    $content = $content -replace 'std::ranges::contains\(([^,]+),\s*([^)]+)\)',
        '(std::find($1.begin(), $1.end(), $2) != $1.end())'

    Set-Content $errorStackCpp -Value $content -NoNewline
    Write-Host "  Fixed error_stack.cpp" -ForegroundColor Green
}

# 3. Fix ser_format.h chrono issue
Write-Host "3. Fixing ser_format.h chrono type conversion..." -ForegroundColor Yellow

$serFormatH = "atom\image\formats\ser\ser_format.h"
if (Test-Path $serFormatH) {
    $content = Get-Content $serFormatH -Raw

    # Fix chrono duration cast
    $content = $content -replace 'return\s+std::chrono::system_clock::now\(\);',
        'return std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::system_clock::now());'

    Set-Content $serFormatH -Value $content -NoNewline
    Write-Host "  Fixed ser_format.h" -ForegroundColor Green
}

Write-Host "`nAll fixes applied!" -ForegroundColor Green
Write-Host "Run cmake --build build-msvc --config Release to test the build" -ForegroundColor Cyan
