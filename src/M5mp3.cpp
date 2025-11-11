#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <Wire.h>
#include <SD.h>
#include <cstdio>  // For snprintf
#include "M5Cardputer.h"
#include "Audio.h"  // https://github.com/schreibfaul1/ESP32-audioI2S   version 2.0.0
#include "font.h"
#include <ESP32Time.h>  // https://github.com/fbiego/ESP32Time  verison 2.0.6
#include "driver/i2s.h"
#include <math.h>
#include <memory>
#include <utility/Keyboard/KeyboardReader/IOMatrix.h>
#include <utility/Keyboard/KeyboardReader/TCA8418.h>
#include "../include/config.hpp"  // Configuration constants
#include "../include/app_state.hpp"  // Application state
#include "../include/input_handler.hpp"  // Keyboard input handling
#include "../include/image_utils.hpp"    // Image scan/size helpers
#include "../include/ui_renderer.hpp"   // UI rendering
#include "../include/board_init.hpp"    // Board / codec init (scaffold)
#include "../include/audio_manager.hpp"  // Audio playback control
#include "../include/file_manager.hpp"   // File operations (list, delete, screenshot)
M5Canvas sprite(&M5Cardputer.Display);
// Removed unused canvas: spr
// Step 3: Centralized application state
AppState appState;
// microSD card
#define SD_SCK 40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS 12
// Cardputer audio pin mappings provided by config.hpp

// Hardware initialization functions have been migrated to BoardInit module

#define BAT_ADC_PIN 10  // ADC pin used for battery reading - verify against schematic!

// Hardware pin variables (initialized by BoardInit)
static int audioBclkPin = CARDPUTER_ADV_I2S_BCLK;
static int audioLrckPin = CARDPUTER_ADV_I2S_LRCK;
static int audioDoutPin = CARDPUTER_ADV_I2S_DOUT;
static int hpDetectPin = CARDPUTER_ADV_HP_DET_PIN;
static int ampEnablePin = CARDPUTER_ADV_AMP_EN_PIN;
// Removed unused macro: AUDIO_FILENAME_01

// Forward declarations (helpers implemented below)
Audio audio;
unsigned short grays[GRAYS_COUNT];  // Color palette (kept as global for now)
// Step 3: State variables moved to AppState
// Temporary variables (not in AppState)
unsigned short gray;
int sliderPos = 0;
unsigned short light;
// Removed unused variable: textPos (was set but never read)
static bool lastHPState = false;
// Task handle for audio task
TaskHandle_t handleAudioTask = NULL;
ESP32Time rtc(0);
// Global flag to indicate codec init state so tasks can check it
bool codec_initialized = false;
// Forward declarations
void Task_TFT(void *pvParameters);
void Task_Audio(void *pvParameters);
const lgfx::U8g2font* detectAndGetFont(const String& text);  // Detect language and return appropriate font
// File operations (listFiles, deleteCurrentFile, captureScreenshot) are now in FileManager module
// Forward declarations for draw functions (now implemented via UiRenderer)
void drawId3Page();  // Render ID3 information page (delegates to UiRenderer)
void resetClock() {
  rtc.setTime(0, 0, 0, 17, 1, 2021);
}

// Wrapper functions for FileManager operations (required for function pointer compatibility)
static void captureScreenshotWrapper() {
  FileManager::captureScreenshot(SD, sprite, rtc);
}

static void deleteCurrentFileWrapper() {
  static FileManager::Callbacks fileCallbacks;
  fileCallbacks.resetClock = &resetClock;
  fileCallbacks.onFileDeleted = [](int deletedIndex, int newPlayingIndex) {
    (void)deletedIndex;
    (void)newPlayingIndex;
  };
  FileManager::deleteCurrentFile(SD, appState, fileCallbacks);
}

