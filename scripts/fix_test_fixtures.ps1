# Script to add default constructors and destructors to GoogleTest fixtures
# This fixes linker errors caused by missing test fixture constructors

$testFiles = Get-ChildItem -Path "tests/web" -Filter "*.hpp" -Recurse

foreach ($file in $testFiles) {
    Write-Host "Processing: $($file.FullName)"

    # Read the file content
    $content = Get-Content $file.FullName -Raw

    # Pattern to match test fixture class declarations
    # Matches: class ClassName : public ::testing::Test {
    #          protected:
    #              void SetUp() override {
    $pattern = '(?m)(class\s+(\w+)\s*:\s*public\s+::testing::Test\s*\{\s*protected:\s*)(void\s+SetUp\(\)\s+override\s*\{)'

    # Replacement adds default constructor and destructor
    $replacement = '$1$2() = default;' + "`n" + '    ~$2() override = default;' + "`n`n" + '    $3'

    # Perform the replacement
    $newContent = $content -replace $pattern, $replacement

    # Only write if changes were made
    if ($content -ne $newContent) {
        Set-Content -Path $file.FullName -Value $newContent -NoNewline
        Write-Host "  ✓ Fixed test fixtures in $($file.Name)" -ForegroundColor Green
    } else {
        Write-Host "  - No changes needed for $($file.Name)" -ForegroundColor Gray
    }
}

Write-Host "`nDone! All test fixtures have been updated." -ForegroundColor Cyan
