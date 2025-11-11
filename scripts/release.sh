#!/bin/bash
# Release script for CardPuter MP3 Player
# Usage: ./scripts/release.sh [version] [notes]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get version from VERSION file or argument
if [ -z "$1" ]; then
    if [ -f "VERSION" ]; then
        VERSION=$(cat VERSION | tr -d ' \n')
    else
        echo -e "${RED}Error: No version specified and VERSION file not found${NC}"
        exit 1
    fi
else
    VERSION=$1
fi

# Validate version format (semantic versioning)
if ! [[ $VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo -e "${RED}Error: Invalid version format. Use semantic versioning (e.g., 2.1.0)${NC}"
    exit 1
fi

echo -e "${GREEN}Preparing release v${VERSION}${NC}"

# Check if we're on main branch
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "main" ]; then
    echo -e "${YELLOW}Warning: Not on main branch. Current branch: ${CURRENT_BRANCH}${NC}"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${RED}Error: You have uncommitted changes. Please commit or stash them first.${NC}"
    exit 1
fi

# Update VERSION file
echo "$VERSION" > VERSION
git add VERSION

# Update config.hpp version
sed -i.bak "s/#define FIRMWARE_VERSION_MAJOR [0-9]*/#define FIRMWARE_VERSION_MAJOR $(echo $VERSION | cut -d. -f1)/" include/config.hpp
sed -i.bak "s/#define FIRMWARE_VERSION_MINOR [0-9]*/#define FIRMWARE_VERSION_MINOR $(echo $VERSION | cut -d. -f2)/" include/config.hpp
sed -i.bak "s/#define FIRMWARE_VERSION_PATCH [0-9]*/#define FIRMWARE_VERSION_PATCH $(echo $VERSION | cut -d. -f3)/" include/config.hpp
sed -i.bak "s/#define FIRMWARE_VERSION_STRING \".*\"/#define FIRMWARE_VERSION_STRING \"$VERSION\"/" include/config.hpp
rm -f include/config.hpp.bak
git add include/config.hpp

# Build firmware
echo -e "${GREEN}Building firmware...${NC}"
pio run -e m5stack-cardputer

# Check if build was successful
if [ ! -f ".pio/build/m5stack-cardputer/firmware.bin" ]; then
    echo -e "${RED}Error: Firmware build failed${NC}"
    exit 1
fi

# Create release directory
RELEASE_DIR="releases/v${VERSION}"
mkdir -p "$RELEASE_DIR"

# Copy firmware
cp .pio/build/m5stack-cardputer/firmware.bin "$RELEASE_DIR/firmware_v${VERSION}.bin"

# Generate release notes from CHANGELOG.md
if [ -f "CHANGELOG.md" ]; then
    # Extract version section from CHANGELOG
    awk "/^## \[${VERSION}\]/,/^## \[/" CHANGELOG.md | sed '$d' > "$RELEASE_DIR/RELEASE_NOTES.md"
fi

# Commit version changes
git commit -m "chore: Bump version to ${VERSION}"

# Create git tag
echo -e "${GREEN}Creating git tag v${VERSION}...${NC}"
git tag -a "v${VERSION}" -m "Release v${VERSION}"

echo -e "${GREEN}Release v${VERSION} prepared successfully!${NC}"
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Review the release:"
echo "   - Firmware: $RELEASE_DIR/firmware_v${VERSION}.bin"
echo "   - Notes: $RELEASE_DIR/RELEASE_NOTES.md"
echo ""
echo "2. Push to GitHub:"
echo "   git push origin main"
echo "   git push origin v${VERSION}"
echo ""
echo "3. Create GitHub Release:"
echo "   - Go to: https://github.com/vicliu624/CardPuter_Mp3_Adv/releases/new"
echo "   - Tag: v${VERSION}"
echo "   - Title: Release v${VERSION}"
echo "   - Description: Copy from $RELEASE_DIR/RELEASE_NOTES.md"
echo "   - Upload: $RELEASE_DIR/firmware_v${VERSION}.bin"

