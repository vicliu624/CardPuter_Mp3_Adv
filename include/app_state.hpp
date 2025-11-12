#pragma once

#include <Arduino.h>
#include "config.hpp"

// Centralized application state
// Step 3: Aggregate scattered global variables into a single structure

struct AppState {
  // Playback state
  int currentSelectedIndex = 0;      // n
  int currentPlayingIndex = 0;
  int volume = 10;                    // 0..21
  int brightnessIndex = 2;             // bri, 0..4
  bool isPlaying = true;
  bool stopped = false;               // stoped (keeping original spelling for compatibility)
  PlaybackMode playMode = PlaybackMode::Sequential;
  
  // UI state
  bool screenOff = false;
  int savedBrightness = 2;
  bool showDeleteDialog = false;
  bool showID3Page = false;
  
  // Battery and time
  int batteryPercent = 0;
  unsigned long lastBatteryUpdate = 0;
  String cachedTimeStr = "";
  unsigned long lastTimeUpdate = 0;
  
  // Spectrum graph
  unsigned long lastGraphUpdate = 0;
  int graphSpeed = 0;
  int graphBars[14] = {0};
  
  // List scrolling
  int lastSelectedIndex = -1;
  unsigned long selectedTime = 0;
  int selectedScrollPos = 8;
  
  // Audio info cache
  String cachedAudioInfo = "";
  unsigned long lastAudioInfoUpdate = 0;
  
  // ID3 metadata
  String id3Title = "";
  String id3Artist = "";
  String id3Album = "";
  String id3Year = "";
  String id3ContentType = "";
  
  // ID3 cover (streaming)
  size_t id3CoverPos = 0;
  size_t id3CoverLen = 0;
  uint8_t* id3CoverBuf = nullptr;
  size_t id3CoverSize = 0;
  
  // ID3 album text scrolling
  int id3AlbumScrollPos = 0;
  unsigned long id3AlbumSelectTime = 0;
  
  // Track switching
  int nextS = 0;  // Request to switch tracks
  bool volUp = false;
  
  // File list
  String audioFiles[MAX_FILES];
  int fileCount = 0;
  
  // Helper methods
  int getBrightness() const {
    return BRIGHTNESS_VALUES[brightnessIndex];
  }
  
  void resetID3Metadata() {
    id3Title = "";
    id3Artist = "";
    id3Album = "";
    id3Year = "";
    id3ContentType = "";
    id3CoverPos = 0;
    id3CoverLen = 0;
    if (id3CoverBuf) {
      heap_caps_free(id3CoverBuf);
      id3CoverBuf = nullptr;
      id3CoverSize = 0;
    }
  }
};

