#pragma once

#include <Arduino.h>
#include <FS.h>

enum class ImageFormat {
  Unknown = 0,
  JPEG,
  PNG,
  BMP,
  GIF,
  QOI
};

// Scan forward from current file position up to maxToScan bytes to find image data.
// Returns true if a supported image magic is found; outputs startOff relative to the scan start,
// and detected image format.
bool findImageStart(fs::File& f, size_t maxToScan, size_t& startOff, ImageFormat& format);

// Read dimensions for the given format from file at dataPos.
// On success returns true and sets outW/outH (best-effort).
bool getImageSize(fs::File& f, size_t dataPos, ImageFormat format, uint32_t& outW, uint32_t& outH);