// Detect language from text and return appropriate font
// Returns efontKR_12 for Korean, efontJA_12 for Japanese, efontCN_12 for Chinese, or nullptr for default
const lgfx::U8g2font* detectAndGetFont(const String& text) {
  if (text.length() == 0) return nullptr;
  
  const uint8_t* utf8 = (const uint8_t*)text.c_str();
  bool hasKorean = false;
  bool hasJapanese = false;
  bool hasChinese = false;
  
  while (*utf8) {
    uint32_t codePoint = 0;
    
    // Decode UTF-8 character
    if ((*utf8 & 0x80) == 0) {
      // ASCII character (0x00-0x7F)
      codePoint = *utf8;
      utf8++;
    } else if ((*utf8 & 0xE0) == 0xC0) {
      // 2-byte UTF-8 (0x80-0x7FF)
      codePoint = ((*utf8 & 0x1F) << 6) | (*(utf8 + 1) & 0x3F);
      utf8 += 2;
    } else if ((*utf8 & 0xF0) == 0xE0) {
      // 3-byte UTF-8 (0x800-0xFFFF)
      codePoint = ((*utf8 & 0x0F) << 12) | ((*(utf8 + 1) & 0x3F) << 6) | (*(utf8 + 2) & 0x3F);
      utf8 += 3;
    } else if ((*utf8 & 0xF8) == 0xF0) {
      // 4-byte UTF-8 (0x10000-0x10FFFF)
      codePoint = ((*utf8 & 0x07) << 18) | ((*(utf8 + 1) & 0x3F) << 12) | ((*(utf8 + 2) & 0x3F) << 6) | (*(utf8 + 3) & 0x3F);
      utf8 += 4;
    } else {
      // Invalid UTF-8, skip byte
      utf8++;
      continue;
    }
    
    // Check Unicode ranges
    if (codePoint >= 0xAC00 && codePoint <= 0xD7AF) {
      // Korean Hangul Syllables
      hasKorean = true;
    } else if ((codePoint >= 0x3040 && codePoint <= 0x309F) ||  // Hiragana
               (codePoint >= 0x30A0 && codePoint <= 0x30FF)) {  // Katakana
      hasJapanese = true;
    } else if (codePoint >= 0x4E00 && codePoint <= 0x9FFF) {
      // CJK Unified Ideographs (could be Chinese or Japanese Kanji)
      // If we've already seen Hiragana/Katakana, it's likely Japanese
      if (hasJapanese) {
        hasJapanese = true;
      } else {
        hasChinese = true;
      }
    }
    
    // Early exit if we found Korean (highest priority)
    if (hasKorean) break;
  }
  
  // Priority: Korean > Japanese > Chinese > Default
  if (hasKorean) {
    return &fonts::efontKR_12;
  } else if (hasJapanese) {
    return &fonts::efontJA_12;
  } else if (hasChinese) {
    return &fonts::efontCN_12;
  }
  
  return nullptr;  // Use default font for English/other languages
}

// Battery helper for M5Cardputer Advanced
// M5Cardputer Advanced uses AXP2101 PMIC which provides accurate battery level
// The library's getBatteryLevel() uses the PMIC's internal gauge for accurate readings
// Battery capacity (1750mAh) is handled by the PMIC, not by voltage-to-percentage mapping
static int getBatteryPercent() {
  // Try to use M5Cardputer's built-in Power API first (recommended for Advanced version)
  // This uses AXP2101 PMIC's internal battery gauge for accurate readings
  int level = M5Cardputer.Power.getBatteryLevel();
  
  // If Power API returns valid value (0-100), use it
  // Returns -1 or -2 if not supported or error
  if (level >= 0 && level <= 100) {
    return level;
  }
  
  // Fallback: Direct ADC reading (for Standard version or if PMIC not available)
  // This method uses voltage-to-percentage mapping which is less accurate
  // Voltage range: 3.3V (0%) to 4.2V (100%) is typical for Li-Po batteries
  // Note: Battery capacity (mAh) doesn't directly affect voltage-to-percentage calculation
  // The voltage range is determined by battery chemistry, not capacity
  int raw = analogRead(BAT_ADC_PIN);
  float voltage = (raw / 4095.0f) * 3.3f * 2.0f; // 2:1 voltage divider assumption
  int mv = (int)(voltage * 1000.0f);
  
  // Voltage-to-percentage mapping for Li-Po batteries
  // 3.3V = 0%, 4.2V = 100% (typical range)
  // Note: This is a linear approximation; actual battery discharge curve is non-linear
  int percent = constrain(map(mv, 3300, 4200, 0, 100), 0, 100);
  return percent;
}

