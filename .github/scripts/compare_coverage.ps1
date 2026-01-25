# Coverage Comparison Script (PowerShell)
# Compares current coverage with base branch to detect decreases

param(
    [Parameter(Mandatory=$true)]
    [double]$CurrentCoverage,

    [Parameter(Mandatory=$true)]
    [double]$BaseCoverage,

    [Parameter(Mandatory=$false)]
    [string]$OutputFile = "coverage_comparison.txt"
)

# Configuration
$THRESHOLD_WARNING = 0.5  # Warn if coverage drops by more than 0.5%
$THRESHOLD_ERROR = 1.0    # Error if coverage drops by more than 1.0%

# Calculate difference
$Diff = $CurrentCoverage - $BaseCoverage
$AbsDiff = [Math]::Abs($Diff)

# Output to console and file
$output = @"
=== Coverage Comparison ===
Current Coverage: $CurrentCoverage%
Base Coverage: $BaseCoverage%
Difference: $Diff%

"@

Write-Host $output
$output | Out-File -FilePath $OutputFile -Encoding UTF8

# Determine status
if ($Diff -lt 0) {
    # Coverage decreased
    if ($AbsDiff -gt $THRESHOLD_ERROR) {
        Write-Host "❌ COVERAGE DECREASE ERROR" -ForegroundColor Red
        Write-Host "Coverage dropped by $AbsDiff% (threshold: $THRESHOLD_ERROR%)"
        Write-Host "::error::Coverage decreased by $AbsDiff% (from $BaseCoverage% to $CurrentCoverage%)"
        "❌ COVERAGE DECREASE ERROR" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
        "Coverage dropped by $AbsDiff% (threshold: $THRESHOLD_ERROR%)" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
        exit 1
    }
    elseif ($AbsDiff -gt $THRESHOLD_WARNING) {
        Write-Host "⚠️  COVERAGE DECREASE WARNING" -ForegroundColor Yellow
        Write-Host "Coverage dropped by $AbsDiff% (threshold: $THRESHOLD_WARNING%)"
        Write-Host "::warning::Coverage decreased by $AbsDiff% (from $BaseCoverage% to $CurrentCoverage%)"
        "⚠️  COVERAGE DECREASE WARNING" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
        "Coverage dropped by $AbsDiff% (threshold: $THRESHOLD_WARNING%)" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
        exit 0
    }
    else {
        Write-Host "ℹ️  Coverage decreased slightly" -ForegroundColor Yellow
        Write-Host "Coverage dropped by $AbsDiff% (within acceptable range)"
        "ℹ️  Coverage decreased slightly" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
        "Coverage dropped by $AbsDiff% (within acceptable range)" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
        exit 0
    }
}
elseif ($Diff -gt 0) {
    # Coverage increased
    Write-Host "✅ COVERAGE IMPROVED" -ForegroundColor Green
    Write-Host "Coverage increased by $Diff%"
    "✅ COVERAGE IMPROVED" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
    "Coverage increased by $Diff%" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
    exit 0
}
else {
    # Coverage unchanged
    Write-Host "✅ COVERAGE UNCHANGED" -ForegroundColor Green
    Write-Host "Coverage remains at $CurrentCoverage%"
    "✅ COVERAGE UNCHANGED" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
    "Coverage remains at $CurrentCoverage%" | Out-File -FilePath $OutputFile -Append -Encoding UTF8
    exit 0
}
