#pragma once

#include <Arduino.h>
#include <FS.h>
#include "app_state.hpp"
#include "config.hpp"
#include "M5Cardputer.h"  // For M5Canvas type alias
#include <ESP32Time.h>    // For ESP32Time class

namespace FileManager {

// Callbacks for file operations that need external actions
struct Callbacks {
  void (*resetClock)() = nullptr;
  void (*onFileDeleted)(int deletedIndex, int newPlayingIndex) = nullptr;
};

// List audio files from directory and populate appState.audioFiles
// Supports recursive directory scanning
void listFiles(fs::FS& fs, const char* dirname, uint8_t levels, AppState& appState);

// Delete currently selected file from SD card and update appState
// Handles index adjustments, playback state, and triggers callbacks
void deleteCurrentFile(fs::FS& fs, AppState& appState, const Callbacks& callbacks);

// Capture current screen content and save as BMP to SD card
// Creates /screen directory if it doesn't exist
void captureScreenshot(fs::FS& fs, M5Canvas& sprite, ESP32Time& rtc);

}  // namespace FileManager