// Hardware initialization functions have been migrated to BoardInit module
void setup() {
  LOG_INIT(115200);
  LOG_PRINTLN("hi");
  resetClock();
  // Initialize M5Cardputer and SD card
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_mic = false;
  cfg.internal_spk = false; // leave external I2S free for ES8311 codec
  M5Cardputer.begin(cfg, true);
  auto spk_cfg = M5Cardputer.Speaker.config();
  spk_cfg.sample_rate = 128000;
  spk_cfg.task_pinned_core = APP_CPU_NUM;
  M5Cardputer.Speaker.config(spk_cfg);
  // Do NOT initialize M5Cardputer.Speaker when using external ES8311 via I2S.
  // It can take over the I2S peripheral and conflict with ESP32-audioI2S.
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(BRIGHTNESS_VALUES[appState.brightnessIndex]);
  // Enable UTF-8 support for Chinese character display
  M5Cardputer.Display.setAttribute(utf8_switch, true);
  sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS)) {
    LOG_PRINTLN(F("ERROR: SD Mount Failed!"));
  }
  // Read song files from /music directory
  FileManager::listFiles(SD, MUSIC_DIR, MAX_FILES, appState);
  if (appState.fileCount == 0) {
    LOG_PRINTLN("No files found in /music, scanning root as fallback");
    FileManager::listFiles(SD, "/", MAX_FILES, appState);
  }
  // Initialize AudioManager with the global Audio instance (must be before BoardInit)
  AudioManager::setAudioInstance(&audio);
  AudioManager::initialize(appState);
  
  // Detect board variant and initialize audio hardware
  BoardInit::Variant detected = BoardInit::detectVariant();
  if (!BoardInit::initAudioForDetectedVariant(detected, audioBclkPin, audioLrckPin, audioDoutPin,
                                               hpDetectPin, ampEnablePin, codec_initialized, appState.volume)) {
    LOG_PRINTLN("[ERROR] Audio initialisation failed - leaving amplifier disabled");
  }
  
  // Initialize lastHPState if headphone detect pin is available
  if (hpDetectPin >= 0) {
    lastHPState = (digitalRead(hpDetectPin) == LOW);
  }
  
  // Configure keyboard driver
  BoardInit::configureKeyboard(detected);
  if (appState.fileCount > 0) {
    LOG_PRINTF("Trying to play: %s\n", appState.audioFiles[appState.currentSelectedIndex].c_str());
    // Double-check the file exists
    if (SD.exists(appState.audioFiles[appState.currentSelectedIndex])) {
      // Open the file directly and print size + header bytes for verification
      File f = SD.open(appState.audioFiles[appState.currentSelectedIndex]);
      if (f) {
        uint32_t sz = f.size();
        LOG_PRINTF("SD open OK, size=%u bytes\n", (unsigned)sz);
        // read first 12 bytes
        uint8_t hdr[12];
        int r = f.read(hdr, sizeof(hdr));
        LOG_PRINTF("First %d bytes: ", r);
        for (int i = 0; i < r; i++) LOG_PRINTF("%02X ", hdr[i]);
        LOG_PRINTLN();
        // Optional: detect ID3 and log a warning if a large tag may delay playback
        if (r >= 3 && hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3') {
          LOG_PRINTLN("Note: ID3 tag detected; initial parsing may delay audio start");
        }
        f.close();
      } else {
        LOG_PRINTLN("SD.open failed (could not read file)");
      }
      // Always connect; decoding + ID3 parsing should not depend on codec init state
      AudioManager::connectToFile(SD, appState.audioFiles[appState.currentSelectedIndex].c_str());
      appState.currentPlayingIndex = appState.currentSelectedIndex;  // Sync playing index on initialization
      appState.isPlaying = true;
      appState.stopped = false;
    } else {
      LOG_PRINTF("File not found on SD: %s\n", appState.audioFiles[appState.currentSelectedIndex].c_str());
    }
  } else {
    LOG_PRINTLN("No audio files found on SD - skipping connect");
  }
  int co = GRAYS_START_COLOR;
  for (int i = 0; i < GRAYS_COUNT; i++) {
    grays[i] = M5Cardputer.Display.color565(co, co, co + GRAYS_BLUE_OFFSET);
    co = co - GRAYS_STEP;
  }
  // Initialize battery display and time cache
  appState.batteryPercent = getBatteryPercent();
  appState.lastBatteryUpdate = millis();
  appState.cachedTimeStr = rtc.getTime().substring(3, 8);
  appState.lastTimeUpdate = millis();
  appState.lastGraphUpdate = millis();
  
  // Create tasks and pin them to different cores
  xTaskCreatePinnedToCore(Task_TFT, "Task_TFT", 20480, NULL, 2, NULL, 0);                  // Core 0
  xTaskCreatePinnedToCore(Task_Audio, "Task_Audio", 10240, NULL, 3, &handleAudioTask, 1);  // Core 1
}
void loop() {
  // Poll headphone detect and gate AMP_EN accordingly
  if (hpDetectPin >= 0) {
    bool hpInserted = (digitalRead(hpDetectPin) == LOW);
    if (hpInserted != lastHPState) {
      lastHPState = hpInserted;
      if (ampEnablePin >= 0) {
        if (hpInserted) {
          LOG_PRINTLN("HP inserted -> speaker AMP OFF");
          digitalWrite(ampEnablePin, LOW);
        } else {
          LOG_PRINTLN("HP removed -> speaker AMP ON");
          digitalWrite(ampEnablePin, HIGH);
        }
      }
    }
  }
  delay(200);  // Increase polling interval to reduce CPU usage
}

// Step 2: Split draw() into smaller functions
// Render ID3 information page
void drawId3Page() {
  // Forward to UiRenderer
  UiRenderer::drawId3Page(sprite, appState, grays, detectAndGetFont);
}

// (removed original implementation after extraction)

