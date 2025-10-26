# Remove header guards from .cpp files that were converted from .hpp files
# This script removes #ifndef, #define, and #endif header guard patterns

$files = Get-ChildItem -Path "tests/web" -Filter "*.cpp" -Recurse

foreach ($file in $files) {
    Write-Host "Processing: $($file.Name)"

    $content = Get-Content $file.FullName -Raw

    # Remove header guard pattern at the beginning
    # Pattern: // filepath: ... \n #ifndef ... \n #define ...
    $content = $content -replace '(?s)^(//\s*filepath:.*?\n)?#ifndef\s+\w+\s*\n#define\s+\w+\s*\n', ''

    # Remove #endif at the end (with optional comment)
    $content = $content -replace '\n#endif\s*(//.*?)?\s*$', ''

    # Write back to file
    Set-Content -Path $file.FullName -Value $content -NoNewline

    Write-Host "  Cleaned: $($file.Name)"
}

Write-Host "`nDone! Processed $($files.Count) files."
