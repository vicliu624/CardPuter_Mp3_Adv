#include "../include/image_utils.hpp"

static inline bool match(const uint8_t* buf, size_t len, const uint8_t* sig, size_t siglen) {
  if (len < siglen) return false;
  return memcmp(buf, sig, siglen) == 0;
}

bool findImageStart(fs::File& f, size_t maxToScan, size_t& startOff, ImageFormat& format) {
  const uint8_t sigJpg[2] = {0xFF, 0xD8};
  const uint8_t sigPng[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  const uint8_t sigBmp[2] = {'B','M'};
  const uint8_t sigGif1[6] = {'G','I','F','8','9','a'};
  const uint8_t sigGif0[6] = {'G','I','F','8','7','a'};
  const uint8_t sigQoi[4] = {'q','o','i','f'};

  uint8_t buf[512];
  size_t scanned = 0;
  startOff = SIZE_MAX;
  format = ImageFormat::Unknown;

  while (scanned < maxToScan) {
    size_t toRead = maxToScan - scanned;
    if (toRead > sizeof(buf)) toRead = sizeof(buf);
    size_t rd = f.read(buf, toRead);
    if (rd == 0) break;
    // JPEG
    for (size_t i = 0; i + 1 < rd; ++i) {
      if (buf[i] == sigJpg[0] && buf[i+1] == sigJpg[1]) {
        startOff = scanned + i; format = ImageFormat::JPEG; break;
      }
    }
    // PNG
    if (startOff == SIZE_MAX) {
      for (size_t i = 0; i + sizeof(sigPng) <= rd; ++i) {
        if (match(&buf[i], rd - i, sigPng, sizeof(sigPng))) { startOff = scanned + i; format = ImageFormat::PNG; break; }
      }
    }
    // BMP
    if (startOff == SIZE_MAX) {
      for (size_t i = 0; i + 2 <= rd; ++i) {
        if (buf[i] == sigBmp[0] && buf[i+1] == sigBmp[1]) { startOff = scanned + i; format = ImageFormat::BMP; break; }
      }
    }
    // GIF
    if (startOff == SIZE_MAX) {
      for (size_t i = 0; i + 6 <= rd; ++i) {
        if (match(&buf[i], rd - i, sigGif1, 6) || match(&buf[i], rd - i, sigGif0, 6)) { startOff = scanned + i; format = ImageFormat::GIF; break; }
      }
    }
    // QOI
    if (startOff == SIZE_MAX) {
      for (size_t i = 0; i + 4 <= rd; ++i) {
        if (match(&buf[i], rd - i, sigQoi, 4)) { startOff = scanned + i; format = ImageFormat::QOI; break; }
      }
    }
    if (startOff != SIZE_MAX) break;
    scanned += rd;
  }
  if (startOff == SIZE_MAX) { startOff = 0; format = ImageFormat::Unknown; }
  return format != ImageFormat::Unknown;
}

bool getImageSize(fs::File& f, size_t dataPos, ImageFormat format, uint32_t& outW, uint32_t& outH) {
  outW = outH = 0;
  uint8_t head[32];
  f.seek(dataPos);
  size_t hdr = f.read(head, sizeof(head));
  if (format == ImageFormat::PNG && hdr >= 24) {
    outW = (head[16] << 24) | (head[17] << 16) | (head[18] << 8) | head[19];
    outH = (head[20] << 24) | (head[21] << 16) | (head[22] << 8) | head[23];
    return true;
  }
  if (format == ImageFormat::BMP && hdr >= 26) {
    outW = (uint32_t)head[18] | ((uint32_t)head[19] << 8) | ((uint32_t)head[20] << 16) | ((uint32_t)head[21] << 24);
    outH = (uint32_t)head[22] | ((uint32_t)head[23] << 8) | ((uint32_t)head[24] << 16) | ((uint32_t)head[25] << 24);
    if ((int32_t)outH < 0) outH = (uint32_t)(-(int32_t)outH);
    return true;
  }
  if (format == ImageFormat::GIF && hdr >= 10) {
    outW = (uint32_t)head[6] | ((uint32_t)head[7] << 8);
    outH = (uint32_t)head[8] | ((uint32_t)head[9] << 8);
    return true;
  }
  if (format == ImageFormat::QOI && hdr >= 12) {
    outW = ((uint32_t)head[4] << 24) | ((uint32_t)head[5] << 16) | ((uint32_t)head[6] << 8) | (uint32_t)head[7];
    outH = ((uint32_t)head[8] << 24) | ((uint32_t)head[9] << 16) | ((uint32_t)head[10] << 8) | (uint32_t)head[11];
    return true;
  }
  if (format == ImageFormat::JPEG) {
    // Scan for SOF0 / SOF2
    // Some JPEGs have long EXIF data, so we need to scan more bytes
    size_t scannedJ = hdr;
    const size_t jpegScanMax = 16384;  // Increased from 4096 to handle JPEGs with long EXIF
    while (scannedJ < jpegScanMax) {
      int c1 = f.read(); if (c1 < 0) break;
      if ((uint8_t)c1 != 0xFF) { scannedJ++; continue; }
      int c2 = f.read(); if (c2 < 0) break; scannedJ += 2;
      if (c2 == 0xC0 || c2 == 0xC2) {
        // Found SOF marker, read dimensions
        int lenH = f.read(); if (lenH < 0) break;
        int lenL = f.read(); if (lenL < 0) break;
        int prec = f.read(); if (prec < 0) break;
        int hH = f.read(); if (hH < 0) break;
        int hL = f.read(); if (hL < 0) break;
        int wH = f.read(); if (wH < 0) break;
        int wL = f.read(); if (wL < 0) break;
        outH = ((uint32_t)(uint8_t)hH << 8) | (uint8_t)hL; 
        outW = ((uint32_t)(uint8_t)wH << 8) | (uint8_t)wL;
        if (outW > 0 && outH > 0) return true;  // Valid dimensions
      } else if (c2 >= 0xE0 && c2 <= 0xEF) {
        // APP markers (EXIF, etc.) - skip segment
        int lh = f.read(); if (lh < 0) break;
        int ll = f.read(); if (ll < 0) break;
        uint16_t seglen = ((uint16_t)(uint8_t)lh << 8) | (uint8_t)ll;
        if (seglen < 2) break;
        f.seek(f.position() + seglen - 2);
        scannedJ += seglen;
      } else if (c2 != 0xFF && c2 != 0x00) {
        // Other markers - skip segment
        int lh = f.read(); if (lh < 0) break;
        int ll = f.read(); if (ll < 0) break;
        uint16_t seglen = ((uint16_t)(uint8_t)lh << 8) | (uint8_t)ll;
        if (seglen < 2) break;
        f.seek(f.position() + seglen - 2);
        scannedJ += seglen;
      }
    }
  }
  return false;
}

bool getImageSizeFromBuffer(const uint8_t* buf, size_t bufSize, ImageFormat format, uint32_t& outW, uint32_t& outH) {
  outW = outH = 0;
  if (!buf || bufSize < 16) return false;
  
  if (format == ImageFormat::PNG && bufSize >= 24) {
    // PNG: width at offset 16, height at offset 20 (IHDR chunk)
    outW = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19];
    outH = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
    return true;
  }
  
  if (format == ImageFormat::BMP && bufSize >= 26) {
    outW = (uint32_t)buf[18] | ((uint32_t)buf[19] << 8) | ((uint32_t)buf[20] << 16) | ((uint32_t)buf[21] << 24);
    outH = (uint32_t)buf[22] | ((uint32_t)buf[23] << 8) | ((uint32_t)buf[24] << 16) | ((uint32_t)buf[25] << 24);
    if ((int32_t)outH < 0) outH = (uint32_t)(-(int32_t)outH);
    return true;
  }
  
  if (format == ImageFormat::GIF && bufSize >= 10) {
    outW = (uint32_t)buf[6] | ((uint32_t)buf[7] << 8);
    outH = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8);
    return true;
  }
  
  if (format == ImageFormat::QOI && bufSize >= 12) {
    outW = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
    outH = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];
    return true;
  }
  
  if (format == ImageFormat::JPEG) {
    // Scan for SOF0 (0xFF 0xC0) or SOF2 (0xFF 0xC2)
    const size_t jpegScanMax = (bufSize < 4096) ? bufSize : 4096;
    for (size_t i = 2; i + 8 < jpegScanMax; ++i) {
      if (buf[i] == 0xFF && (buf[i+1] == 0xC0 || buf[i+1] == 0xC2)) {
        // Skip length (2 bytes) and precision (1 byte)
        outH = ((uint32_t)buf[i+5] << 8) | buf[i+6];
        outW = ((uint32_t)buf[i+7] << 8) | buf[i+8];
        return true;
      }
    }
  }
  
  return false;
}