void draw() {
  if (appState.showID3Page) {
    drawId3Page();
    return;
  }
  
  // Delegate main view rendering to UiRenderer
  UiRenderer::drawMainView(sprite, appState, grays, gray, light, sliderPos, rtc, getBatteryPercent, detectAndGetFont);
}

void Task_TFT(void *pvParameters) {
  while (1) {
    M5Cardputer.update();
    // Check for key press events
    if (M5Cardputer.Keyboard.isChange()) {
      // Centralized handlers
      (void)InputHandler::processBasicToggles(appState);
      (void)InputHandler::processPlaybackAndList(appState);
      InputHandler::Actions acts;
      acts.captureScreenshot = &captureScreenshotWrapper;
      acts.deleteCurrentFile = &deleteCurrentFileWrapper;
      (void)InputHandler::processDeleteAndScreenshot(appState, acts);

      if (M5Cardputer.Keyboard.isKeyPressed('a')) {
        appState.isPlaying = !appState.isPlaying;
        appState.stopped = !appState.stopped;
      }  // Toggle the playback state
      if (M5Cardputer.Keyboard.isKeyPressed('v')) {
        appState.isPlaying = false;
        appState.volUp = true;
        appState.volume = appState.volume + 5;
        if (appState.volume > 20) appState.volume = 5;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('-')) {
        // '-' key: Decrease appState.volume
        appState.volUp = true;
        appState.volume = appState.volume - 1;
        if (appState.volume < 0) appState.volume = 0;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('=')) {
        // '=' key: Increase appState.volume
        appState.volUp = true;
        appState.volume = appState.volume + 1;
        if (appState.volume > 21) appState.volume = 21;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('l')) {
        appState.brightnessIndex++;
        if (appState.brightnessIndex == 5) appState.brightnessIndex = 0;
        M5Cardputer.Display.setBrightness(BRIGHTNESS_VALUES[appState.brightnessIndex]);
      }
      // All other keys handled by InputHandler
    }
    // If screen is off, skip drawing to save CPU
    if (!appState.screenOff) {
      draw();
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);  // Balance refresh rate: 50ms (20fps), smooth and power-efficient
  }
}

void Task_Audio(void *pvParameters) {
  static unsigned long lastLog = 0;
  const TickType_t playDelay = pdMS_TO_TICKS(1);
  const TickType_t idleDelay = pdMS_TO_TICKS(20);

  while (1) {
    if (appState.volUp) {
      AudioManager::setVolume(appState.volume);
      appState.isPlaying = true;
      appState.volUp = false;
    }

    if (appState.nextS) {
      AudioManager::stop();
      LOG_PRINTF("Task_Audio: next track requested: %s\n", appState.audioFiles[appState.currentSelectedIndex].c_str());
      if (SD.exists(appState.audioFiles[appState.currentSelectedIndex])) {
        // Reset ID3 metadata before opening the next file to avoid stale display
        appState.resetID3Metadata();
        AudioManager::connectToFile(SD, appState.audioFiles[appState.currentSelectedIndex].c_str());
        appState.currentPlayingIndex = appState.currentSelectedIndex;  // Update actual playing index
        // Reset audio info cache when switching songs (will be updated after decoder initializes)
        appState.cachedAudioInfo = "";
        appState.lastAudioInfoUpdate = millis();  // Reset timer to allow decoder initialization time
      } else {
        LOG_PRINTF("Task_Audio: file not found: %s\n", appState.audioFiles[appState.currentSelectedIndex].c_str());
      }
      appState.isPlaying = true;
      appState.stopped = false;  // Ensure playback is not stopped after switching tracks
      appState.nextS = 0;
    }

    // Do not gate decoding/ID3 parsing on codec_initialized; allow loop() to run
    if (appState.isPlaying && !appState.stopped) {
      AudioManager::loop(appState, codec_initialized);

      if (millis() - lastLog >= 500) {
        int ampState = -1;
        if (ampEnablePin >= 0) {
          ampState = digitalRead(ampEnablePin);
        }
        //Serial.printf("Task_Audio: audio.loop() heartbeat  codec_initialized=%d AMP_EN=%d ES_ADDR=0x%02X\n", codec_initialized ? 1 : 0, ampState, ES8311_ADDR);
        lastLog = millis();
      }

      vTaskDelay(playDelay);
    } else {
      vTaskDelay(idleDelay);
    }
  }
}

// Function to play a song from a given URL or file path
// Removed unused functions: playSong, stopSong, openSong
// Audio control is now handled through AudioManager interface
// File operations have been migrated to FileManager module

void audio_eof_mp3(const char *info) {
  AudioManager::onEOF(info, appState, SD);
}

void audio_id3data(const char* info) {
  AudioManager::onID3Data(info, appState);
}

void audio_id3image(File& file, const size_t pos, const size_t size) {
  AudioManager::onID3Image(file, pos, size, appState);
}

