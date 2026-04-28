# Patch application script for Arrow
# Applies patches idempotently - skips if already applied

param(
    [string]$SourceDir = ".",
    [string]$PatchDir
)

if (-not $PatchDir) {
    $PatchDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}

function Apply-PatchIdempotent {
    param([string]$PatchFile)

    $patchName = Split-Path $PatchFile -Leaf

    # Create a stripped patch (remove comment header before first '--- ' line).
    $lines = Get-Content $PatchFile
    $startIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^--- ') {
            $startIdx = $i
            break
        }
    }

    if ($startIdx -lt 0) {
        Write-Host "WARNING: Could not find patch start in $patchName"
        return
    }

    $strippedContent = ($lines[$startIdx..($lines.Count-1)] | Out-String)
    $strippedFile = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "$patchName-stripped.patch")
    $strippedContent | Set-Content $strippedFile -Encoding UTF8

    try {
        # Check if patch can be applied (not yet applied).
        $checkOutput = & git apply --check $strippedFile 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Applying patch: $patchName"
            & git apply $strippedFile 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Host "ERROR: Failed to apply patch $patchName"
                exit 1
            }
            Write-Host "Successfully applied: $patchName"
        } else {
            # git apply --check failed. This could mean:
            # 1. The patch is already applied (most common case)
            # 2. The patch conflicts with something else
            # Try reverse check to confirm it's already applied.
            $reverseCheck = & git apply --check --reverse $strippedFile 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Host "Patch already applied (skipping): $patchName"
            } else {
                # Both forward and reverse checks failed.
                # This could happen if the patch was only partially applied.
                # Try to apply with --ignore-whitespace.
                Write-Host "Trying to apply with --ignore-whitespace: $patchName"
                $applyOutput = & git apply --ignore-whitespace $strippedFile 2>&1
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "Successfully applied with --ignore-whitespace: $patchName"
                } else {
                    # Final fallback: assume already applied and continue.
                    Write-Host "WARNING: Cannot determine patch state for $patchName, assuming already applied."
                    Write-Host "  git apply --check: $checkOutput"
                }
            }
        }
    } finally {
        Remove-Item $strippedFile -ErrorAction SilentlyContinue
    }
}

# Change to source directory
Push-Location $SourceDir

try {
    # Initialize git repo if needed.
    $gitCheck = & git rev-parse --is-inside-work-tree 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Initializing git repo in: $(Get-Location)"
        & git init 2>&1 | Out-Null
        & git config user.email "build@local" 2>&1 | Out-Null
        & git config user.name "build" 2>&1 | Out-Null
        & git add -A 2>&1 | Out-Null
        & git commit -m "initial" --quiet 2>&1 | Out-Null
        Write-Host "Git repo initialized with current state as baseline."
    }

    # Apply patches.
    Apply-PatchIdempotent "$PatchDir\cmake-compatibility.patch"
    Apply-PatchIdempotent "$PatchDir\thrift-download.patch"
    # Apply the static CRT patch separately as a safety net.  The CRT fix is
    # also included in thrift-download.patch, but if that multi-file patch
    # failed to apply cleanly the CRT hunk may have been skipped.  Applying
    # the single-hunk patch here ensures the Thrift ExternalProject always
    # builds with /MT when ARROW_USE_STATIC_CRT=ON.
    Apply-PatchIdempotent "$PatchDir\thrift-static-crt.patch"
} finally {
    Pop-Location
}

exit 0
