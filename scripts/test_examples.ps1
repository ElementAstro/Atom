#!/usr/bin/env pwsh
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$target = Join-Path $PSScriptRoot "test" "test_examples.ps1"
if (-not (Test-Path $target)) {
    Write-Error "Script not found: $target"
    exit 1
}

& $target @args
exit $LASTEXITCODE
