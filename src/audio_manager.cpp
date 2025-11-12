#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include "../include/audio_manager.hpp"
#include "../include/config.hpp"
#include "M5Cardputer.h"
#include <ESP32Time.h>

// Forward declaration for resetClock
extern void resetClock();
extern ESP32Time rtc;

// Global Audio instance (managed by AudioManager)
static Audio* g_audio = nullptr;

namespace AudioManager {

bool initialize(AppState& appState) {
  // Audio object will be created externally and passed via setAudioInstance
  // For now, we'll use a static instance
  static Audio audioInstance;
  g_audio = &audioInstance;
  return true;
}

void setAudioInstance(Audio* audio) {
  g_audio = audio;
}

Audio* getAudioInstance() {
  return g_audio;
}

void connectToFile(fs::FS& fs, const char* path) {
  if (!g_audio) return;
  g_audio->connecttoFS(fs, path);
}

void stop() {
  if (!g_audio) return;
  g_audio->stopSong();
}

void loop(AppState& appState, bool codecInitialized) {
  if (!g_audio) return;
  if (appState.isPlaying && !appState.stopped) {
    g_audio->loop();
  }
}

void setVolume(int volume) {
  if (!g_audio) return;
  g_audio->setVolume(volume);
}

void setBalance(int balance) {
  if (!g_audio) return;
  g_audio->setBalance(balance);
}

void setPinout(int bclkPin, int lrckPin, int doutPin) {
  if (!g_audio) return;
  g_audio->setPinout(bclkPin, lrckPin, doutPin);
}

uint32_t getSampleRate() {
  if (!g_audio) return 0;
  return g_audio->getSampleRate();
}

uint8_t getBitsPerSample() {
  if (!g_audio) return 0;
  return g_audio->getBitsPerSample();
}

uint32_t getCurrentTime() {
  if (!g_audio) return 0;
  return g_audio->getAudioCurrentTime();
}

uint32_t getFileDuration() {
  if (!g_audio) return 0;
  return g_audio->getAudioFileDuration();
}

void onID3Data(const char* info, AppState& appState) {
  if (!info) return;
  String s(info);
  LOG_PRINTF("ID3DATA: %s\n", s.c_str());

  // Strip UTF-16 BOM if present
  if (s.length() >= 2 && (uint8_t)s[0] == 0xFF && (uint8_t)s[1] == 0xFE) {
    s = s.substring(2);
  } else if (s.length() >= 2 && (uint8_t)s[0] == 0xFE && (uint8_t)s[1] == 0xFF) {
    s = s.substring(2);
  }

  auto assignKV = [&](const char* key, String& out) {
    int n = strlen(key);
    if (s.startsWith(key)) {
      int pos = n;
      if (pos < s.length() && (s[pos] == ':' || s[pos] == '=')) pos++;
      String v = s.substring(pos);
      v.trim();
      if (v.length() > 0) { out = v; return true; }
    }
    return false;
  };

  auto assignFrame = [&](const char* frame, String& out) {
    int n = strlen(frame);
    if (s.startsWith(frame)) {
      String v = s.substring(n);
      if (v.length() > 0 && (v[0] == ':' || v[0] == '=')) v = v.substring(1);
      v.trim();
      if (v.length() > 0) { out = v; return true; }
    }
    return false;
  };

  bool matched = false;
  matched |= assignKV("Title",  appState.id3Title);
  matched |= assignKV("Artist", appState.id3Artist);
  matched |= assignKV("Album",  appState.id3Album);
  matched |= assignKV("Year",   appState.id3Year);
  matched |= assignKV("ContentType", appState.id3ContentType);

  matched |= assignFrame("TIT2", appState.id3Title);
  matched |= assignFrame("TALB", appState.id3Album);
  matched |= assignFrame("TPE1", appState.id3Artist);
  matched |= assignFrame("TYER", appState.id3Year);
  matched |= assignFrame("TDRC", appState.id3Year);
  matched |= assignFrame("TCON", appState.id3ContentType);

  // Ignore non-metadata lines (e.g., "SettingsForEncoding: Lavf58.76.100")
  if (!matched && (s.indexOf(":") >= 0 || s.indexOf("=") >= 0)) {
    // Could be metadata but not recognized, ignore
  }
}

void onID3Image(File& file, const size_t pos, const size_t size, AppState& appState) {
  // Streaming-only: never allocate full image into RAM
  appState.id3CoverPos = pos;
  appState.id3CoverLen = size;
  if (appState.id3CoverBuf) { 
    heap_caps_free(appState.id3CoverBuf); 
    appState.id3CoverBuf = nullptr; 
    appState.id3CoverSize = 0; 
  }
  LOG_PRINTF("ID3 image will stream: size=%u pos=%u\n", (unsigned)size, (unsigned)pos);
}

void onEOF(const char* info, AppState& appState, fs::FS& fs) {
  resetClock();
  LOG_PRINT("eof_mp3     ");
  LOG_PRINTLN(info);
  
  // Determine next song based on playback mode
  if (appState.playMode == PlaybackMode::Sequential) {
    // Sequential playback: next song
    appState.currentPlayingIndex++;
    if (appState.currentPlayingIndex >= appState.fileCount) appState.currentPlayingIndex = 0;
  } else if (appState.playMode == PlaybackMode::Random) {
    // Random playback: random selection
    appState.currentPlayingIndex = random(0, appState.fileCount);
  } else if (appState.playMode == PlaybackMode::SingleRepeat) {
    // Single repeat: don't change index, continue playing current song
    // appState.currentPlayingIndex remains unchanged
  }
  
  appState.currentSelectedIndex = appState.currentPlayingIndex;  // Sync selected index to playing index
  LOG_PRINTF("eof: opening next file: %s (index %d, mode %d)\n", 
                appState.audioFiles[appState.currentPlayingIndex].c_str(), 
                appState.currentPlayingIndex, 
                static_cast<int>(appState.playMode));
  if (fs.exists(appState.audioFiles[appState.currentPlayingIndex])) {
    connectToFile(fs, appState.audioFiles[appState.currentPlayingIndex].c_str());
    // Reset audio info cache when auto-switching songs (will be updated after decoder initializes)
    appState.cachedAudioInfo = "";
    appState.lastAudioInfoUpdate = millis();  // Reset timer to allow decoder initialization time
    // Reset ID3 metadata
    appState.resetID3Metadata();
  } else {
    LOG_PRINTF("eof: next file not found: %s\n", appState.audioFiles[appState.currentPlayingIndex].c_str());
  }
}

}  // namespace AudioManager

