# Release script for CardPuter MP3 Player (PowerShell version)
# Usage: .\scripts\release.ps1 [version] [notes]

param(
    [string]$Version = "",
    [string]$Notes = ""
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-ColorOutput($ForegroundColor) {
    $fc = $host.UI.RawUI.ForegroundColor
    $host.UI.RawUI.ForegroundColor = $ForegroundColor
    if ($args) {
        Write-Output $args
    }
    $host.UI.RawUI.ForegroundColor = $fc
}

# Get version from VERSION file or argument
if ([string]::IsNullOrEmpty($Version)) {
    if (Test-Path "VERSION") {
        $Version = (Get-Content "VERSION" -Raw).Trim()
    } else {
        Write-ColorOutput Red "Error: No version specified and VERSION file not found"
        exit 1
    }
}

# Validate version format (semantic versioning)
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-ColorOutput Red "Error: Invalid version format. Use semantic versioning (e.g., 2.1.0)"
    exit 1
}

Write-ColorOutput Green "Preparing release v$Version"

# Check if we're on main branch
$CurrentBranch = git branch --show-current
if ($CurrentBranch -ne "main") {
    Write-ColorOutput Yellow "Warning: Not on main branch. Current branch: $CurrentBranch"
    $response = Read-Host "Continue anyway? (y/n)"
    if ($response -ne "y" -and $response -ne "Y") {
        exit 1
    }
}

# Check for uncommitted changes
$status = git status --porcelain
if ($status) {
    Write-ColorOutput Red "Error: You have uncommitted changes. Please commit or stash them first."
    exit 1
}

# Update VERSION file
$Version | Out-File -FilePath "VERSION" -Encoding utf8 -NoNewline
git add VERSION

# Update config.hpp version
$configPath = "include/config.hpp"
$configContent = Get-Content $configPath -Raw
$major = $Version.Split('.')[0]
$minor = $Version.Split('.')[1]
$patch = $Version.Split('.')[2]

$configContent = $configContent -replace '#define FIRMWARE_VERSION_MAJOR \d+', "#define FIRMWARE_VERSION_MAJOR $major"
$configContent = $configContent -replace '#define FIRMWARE_VERSION_MINOR \d+', "#define FIRMWARE_VERSION_MINOR $minor"
$configContent = $configContent -replace '#define FIRMWARE_VERSION_PATCH \d+', "#define FIRMWARE_VERSION_PATCH $patch"
$configContent = $configContent -replace '#define FIRMWARE_VERSION_STRING ".*"', "#define FIRMWARE_VERSION_STRING `"$Version`""

$configContent | Out-File -FilePath $configPath -Encoding utf8 -NoNewline
git add $configPath

# Build firmware
Write-ColorOutput Green "Building firmware..."
pio run -e m5stack-cardputer

# Check if build was successful
$firmwarePath = ".pio\build\m5stack-cardputer\firmware.bin"
if (-not (Test-Path $firmwarePath)) {
    Write-ColorOutput Red "Error: Firmware build failed"
    exit 1
}

# Create release directory
$ReleaseDir = "releases\v$Version"
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null

# Copy firmware
Copy-Item $firmwarePath "$ReleaseDir\firmware_v$Version.bin"

# Generate release notes from CHANGELOG.md
if (Test-Path "CHANGELOG.md") {
    $changelog = Get-Content "CHANGELOG.md" -Raw
    $pattern = "(?s)## \[$Version\].*?(?=## \[|$)"
    if ($changelog -match $pattern) {
        $releaseNotes = $matches[0]
        $releaseNotes | Out-File -FilePath "$ReleaseDir\RELEASE_NOTES.md" -Encoding utf8
    }
}

# Commit version changes
git commit -m "chore: Bump version to $Version"

# Create git tag
Write-ColorOutput Green "Creating git tag v$Version..."
git tag -a "v$Version" -m "Release v$Version"

Write-ColorOutput Green "Release v$Version prepared successfully!"
Write-ColorOutput Yellow "Next steps:"
Write-Output "1. Review the release:"
Write-Output "   - Firmware: $ReleaseDir\firmware_v$Version.bin"
Write-Output "   - Notes: $ReleaseDir\RELEASE_NOTES.md"
Write-Output ""
Write-Output "2. Push to GitHub:"
Write-Output "   git push origin main"
Write-Output "   git push origin v$Version"
Write-Output ""
Write-Output "3. Create GitHub Release:"
Write-Output "   - Go to: https://github.com/vicliu624/CardPuter_Mp3_Adv/releases/new"
Write-Output "   - Tag: v$Version"
Write-Output "   - Title: Release v$Version"
Write-Output "   - Description: Copy from $ReleaseDir\RELEASE_NOTES.md"
Write-Output "   - Upload: $ReleaseDir\firmware_v$Version.bin"

