#pragma once

#include <Arduino.h>
#include <FS.h>
#include "Audio.h"
#include "app_state.hpp"

// AudioManager: Centralized audio playback control and callback handling
// Step 7: Extract audio control logic from M5mp3.cpp

namespace AudioManager {

// Set the Audio instance to manage (call before other functions)
void setAudioInstance(class Audio* audio);

// Get the managed Audio instance
class Audio* getAudioInstance();

// Initialize audio system (call once in setup)
// Returns true if initialization successful
bool initialize(AppState& appState);

// Connect to audio file on SD card
void connectToFile(fs::FS& fs, const char* path);

// Stop current playback
void stop();

// Main audio loop (call in Task_Audio)
void loop(AppState& appState, bool codecInitialized);

// Set volume (0-21)
void setVolume(int volume);

// Set balance (-16 to +16)
void setBalance(int balance);

// Set I2S pinout (BCLK, LRCK, DOUT)
void setPinout(int bclkPin, int lrckPin, int doutPin);

// Get current sample rate
uint32_t getSampleRate();

// Get current bits per sample
uint8_t getBitsPerSample();

// Get current playback time in seconds
uint32_t getCurrentTime();

// Get total file duration in seconds
uint32_t getFileDuration();

// ID3 metadata callback (called by ESP32-audioI2S library)
void onID3Data(const char* info, AppState& appState);

// ID3 image callback (called by ESP32-audioI2S library)
void onID3Image(File& file, const size_t pos, const size_t size, AppState& appState);

// EOF callback (called by ESP32-audioI2S library)
void onEOF(const char* info, AppState& appState, fs::FS& fs);

}  // namespace AudioManager

