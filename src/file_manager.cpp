#include "../include/file_manager.hpp"
#include "../include/config.hpp"
#include <SD.h>
#include "M5Cardputer.h"
#include <ESP32Time.h>
#include <cstdio>

namespace FileManager {

void listFiles(fs::FS& fs, const char* dirname, uint8_t levels, AppState& appState) {
  LOG_PRINTF("Listing directory: %s\n", dirname);
  // Ensure dirname starts with '/'
  String dir = String(dirname);
  if (!dir.startsWith("/")) dir = String("/") + dir;
  File root = fs.open(dir.c_str());
  if (!root) {
    LOG_PRINTLN("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    LOG_PRINTLN("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file && appState.fileCount < MAX_FILES) {
    if (file.isDirectory()) {
      // Build subdirectory path ensuring single leading '/'
      String sub = String(file.name());
      if (!sub.startsWith("/")) sub = dir + String("/") + sub;
      LOG_PRINT("DIR : ");
      LOG_PRINTLN(sub.c_str());
      if (levels) {
        listFiles(fs, sub.c_str(), levels - 1, appState);
      }
    } else {
      // Build full file path
      String fname = String(file.name());
      DEBUG_PRINTF("DEBUG: file.name() = '%s', dir = '%s'\n", fname.c_str(), dir.c_str());
      if (!fname.startsWith("/")) fname = dir + String("/") + fname;
      DEBUG_PRINTF("DEBUG: after path build, fname = '%s'\n", fname.c_str());
      // Filter by supported extensions: .mp3 and .wav
      String lower = fname; lower.toLowerCase();
      int dot = lower.lastIndexOf('.');
      bool supported = false;
      if (dot >= 0) {
        String ext = lower.substring(dot + 1);
        if (ext == "mp3" || ext == "wav") supported = true;
      }
      if (supported) {
        LOG_PRINT("FILE: ");
        LOG_PRINTLN(fname.c_str());
        appState.audioFiles[appState.fileCount] = fname;
        LOG_PRINTF("Stored audioFiles[%d] = %s\n", appState.fileCount, appState.audioFiles[appState.fileCount].c_str());
        appState.fileCount++;
      } else {
        LOG_PRINTF("SKIP (unsupported): %s\n", fname.c_str());
      }
    }
    file = root.openNextFile();
  }
}

void deleteCurrentFile(fs::FS& fs, AppState& appState, const Callbacks& callbacks) {
  if (appState.fileCount == 0 || appState.currentSelectedIndex >= appState.fileCount) {
    LOG_PRINTLN("No file to delete");
    return;
  }
  
  int deleteIndex = appState.currentSelectedIndex;  // File index to delete (currently selected)
  String fileToDelete = appState.audioFiles[deleteIndex];
  LOG_PRINTF("Attempting to delete: %s (index %d)\n", fileToDelete.c_str(), deleteIndex);
  
  // Record if playing before delete, and which song is playing
  bool wasPlaying = (appState.isPlaying && !appState.stopped);
  int playingIndexBeforeDelete = appState.currentPlayingIndex;
  bool deletingPlayingSong = (deleteIndex == playingIndexBeforeDelete);
  
  // Delete the file from SD card
  if (fs.remove(fileToDelete)) {
    LOG_PRINTF("File deleted successfully: %s\n", fileToDelete.c_str());
  } else {
    LOG_PRINTF("Failed to delete file: %s\n", fileToDelete.c_str());
    return;
  }
  
  // Remove file from list
  for (int i = deleteIndex; i < appState.fileCount - 1; i++) {
    appState.audioFiles[i] = appState.audioFiles[i + 1];
  }
  appState.fileCount--;
  
  // Adjust playing index: if delete index <= playing index, playing index needs to decrease by 1
  int playingIndexAfterDelete = playingIndexBeforeDelete;
  if (deleteIndex <= playingIndexBeforeDelete) {
    playingIndexAfterDelete--;  // Playing index moves forward
    if (playingIndexAfterDelete < 0 && appState.fileCount > 0) playingIndexAfterDelete = 0;
    if (playingIndexAfterDelete >= appState.fileCount && appState.fileCount > 0) playingIndexAfterDelete = appState.fileCount - 1;
  }
  
  // Adjust current selected index
  if (deleteIndex < appState.currentSelectedIndex) {
    appState.currentSelectedIndex--;  // Deleted file is before current position, index moves forward
  } else if (deleteIndex == appState.currentSelectedIndex) {
    // Deleted the currently selected song, need to adjust selected index
    if (appState.currentSelectedIndex >= appState.fileCount) {
      appState.currentSelectedIndex = appState.fileCount - 1;
    }
    if (appState.currentSelectedIndex < 0) {
      appState.currentSelectedIndex = 0;
    }
  }
  // If deleteIndex > currentSelectedIndex, selection remains unchanged (deleted file is after current position)
  
  // Ensure currentSelectedIndex is within valid range
  if (appState.currentSelectedIndex < 0) appState.currentSelectedIndex = 0;
  if (appState.currentSelectedIndex >= appState.fileCount && appState.fileCount > 0) appState.currentSelectedIndex = appState.fileCount - 1;
  
  // If deleting currently playing song, need to switch to new song
  if (deletingPlayingSong) {
    // Playing index before delete has been adjusted, now need to switch to adjusted index
    if (appState.fileCount > 0 && playingIndexAfterDelete >= 0 && playingIndexAfterDelete < appState.fileCount) {
      appState.currentSelectedIndex = playingIndexAfterDelete;  // Make selected index follow playing index
      appState.currentPlayingIndex = playingIndexAfterDelete;  // Update global variable
      if (callbacks.resetClock) callbacks.resetClock();
      appState.nextS = 1;
      // If playing before delete, continue playing new song after delete
      if (wasPlaying) {
        appState.isPlaying = true;
        appState.stopped = false;
      }
      LOG_PRINTF("Switched to new current file: %s (index %d)\n", appState.audioFiles[appState.currentSelectedIndex].c_str(), appState.currentSelectedIndex);
      if (callbacks.onFileDeleted) {
        callbacks.onFileDeleted(deleteIndex, playingIndexAfterDelete);
      }
    } else {
      // No more files, stop playback
      appState.isPlaying = false;
      appState.stopped = true;
      appState.currentPlayingIndex = 0;
      LOG_PRINTLN("No more files available");
    }
  } else {
    // Deleted song was not playing, continue playing current song, don't switch
    // Only need to update appState.currentPlayingIndex to adjusted index
    appState.currentPlayingIndex = playingIndexAfterDelete;
    LOG_PRINTF("Deleted file (index %d) was not playing (was index %d, now %d), continuing with: %s\n", 
                  deleteIndex, playingIndexBeforeDelete, playingIndexAfterDelete, 
                  appState.fileCount > 0 && playingIndexAfterDelete < appState.fileCount ? appState.audioFiles[playingIndexAfterDelete].c_str() : "none");
  }
}

void captureScreenshot(fs::FS& fs, M5Canvas& sprite, ESP32Time& rtc) {
  // Create /screen directory if it doesn't exist
  if (!fs.exists(SCREEN_DIR)) {
    fs.mkdir(SCREEN_DIR);
    LOG_PRINTLN("Created /screen directory");
  }
  
  // Generate filename with timestamp (optimized: use char buffer instead of String concatenation)
  char filename[64];
  String timestamp = rtc.getTime("%Y%m%d_%H%M%S");
  snprintf(filename, sizeof(filename), "%s/screenshot_%s.bmp", SCREEN_DIR, timestamp.c_str());
  
  // Open file for writing
  File file = fs.open(filename, FILE_WRITE);
  if (!file) {
    LOG_PRINTF("Failed to create screenshot file: %s\n", filename);
    return;
  }
  
  // BMP header (24-bit, no compression)
  uint8_t bmpHeader[54] = {
    0x42, 0x4D,  // 'BM'
    0x00, 0x00, 0x00, 0x00,  // File size (will be filled later)
    0x00, 0x00, 0x00, 0x00,  // Reserved
    0x36, 0x00, 0x00, 0x00,  // Offset to pixel data (54 bytes)
    0x28, 0x00, 0x00, 0x00,  // DIB header size (40 bytes)
    0x00, 0x00, 0x00, 0x00,  // Width (will be filled)
    0x00, 0x00, 0x00, 0x00,  // Height (will be filled, positive = bottom-up)
    0x01, 0x00,  // Planes (1)
    0x18, 0x00,  // Bits per pixel (24)
    0x00, 0x00, 0x00, 0x00,  // Compression (none)
    0x00, 0x00, 0x00, 0x00,  // Image size (0 for uncompressed)
    0x00, 0x00, 0x00, 0x00,  // X pixels per meter
    0x00, 0x00, 0x00, 0x00,  // Y pixels per meter
    0x00, 0x00, 0x00, 0x00,  // Colors used
    0x00, 0x00, 0x00, 0x00   // Important colors
  };
  
  // Fill width and height (little-endian)
  bmpHeader[18] = SCREEN_WIDTH & 0xFF;
  bmpHeader[19] = (SCREEN_WIDTH >> 8) & 0xFF;
  bmpHeader[20] = (SCREEN_WIDTH >> 16) & 0xFF;
  bmpHeader[21] = (SCREEN_WIDTH >> 24) & 0xFF;
  bmpHeader[22] = SCREEN_HEIGHT & 0xFF;
  bmpHeader[23] = (SCREEN_HEIGHT >> 8) & 0xFF;
  bmpHeader[24] = (SCREEN_HEIGHT >> 16) & 0xFF;
  bmpHeader[25] = (SCREEN_HEIGHT >> 24) & 0xFF;
  
  // Calculate row size (must be multiple of 4)
  int rowSize = ((SCREEN_WIDTH * 3 + 3) / 4) * 4;
  int imageSize = rowSize * SCREEN_HEIGHT;
  int fileSize = 54 + imageSize;
  
  // Fill file size (little-endian)
  bmpHeader[2] = fileSize & 0xFF;
  bmpHeader[3] = (fileSize >> 8) & 0xFF;
  bmpHeader[4] = (fileSize >> 16) & 0xFF;
  bmpHeader[5] = (fileSize >> 24) & 0xFF;
  
  // Write header
  file.write(bmpHeader, 54);
  
  // Read pixels from sprite and write to file (BMP is bottom-up, so start from bottom row)
  uint8_t rowBuffer[rowSize];
  for (int y = SCREEN_HEIGHT - 1; y >= 0; y--) {
    int bufIdx = 0;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      // Read pixel from sprite (RGB565)
      uint16_t pixel = sprite.readPixel(x, y);
      // Convert RGB565 to BGR888 (BMP uses BGR)
      // First byte-swap to get standard RGB565
      pixel = ((pixel & 0xFF) << 8) | ((pixel >> 8) & 0xFF);
      // Extract R, G, B (5-6-5 bits)
      uint8_t r5 = (pixel >> 11) & 0x1F;
      uint8_t g6 = (pixel >> 5) & 0x3F;
      uint8_t b5 = pixel & 0x1F;
      // Expand to 8 bits
      uint8_t r = (r5 << 3) | (r5 >> 2);
      uint8_t g = (g6 << 2) | (g6 >> 4);
      uint8_t b = (b5 << 3) | (b5 >> 2);
      // Write as BGR (BMP format)
      rowBuffer[bufIdx++] = b;
      rowBuffer[bufIdx++] = g;
      rowBuffer[bufIdx++] = r;
    }
    // Pad row to multiple of 4 bytes
    while (bufIdx < rowSize) {
      rowBuffer[bufIdx++] = 0;
    }
    file.write(rowBuffer, rowSize);
  }
  
  file.close();
  LOG_PRINTF("Screenshot saved: %s\n", filename);
}

}  // namespace FileManager

