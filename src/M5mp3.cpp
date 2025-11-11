#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <Wire.h>
#include <SD.h>
#include "M5Cardputer.h"
#include "Audio.h"  // https://github.com/schreibfaul1/ESP32-audioI2S   version 2.0.0
#include "font.h"
#include <ESP32Time.h>  // https://github.com/fbiego/ESP32Time  verison 2.0.6
#include "driver/i2s.h"
#include <math.h>
#include <memory>
#include <utility/Keyboard/KeyboardReader/IOMatrix.h>
#include <utility/Keyboard/KeyboardReader/TCA8418.h>
M5Canvas sprite(&M5Cardputer.Display);
M5Canvas spr(&M5Cardputer.Display);
// microSD card
#define SD_SCK 40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS 12
// Cardputer audio pin mappings (Stamp-S3A):
// G8  = SDA (I2C)
// G9  = SCL (I2C)
// G41 = SCLK / BCLK
// G43 = LRCK
// G42 = ASDOUT (DOUT)
// G46 = AMP_EN (Cardputer-Adv)
constexpr int CARDPUTER_I2C_SDA = 8;   // G8
constexpr int CARDPUTER_I2C_SCL = 9;   // G9
constexpr int CARDPUTER_ADV_I2S_BCLK = 41;
constexpr int CARDPUTER_ADV_I2S_LRCK = 43;
constexpr int CARDPUTER_ADV_I2S_DOUT = 42;
constexpr int CARDPUTER_ADV_HP_DET_PIN = 17;   // LOW when headphones inserted
constexpr int CARDPUTER_ADV_AMP_EN_PIN = 46;   // HIGH enables amplifier

// Base Cardputer (v1.1) reuses the Stamp-S3 I2S pins but has no headphone detect / AMP gate exposed.
// If future revisions diverge, update these defaults accordingly.
constexpr int CARDPUTER_STD_I2S_BCLK = 41;
constexpr int CARDPUTER_STD_I2S_LRCK = 43;
constexpr int CARDPUTER_STD_I2S_DOUT = 42;

// Simple I2C scan utility (implemented below)
static void i2c_scan();

#define BAT_ADC_PIN 10  // ADC pin used for battery reading - verify against schematic!
#define ES8311_I2C_FREQ 400000UL
// ES8311 I2C address on Cardputer-Adv
constexpr uint8_t ES8311_ADDR = 0x18;
constexpr uint8_t AW88298_ADDR = 0x36;   // Used on legacy Cardputer (no headphones)
constexpr uint8_t AW9523_ADDR = 0x58;
constexpr uint8_t TCA8418_ADDR = 0x34;   // Keyboard controller on Cardputer-Adv

enum class CardputerVariant { Unknown, Standard, Advanced };

static CardputerVariant g_cardputerVariant = CardputerVariant::Unknown;
static int audioBclkPin = CARDPUTER_ADV_I2S_BCLK;
static int audioLrckPin = CARDPUTER_ADV_I2S_LRCK;
static int audioDoutPin = CARDPUTER_ADV_I2S_DOUT;
static int hpDetectPin = CARDPUTER_ADV_HP_DET_PIN;
static int ampEnablePin = CARDPUTER_ADV_AMP_EN_PIN;
#define AUDIO_FILENAME_01 "/song.mp3"

// Forward declarations (helpers implemented below)
static bool es8311_write(uint8_t reg, uint8_t val);
static CardputerVariant detectCardputerVariant();
static bool initCardputerStdAudio();
static bool initCardputerAdvCodec();
static bool initCardputerAudio();
static bool initCardputerStdAudio();
static bool probeI2CDevice(uint8_t address, uint32_t freq = 100000);
static void configureKeyboardDriver(CardputerVariant variant);
Audio audio;
unsigned short grays[18];
unsigned short gray;
int sliderPos = 0;
// UI and playback state
int volume = 10;          // 0..21 for ESP32-audioI2S
int bri = 2;              // brightness index 0..4
int brightness[5] = {60, 120, 180, 220, 255};
bool stoped = false;      // spelling kept from original code
bool isPlaying = true;
bool screenOff = false;   // Screen off state
int savedBrightness = 2;  // Brightness saved before screen off
bool showDeleteDialog = false;  // Whether to show delete confirmation dialog
int batteryPercent = 0;   // Cached battery percentage
unsigned long lastBatteryUpdate = 0;  // Last battery update time
const unsigned long BATTERY_UPDATE_INTERVAL = 30000;  // Battery update interval (ms)
String cachedTimeStr = "";  // Cached time string
unsigned long lastTimeUpdate = 0;  // Last time update time
const unsigned long TIME_UPDATE_INTERVAL = 1000;  // Time update interval (ms)
unsigned long lastGraphUpdate = 0;  // Last spectrum graph update time
const unsigned long GRAPH_UPDATE_INTERVAL = 200;  // Spectrum graph update interval (ms)
int lastSelectedIndex = -1;  // Last selected song index
unsigned long selectedTime = 0;  // Time when current song was selected
int selectedScrollPos = 8;  // Scroll position for selected song (initial position)
const unsigned long SELECTED_SCROLL_DELAY = 1000;  // Delay before starting scroll after selection (ms)
String cachedAudioInfo = "";  // Cached audio sample rate and bit depth string
unsigned long lastAudioInfoUpdate = 0;  // Last audio info update time
const unsigned long AUDIO_INFO_UPDATE_INTERVAL = 500;  // Audio info update interval (ms) - check every 500ms after song starts
bool volUp = false;
int n = 0;                // current file index (selected)
int currentPlayingIndex = 0;  // Actually playing song index
int nextS = 0;            // request to switch tracks
// Playback mode: 0=Sequential, 1=Random, 2=Single Repeat
int playMode = 0;  // Default sequential playback
int graphSpeed = 0;
int g[14] = {0};
unsigned short light;
int textPos = 90;
static bool lastHPState = false;
// Task handle for audio task
TaskHandle_t handleAudioTask = NULL;
ESP32Time rtc(0);
//
// Global flag to indicate codec init state so tasks can check it
bool codec_initialized = false;

#define MAX_FILES 100
// Array to store file names
String audioFiles[MAX_FILES];
int fileCount = 0;
// Forward declarations
void Task_TFT(void *pvParameters);
void Task_Audio(void *pvParameters);
void listFiles(fs::FS &fs, const char *dirname, uint8_t levels);
void deleteCurrentFile();  // Delete currently selected file
const lgfx::U8g2font* detectAndGetFont(const String& text);  // Detect language and return appropriate font
void captureScreenshot();  // Capture current screen and save to SD card
void resetClock() {
  rtc.setTime(0, 0, 0, 17, 1, 2021);
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

// Battery helper (assumes 12-bit ADC and ~2:1 divider to 3.3V ADC ref)
static int getBatteryPercent() {
  int raw = analogRead(BAT_ADC_PIN);
  float voltage = (raw / 4095.0f) * 3.3f * 2.0f; // approximate divider x2
  int mv = (int)(voltage * 1000.0f);
  int percent = constrain(map(mv, 3300, 4200, 0, 100), 0, 100);
  return percent;
}

// Simple I2C scan for debugging
static void i2c_scan() {
  Serial.println("I2C scan start");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" I2C device found at 0x%02X\n", addr);
    }
  }
  Serial.println("I2C scan done");
}

static bool probeI2CDevice(uint8_t address, uint32_t freq) {
  Wire.setClock(freq);
  Wire.beginTransmission(address);
  return Wire.endTransmission(true) == 0;
}

static void configureKeyboardDriver(CardputerVariant variant) {
  bool useAdvKeyboard = false;
  auto boardType = M5.getBoard();

  if (variant == CardputerVariant::Advanced) {
    useAdvKeyboard = true;
  } else if (variant == CardputerVariant::Standard) {
    useAdvKeyboard = false;
  } else {
    if (boardType == m5::board_t::board_M5CardputerADV) {
      useAdvKeyboard = true;
    } else if (boardType == m5::board_t::board_M5Cardputer) {
      useAdvKeyboard = false;
    } else {
      bool hasTca = probeI2CDevice(TCA8418_ADDR, 400000);
      bool hasEs8311 = probeI2CDevice(ES8311_ADDR, ES8311_I2C_FREQ);
      bool hasAw = probeI2CDevice(AW88298_ADDR, ES8311_I2C_FREQ);
      if (hasTca || hasEs8311) {
        useAdvKeyboard = true;
      } else if (hasAw) {
        useAdvKeyboard = false;
      } else {
        // Fallback to original board hint if nothing conclusive was found.
        useAdvKeyboard = (boardType == m5::board_t::board_M5CardputerADV);
      }
    }
  }

  Serial.printf("configureKeyboardDriver(): using %s keyboard driver\n",
                useAdvKeyboard ? "TCA8418" : "IO matrix");

  if (useAdvKeyboard) {
    std::unique_ptr<KeyboardReader> reader(new TCA8418KeyboardReader());
    M5Cardputer.Keyboard.begin(std::move(reader));
  } else {
    std::unique_ptr<KeyboardReader> reader(new IOMatrixKeyboardReader());
    M5Cardputer.Keyboard.begin(std::move(reader));
  }
}

static CardputerVariant detectCardputerVariant() {
  auto boardType = M5.getBoard();
  Serial.printf("detectCardputerVariant(): M5.getBoard() -> %d\n", static_cast<int>(boardType));
  if (boardType == m5::board_t::board_M5CardputerADV) {
    g_cardputerVariant = CardputerVariant::Advanced;
    return g_cardputerVariant;
  }
  if (boardType == m5::board_t::board_M5Cardputer) {
    g_cardputerVariant = CardputerVariant::Standard;
    return g_cardputerVariant;
  }

  Wire.end();
  delay(10);
  Wire.begin(CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 100000);
  Wire.setTimeOut(50);
  delay(5);

  if (probeI2CDevice(TCA8418_ADDR, 400000)) {
    Serial.println("Detected TCA8418 keyboard controller (Cardputer-Adv)");
    g_cardputerVariant = CardputerVariant::Advanced;
    return g_cardputerVariant;
  }

  if (probeI2CDevice(ES8311_ADDR, ES8311_I2C_FREQ)) {
    Serial.println("Detected ES8311 codec (Cardputer-Adv)");
    g_cardputerVariant = CardputerVariant::Advanced;
    return g_cardputerVariant;
  }

  if (probeI2CDevice(AW88298_ADDR, ES8311_I2C_FREQ)) {
    Serial.println("Detected AW88298 amplifier (Cardputer v1.1)");
    g_cardputerVariant = CardputerVariant::Standard;
    return g_cardputerVariant;
  }

  Serial.println("Cardputer variant detection inconclusive; will attempt ADV init first");
  g_cardputerVariant = CardputerVariant::Unknown;
  return g_cardputerVariant;
}

static bool initCardputerStdAudio() {
  Serial.println("initCardputerStdAudio(): configuring AW88298 path for Cardputer v1.1");

  Wire.end();
  delay(20);
  Wire.begin(CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 100000);
  Wire.setTimeOut(50);
  delay(5);

  // Enable amplifier via IO expander if present (AW9523 on original Cardputer)
  bool expanderOk = true;
  if (!M5.In_I2C.bitOn(AW9523_ADDR, 0x02, 0b00000100, 400000)) {
    Serial.println("[WARN] Unable to toggle AW9523 output; continuing regardless");
    expanderOk = false;
  }

  auto writeAw = [](uint8_t reg, uint16_t value) {
    uint8_t payload[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    if (!M5.In_I2C.writeRegister(AW88298_ADDR, reg, payload, 2, 400000)) {
      Serial.printf("AW88298 write failed reg=0x%02X\n", reg);
      return false;
    }
    delay(2);
    return true;
  };

  static constexpr uint8_t rate_tbl[] = {4, 5, 6, 8, 10, 11, 15, 20, 22, 44};
  size_t reg0x06_value = 0;
  size_t rate = (44100 + 1102) / 2205;  // Approximation mirroring M5Unified logic
  while (reg0x06_value + 1 < sizeof(rate_tbl) && rate > rate_tbl[reg0x06_value]) {
    ++reg0x06_value;
  }
  reg0x06_value |= 0x14C0;  // I2SBCK=0 (BCK mode 16*2)

  bool awOk = true;
  awOk &= writeAw(0x61, 0x0673);
  awOk &= writeAw(0x04, 0x4040);
  awOk &= writeAw(0x05, 0x0008);
  awOk &= writeAw(0x06, static_cast<uint16_t>(reg0x06_value));
  awOk &= writeAw(0x0C, 0x0064);

  audioBclkPin = CARDPUTER_STD_I2S_BCLK;
  audioLrckPin = CARDPUTER_STD_I2S_LRCK;
  audioDoutPin = CARDPUTER_STD_I2S_DOUT;
  hpDetectPin = -1;
  ampEnablePin = -1;
  lastHPState = false;
  g_cardputerVariant = CardputerVariant::Standard;
  codec_initialized = true;

  audio.setPinout(audioBclkPin, audioLrckPin, audioDoutPin);
  audio.setVolume(volume);
  audio.setBalance(0);

  if (!awOk) {
    Serial.println("[WARN] AW88298 initialisation reported errors; audio may be muted");
  }
  return awOk || expanderOk;
}

static bool initCardputerAudio() {
  // Detect which Cardputer variant we're running on and bring up the
  // appropriate audio path. The ADV board exposes an ES8311 codec that needs
  // explicit register programming, while the original Cardputer relies on an
  // AW88298 amplifier that can be primed via the I/O expander.
  codec_initialized = false;
  CardputerVariant detected = detectCardputerVariant();
  bool ok = false;
  if (detected == CardputerVariant::Advanced) {
    ok = initCardputerAdvCodec();
    if (!ok) {
      Serial.println("[WARN] ADV init failed, attempting standard path");
      ok = initCardputerStdAudio();
    }
  } else if (detected == CardputerVariant::Standard) {
    ok = initCardputerStdAudio();
  } else {
    ok = initCardputerAdvCodec();
    if (!ok) {
      Serial.println("[WARN] ADV init fallback failed, trying standard path");
      ok = initCardputerStdAudio();
    }
  }
  return ok;
}

// Minimal ES8311 blind write helper
static bool es8311_write(uint8_t reg, uint8_t val) {
  uint8_t data = val;
  if (!M5.In_I2C.writeRegister(ES8311_ADDR, reg, &data, 1, ES8311_I2C_FREQ)) {
    Serial.printf("ES8311 I2C write failed reg 0x%02X\n", reg);
    return false;
  }
  delay(2);
  return true;
}




// Simple boot-time sine test: ESP32 drives I2S clocks (master), ES8311 is slave.
static void playTestTone(uint32_t freq_hz, uint32_t duration_ms, uint32_t sample_rate = 44100, uint16_t amplitude = 12000) {
  Serial.printf("Playing test tone: %lu Hz for %lu ms\n", (unsigned long)freq_hz, (unsigned long)duration_ms);
  if (audioBclkPin < 0 || audioLrckPin < 0 || audioDoutPin < 0) {
    Serial.println("Test tone skipped: audio pins not configured");
    return;
  }
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = (int)sample_rate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  // Use non-deprecated standard I2S format
  cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("i2s_driver_install failed (I2S0) - skipping test tone");
    return;
  }
  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = audioBclkPin,
    .ws_io_num = audioLrckPin,
    .data_out_num = audioDoutPin,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("i2s_set_pin failed - skipping test tone");
    i2s_driver_uninstall(I2S_NUM_0);
    return;
  }
  i2s_set_clk(I2S_NUM_0, sample_rate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  const size_t frames_per_buf = 256; // stereo frames
  int16_t buf[frames_per_buf * 2];   // 2 channels
  double phase = 0.0;
  double phase_inc = 2.0 * M_PI * (double)freq_hz / (double)sample_rate;
  uint32_t elapsed = 0;
  uint32_t chunk_ms = (uint32_t)((1000.0 * frames_per_buf) / (double)sample_rate);
  if (chunk_ms == 0) chunk_ms = 6; // ~5.8ms at 44.1kHz
  while (elapsed < duration_ms) {
    for (size_t i = 0; i < frames_per_buf; ++i) {
      int16_t s = (int16_t)(sin(phase) * amplitude);
      phase += phase_inc;
      if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
      buf[2 * i + 0] = s; // Left
      buf[2 * i + 1] = s; // Right
    }
    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, (const char *)buf, sizeof(buf), &bytes_written, portMAX_DELAY);
    elapsed += chunk_ms;
  }
  i2s_driver_uninstall(I2S_NUM_0);
  Serial.println("Test tone done");
}

static bool initCardputerAdvCodec() {
  Serial.println("initCardputerAdvCodec(): preparing ES8311 on Cardputer-Adv");

  Wire.end();
  delay(50);
  Wire.begin(CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 100000);
  Wire.setTimeOut(50);
  delay(10);
  i2c_scan();

  audioBclkPin = CARDPUTER_ADV_I2S_BCLK;
  audioLrckPin = CARDPUTER_ADV_I2S_LRCK;
  audioDoutPin = CARDPUTER_ADV_I2S_DOUT;
  hpDetectPin = CARDPUTER_ADV_HP_DET_PIN;
  ampEnablePin = CARDPUTER_ADV_AMP_EN_PIN;

  if (hpDetectPin >= 0) {
    pinMode(hpDetectPin, INPUT_PULLUP);
  }
  if (ampEnablePin >= 0) {
    pinMode(ampEnablePin, OUTPUT);
    digitalWrite(ampEnablePin, LOW);
    Serial.printf("AMP_EN: held LOW on pin %d until codec init\n", ampEnablePin);
  }

  struct RegisterValue {
    uint8_t reg;
    uint8_t value;
  };

  static constexpr RegisterValue kInitSequence[] = {
      {0x00, 0x80},
      {0x01, 0xB5},
      {0x02, 0x18},
      {0x0D, 0x01},
      {0x12, 0x00},
      {0x13, 0x10},
      {0x32, 0xBF},
      {0x37, 0x08},
  };

  bool ok = true;
  for (const auto &entry : kInitSequence) {
    if (!es8311_write(entry.reg, entry.value)) {
      ok = false;
    }
  }

  codec_initialized = ok;
  if (!codec_initialized) {
    Serial.println("ES8311 init sequence failed");
    g_cardputerVariant = CardputerVariant::Unknown;
    return false;
  }

  g_cardputerVariant = CardputerVariant::Advanced;
  Serial.println("ES8311 init sequence done");
  Serial.printf("Using Cardputer-Adv audio pins: I2C SDA=%d SCL=%d, BCLK=%d LRCK=%d DOUT=%d\n", CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, audioBclkPin, audioLrckPin, audioDoutPin);

  if (hpDetectPin >= 0) {
    bool hpInsertedBoot = (digitalRead(hpDetectPin) == LOW);
    lastHPState = hpInsertedBoot;
    if (hpInsertedBoot) {
      Serial.println("Headphones detected at boot -> keeping speaker AMP OFF");
      if (ampEnablePin >= 0) {
        digitalWrite(ampEnablePin, LOW);
      }
    } else {
      Serial.println("No headphones -> speaker AMP enabled");
      if (ampEnablePin >= 0) {
        digitalWrite(ampEnablePin, HIGH);
      }
    }
  } else if (ampEnablePin >= 0) {
    digitalWrite(ampEnablePin, HIGH);
  }

  playTestTone(440, 1500, 44100, 12000);

  audio.setPinout(audioBclkPin, audioLrckPin, audioDoutPin);
  audio.setVolume(volume);
  audio.setBalance(0);

  return true;
}
void setup() {
  Serial.begin(115200);
  Serial.println("hi");
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
  M5Cardputer.Display.setBrightness(brightness[bri]);
  // Enable UTF-8 support for Chinese character display
  M5Cardputer.Display.setAttribute(utf8_switch, true);
  sprite.createSprite(240, 135);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS)) {
    Serial.println(F("ERROR: SD Mount Failed!"));
  }
  // Read song files from /music directory
  listFiles(SD, "/music", MAX_FILES);
  if (fileCount == 0) {
    Serial.println("No files found in /music, scanning root as fallback");
    listFiles(SD, "/", MAX_FILES);
  }
  if (!initCardputerAudio()) {
    Serial.println("[ERROR] Audio initialisation failed - leaving amplifier disabled");
  }
  configureKeyboardDriver(g_cardputerVariant);
  if (fileCount > 0) {
    Serial.printf("Trying to play: %s\n", audioFiles[n].c_str());
    // Double-check the file exists
    if (SD.exists(audioFiles[n])) {
      // Open the file directly and print size + header bytes for verification
      File f = SD.open(audioFiles[n]);
      if (f) {
        uint32_t sz = f.size();
        Serial.printf("SD open OK, size=%u bytes\n", (unsigned)sz);
        // read first 12 bytes
        uint8_t hdr[12];
        int r = f.read(hdr, sizeof(hdr));
        Serial.printf("First %d bytes: ", r);
        for (int i = 0; i < r; i++) Serial.printf("%02X ", hdr[i]);
        Serial.println();
        // Optional: detect ID3 and log a warning if a large tag may delay playback
        if (r >= 3 && hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3') {
          Serial.println("Note: ID3 tag detected; initial parsing may delay audio start");
        }
        f.close();
      } else {
        Serial.println("SD.open failed (could not read file)");
      }
      if (codec_initialized) {
        audio.connecttoFS(SD, audioFiles[n].c_str());
        currentPlayingIndex = n;  // Sync playing index on initialization
      } else {
        Serial.println("Skipping audio.connecttoFS(): codec not initialized");
      }
    } else {
      Serial.printf("File not found on SD: %s\n", audioFiles[n].c_str());
    }
  } else {
    Serial.println("No audio files found on SD - skipping connect");
  }
  int co = 214;
  for (int i = 0; i < 18; i++) {
    grays[i] = M5Cardputer.Display.color565(co, co, co + 40);
    co = co - 13;
  }
  // Initialize battery display and time cache
  batteryPercent = getBatteryPercent();
  lastBatteryUpdate = millis();
  cachedTimeStr = rtc.getTime().substring(3, 8);
  lastTimeUpdate = millis();
  lastGraphUpdate = millis();
  
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
          Serial.println("HP inserted -> speaker AMP OFF");
          digitalWrite(ampEnablePin, LOW);
        } else {
          Serial.println("HP removed -> speaker AMP ON");
          digitalWrite(ampEnablePin, HIGH);
        }
      }
    }
  }
  delay(200);  // Increase polling interval to reduce CPU usage
}
void draw() {
  if (graphSpeed == 0) {
    gray = grays[15];
    light = grays[11];
    sprite.fillRect(0, 0, 240, 135, gray);
    sprite.fillRect(4, 8, 130, 122, BLACK);
    sprite.fillRect(129, 8, 5, 122, 0x0841);
    sliderPos = map(n, 0, fileCount, 8, 110);
    sprite.fillRect(129, sliderPos, 5, 20, grays[2]);
    sprite.fillRect(131, sliderPos + 4, 1, 12, grays[16]);
    sprite.fillRect(4, 2, 50, 2, ORANGE);
    sprite.fillRect(84, 2, 50, 2, ORANGE);
    sprite.fillRect(190, 2, 45, 2, ORANGE);
    sprite.fillRect(190, 6, 45, 3, grays[4]);
    sprite.drawFastVLine(3, 9, 120, light);
    sprite.drawFastVLine(134, 9, 120, light);
    sprite.drawFastHLine(3, 129, 130, light);
    sprite.drawFastHLine(0, 0, 240, light);
    sprite.drawFastHLine(0, 134, 240, light);
    sprite.fillRect(139, 0, 3, 135, BLACK);
    sprite.fillRect(148, 14, 86, 42, BLACK);
    sprite.fillRect(148, 59, 86, 16, BLACK);  // Playback mode display area
    sprite.fillTriangle(162, 18, 162, 26, 168, 22, GREEN);
    sprite.fillRect(162, 30, 6, 6, RED);
    sprite.drawFastVLine(143, 0, 135, light);
    sprite.drawFastVLine(238, 0, 135, light);
    sprite.drawFastVLine(138, 0, 135, light);
    sprite.drawFastVLine(148, 14, 42, light);
    sprite.drawFastHLine(148, 14, 86, light);
    //buttons
    for (int i = 0; i < 4; i++)
      sprite.fillRoundRect(148 + (i * 22), 94, 18, 18, 3, grays[4]);
    //button icons
    sprite.fillRect(220, 104, 8, 2, grays[13]);
    sprite.fillRect(220, 108, 8, 2, grays[13]);
    sprite.fillTriangle(228, 102, 228, 106, 231, 105, grays[13]);
    sprite.fillTriangle(220, 106, 220, 110, 217, 109, grays[13]);
    if (!stoped) {
      sprite.fillRect(152, 104, 3, 6, grays[13]);
      sprite.fillRect(157, 104, 3, 6, grays[13]);
    } else {
      sprite.fillTriangle(156, 102, 156, 110, 160, 106, grays[13]);
    }
    //volume bar
    sprite.fillRoundRect(172, 82, 60, 3, 2, YELLOW);
    // Volume range 0-21, mapped to position 155-215 (60 pixel range)
    // Use integer calculation for precision
    int volumePos = 155 + (volume * 60 / 21);
    sprite.fillRoundRect(volumePos, 80, 10, 8, 2, grays[2]);
    sprite.fillRoundRect(volumePos + 2, 82, 6, 4, 2, grays[10]);
    // brightness
    sprite.fillRoundRect(172, 124, 30, 3, 2, MAGENTA);
    sprite.fillRoundRect(172 + (bri * 5), 122, 10, 8, 2, grays[2]);
    sprite.fillRoundRect(174 + (bri * 5), 124, 6, 4, 2, grays[10]);
    //BATTERY
    sprite.drawRect(206, 119, 28, 12, GREEN);
    sprite.fillRect(234, 122, 3, 6, GREEN);
    //graph - Reduce spectrum graph update frequency to save CPU
    unsigned long now = millis();
    if (!stoped && (now - lastGraphUpdate >= GRAPH_UPDATE_INTERVAL)) {
      for (int i = 0; i < 14; i++) {
        g[i] = random(1, 5);
      }
      lastGraphUpdate = now;
    }
    for (int i = 0; i < 14; i++) {
      for (int j = 0; j < g[i]; j++)
        sprite.fillRect(172 + (i * 4), 50 - j * 3, 3, 2, grays[4]);
    }
    // Check if selected song changed, reset scroll position if changed
    if (lastSelectedIndex != n) {
      lastSelectedIndex = n;
      selectedTime = now;
      selectedScrollPos = 8;  // Reset scroll position
    }
    
    sprite.setTextDatum(0);
    // Display 7 lines (16 pixel line height)
    if (n < 3)
      for (int i = 0; i < 7; i++) {
        if (i < fileCount) {
          // Set color: playing song=red, selected song=white, others=green
          if (i == currentPlayingIndex) {
            sprite.setTextColor(RED, BLACK);  // Currently playing song: red
          } else if (i == n) {
            sprite.setTextColor(WHITE, BLACK);  // Selected song: white
          } else {
            sprite.setTextColor(GREEN, BLACK);  // Other songs: green
          }
          // Extract filename (remove path and extension)
          String fileName = audioFiles[i];
          int lastSlash = fileName.lastIndexOf('/');
          if (lastSlash >= 0) {
            fileName = fileName.substring(lastSlash + 1);
          }
          // Remove file extension (.mp3, .wav, etc.)
          int lastDot = fileName.lastIndexOf('.');
          if (lastDot >= 0) {
            fileName = fileName.substring(0, lastDot);
          }
          
          // Detect language and set appropriate font
          const lgfx::U8g2font* detectedFont = detectAndGetFont(fileName);
          if (detectedFont) {
            sprite.setFont(detectedFont);
          } else {
            sprite.setTextFont(0);  // Default font for English
          }
          
          // If selected song and stayed for more than 1 second, show scroll effect
          if (i == n && (now - selectedTime >= SELECTED_SCROLL_DELAY)) {
            // Calculate scroll position (scroll from right to left)
            // LIST box range: x from 4 to 133 (fillRect(4, 8, 130, 122), right edge=4+130-1=133)
            // Scrollbar area: x from 129 to 133 (fillRect(129, 8, 5, 122), right edge=129+5-1=133)
            // Text display area: x from 8 to 128 (must not enter scrollbar area 129-133)
            const int TEXT_LEFT = 8;   // Text left boundary
            const int TEXT_RIGHT = 128; // Text right boundary (scrollbar starts at 129, 1px margin)
            const int LIST_LEFT = 4;    // LIST box left boundary
            const int LIST_TOP = 8;     // LIST box top boundary
            const int LIST_WIDTH = 125; // Text display area width (128-8+1=121, but use 125 to ensure no overflow)
            const int LIST_HEIGHT = 122; // LIST box height
            
            if (graphSpeed == 0) {  // Update only every 4 frames
              selectedScrollPos = selectedScrollPos - 2;
              // Estimate text width (Chinese font ~6px wide, English ~4px wide, average 5)
              int textWidth = fileName.length() * 5;
              // If text completely scrolled to left (text right edge beyond left boundary), reset to right
              if (selectedScrollPos + textWidth < TEXT_LEFT) {
                selectedScrollPos = TEXT_RIGHT;  // Start from right
              }
              // Ensure text doesn't exceed right boundary (must not enter scrollbar area)
              if (selectedScrollPos > TEXT_RIGHT) {
                selectedScrollPos = TEXT_RIGHT;
              }
            }
            // Display full filename (no truncation), use clipRect to limit within text display area (excluding scrollbar)
            sprite.setClipRect(LIST_LEFT, LIST_TOP, LIST_WIDTH, LIST_HEIGHT);  // Limit drawing area, width 125 ensures no entry into scrollbar
            sprite.drawString(fileName, selectedScrollPos, 10 + (i * 16));
            sprite.clearClipRect();  // Clear clip region
          } else {
            // Normal display: truncate to first 20 characters (ensure no overflow)
            String displayName = fileName;
            if (displayName.length() > 20) {
              displayName = displayName.substring(0, 20);
            }
            sprite.drawString(displayName, 8, 10 + (i * 16));
          }
        }
      }
    int yos = 0;
    if (n >= 3)
      for (int i = n - 3; i < n - 3 + 7; i++) {
        if (i < fileCount) {
          // Set color: playing song=red, selected song=white, others=green
          if (i == currentPlayingIndex) {
            sprite.setTextColor(RED, BLACK);  // Currently playing song: red
          } else if (i == n) {
            sprite.setTextColor(WHITE, BLACK);  // Selected song: white
          } else {
            sprite.setTextColor(GREEN, BLACK);  // Other songs: green
          }
          // Extract filename (remove path and extension)
          String fileName = audioFiles[i];
          int lastSlash = fileName.lastIndexOf('/');
          if (lastSlash >= 0) {
            fileName = fileName.substring(lastSlash + 1);
          }
          // Remove file extension (.mp3, .wav, etc.)
          int lastDot = fileName.lastIndexOf('.');
          if (lastDot >= 0) {
            fileName = fileName.substring(0, lastDot);
          }
          
          // Detect language and set appropriate font
          const lgfx::U8g2font* detectedFont = detectAndGetFont(fileName);
          if (detectedFont) {
            sprite.setFont(detectedFont);
          } else {
            sprite.setTextFont(0);  // Default font for English
          }
          
          // If selected song and stayed for more than 1 second, show scroll effect
          if (i == n && (now - selectedTime >= SELECTED_SCROLL_DELAY)) {
            // Calculate scroll position (scroll from right to left)
            // LIST box range: x from 4 to 133 (fillRect(4, 8, 130, 122), right edge=4+130-1=133)
            // Scrollbar area: x from 129 to 133 (fillRect(129, 8, 5, 122), right edge=129+5-1=133)
            // Text display area: x from 8 to 128 (must not enter scrollbar area 129-133)
            const int TEXT_LEFT = 8;   // Text left boundary
            const int TEXT_RIGHT = 128; // Text right boundary (scrollbar starts at 129, 1px margin)
            const int LIST_LEFT = 4;    // LIST box left boundary
            const int LIST_TOP = 8;     // LIST box top boundary
            const int LIST_WIDTH = 125; // Text display area width (128-8+1=121, but use 125 to ensure no overflow)
            const int LIST_HEIGHT = 122; // LIST box height
            
            if (graphSpeed == 0) {  // Update only every 4 frames
              selectedScrollPos = selectedScrollPos - 2;
              // Estimate text width (Chinese font ~6px wide, English ~4px wide, average 5)
              int textWidth = fileName.length() * 5;
              // If text completely scrolled to left (text right edge beyond left boundary), reset to right
              if (selectedScrollPos + textWidth < TEXT_LEFT) {
                selectedScrollPos = TEXT_RIGHT;  // Start from right
              }
              // Ensure text doesn't exceed right boundary (must not enter scrollbar area)
              if (selectedScrollPos > TEXT_RIGHT) {
                selectedScrollPos = TEXT_RIGHT;
              }
            }
            // Display full filename (no truncation), use clipRect to limit within text display area (excluding scrollbar)
            sprite.setClipRect(LIST_LEFT, LIST_TOP, LIST_WIDTH, LIST_HEIGHT);  // Limit drawing area, width 125 ensures no entry into scrollbar
            sprite.drawString(fileName, selectedScrollPos, 10 + (yos * 16));
            sprite.clearClipRect();  // Clear clip region
          } else {
            // Normal display: truncate to first 20 characters (ensure no overflow)
            String displayName = fileName;
            if (displayName.length() > 20) {
              displayName = displayName.substring(0, 20);
            }
            sprite.drawString(displayName, 8, 10 + (yos * 16));
          }
        }
        yos++;
      }
    // Restore default font for other text
    sprite.setTextFont(0);
    sprite.setTextColor(grays[1], gray);
    sprite.drawString("WINAMP", 150, 4);
    sprite.setTextColor(grays[2], gray);
    sprite.drawString("LIST", 58, 0);
    sprite.setTextColor(grays[4], gray);
    sprite.drawString("VOL", 150, 80);
    sprite.drawString("LIG", 150, 122);
    if (isPlaying) {
      sprite.setTextColor(grays[8], BLACK);
      sprite.drawString("P", 152, 18);
      sprite.drawString("L", 152, 27);
      sprite.drawString("A", 152, 36);
      sprite.drawString("Y", 152, 45);
    } else {
      sprite.setTextColor(grays[8], BLACK);
      sprite.drawString("S", 152, 18);
      sprite.drawString("T", 152, 27);
      sprite.drawString("O", 152, 36);
      sprite.drawString("P", 152, 45);
    }
    sprite.setTextColor(GREEN, BLACK);
  sprite.setFont(&DSEG7_Classic_Mini_Regular_16);
    // Cache time string to reduce string operations
    if (!stoped) {
      if (now - lastTimeUpdate >= TIME_UPDATE_INTERVAL) {
        cachedTimeStr = rtc.getTime().substring(3, 8);
        lastTimeUpdate = now;
      }
      sprite.drawString(cachedTimeStr, 172, 18);
    }
    sprite.setTextFont(0);
    // Reduce battery refresh rate: update every 30 seconds to reduce ADC reads and display jitter
    if (now - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL) {
      batteryPercent = getBatteryPercent();
      lastBatteryUpdate = now;
    }
    sprite.setTextDatum(3);
    sprite.drawString(String(batteryPercent) + "%", 220, 121);
    sprite.setTextColor(BLACK, grays[4]);
    sprite.drawString("M", 220, 96);
    sprite.drawString("N", 198, 96);
    sprite.drawString("P", 176, 96);
    sprite.drawString("A", 154, 96);
    sprite.setTextColor(BLACK, grays[5]);
    sprite.drawString(">>", 202, 103);
    sprite.drawString("<<", 180, 103);
    
    // Display playback mode
    sprite.setTextFont(0);
    sprite.setTextColor(GREEN, BLACK);
    sprite.setTextDatum(0);
    String modeText = "";
    if (playMode == 0) {
      modeText = "SEQ";  // Sequential playback
    } else if (playMode == 1) {
      modeText = "RND";  // Random playback
    } else if (playMode == 2) {
      modeText = "ONE";  // Single repeat
    }
    sprite.drawString(modeText, 150, 63);
    
    // Update audio info (sample rate and bit depth) cache periodically
    // Delay reading to allow decoder to initialize (check every 500ms)
    if (isPlaying && !stoped && (now - lastAudioInfoUpdate >= AUDIO_INFO_UPDATE_INTERVAL)) {
      uint32_t sampleRate = audio.getSampleRate();
      uint8_t bitsPerSample = audio.getBitsPerSample();
      if (sampleRate > 0 && bitsPerSample > 0) {
        // Convert sample rate to kHz for shorter display
        // Format: "44.1/16" or "48/16" or "96/24" etc.
        float sampleRateKHz = sampleRate / 1000.0f;
        // Format with 1 decimal place if needed (e.g., 44.1, 48.0 -> 48)
        if (sampleRateKHz == (int)sampleRateKHz) {
          // Integer kHz, no decimal needed
          cachedAudioInfo = String((int)sampleRateKHz) + "/" + String(bitsPerSample);
        } else {
          // Has decimal part, show 1 decimal place
          cachedAudioInfo = String(sampleRateKHz, 1) + "/" + String(bitsPerSample);
        }
        lastAudioInfoUpdate = now;
      } else {
        // Decoder not ready yet, keep empty or previous value
        if (cachedAudioInfo.length() == 0) {
          cachedAudioInfo = "";
        }
      }
    }
    
    // Display audio info (sample rate/bit depth) on the right side of playback mode
    if (cachedAudioInfo.length() > 0) {
      sprite.setTextDatum(2);  // Right alignment
      sprite.drawString(cachedAudioInfo, 232, 63);  // Right edge at x=232 (148+86-2)
      sprite.setTextDatum(0);  // Restore left alignment
    }
    
    // If showing delete dialog, draw dialog
    if (showDeleteDialog) {
      // Draw dialog background and border (increased height to accommodate Chinese)
      sprite.fillRect(20, 40, 200, 70, BLACK);
      sprite.drawRect(20, 40, 200, 70, WHITE);
      sprite.setTextFont(0);
      sprite.setTextColor(WHITE, BLACK);
      sprite.setTextDatum(0);
      sprite.drawString("Delete song?", 30, 45);
      if (n < fileCount) {
        // Detect language and use appropriate font (same as song list)
        String fileName = audioFiles[n];
        // Extract filename (remove path)
        int lastSlash = fileName.lastIndexOf('/');
        if (lastSlash >= 0) {
          fileName = fileName.substring(lastSlash + 1);
        }
        // Remove file extension
        int lastDot = fileName.lastIndexOf('.');
        if (lastDot >= 0) {
          fileName = fileName.substring(0, lastDot);
        }
        // Detect language and set appropriate font
        const lgfx::U8g2font* detectedFont = detectAndGetFont(fileName);
        if (detectedFont) {
          sprite.setFont(detectedFont);
        } else {
          sprite.setTextFont(0);  // Default font for English
        }
        // Truncate filename (max 20 characters, same as list)
        if (fileName.length() > 20) {
          fileName = fileName.substring(0, 20);
        }
        // Use 16 pixel line height, same as song list
        sprite.drawString(fileName, 30, 57);
        // Restore default font for other text
        sprite.setTextFont(0);
      }
      sprite.drawString("Y:Yes  C:Cancel", 30, 75);
    }
    
    sprite.pushSprite(0, 0);
  }
  graphSpeed++;
  if (graphSpeed == 4) graphSpeed = 0;
}
void Task_TFT(void *pvParameters) {
  while (1) {
    M5Cardputer.update();
    // Check for key press events
    if (M5Cardputer.Keyboard.isChange()) {
      if (M5Cardputer.Keyboard.isKeyPressed('a')) {
        isPlaying = !isPlaying;
        stoped = !stoped;
      }  // Toggle the playback state
      if (M5Cardputer.Keyboard.isKeyPressed('v')) {
        isPlaying = false;
        volUp = true;
        volume = volume + 5;
        if (volume > 20) volume = 5;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('-')) {
        // '-' key: Decrease volume
        volUp = true;
        volume = volume - 1;
        if (volume < 0) volume = 0;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('=')) {
        // '=' key: Increase volume
        volUp = true;
        volume = volume + 1;
        if (volume > 21) volume = 21;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('l')) {
        bri++;
        if (bri == 5) bri = 0;
        M5Cardputer.Display.setBrightness(brightness[bri]);
      }
      if (M5Cardputer.Keyboard.isKeyPressed('n')) {
        resetClock();
        isPlaying = false;
        textPos = 90;
        if (playMode == 1) {
          // Random mode: select a random song (avoid current playing song if possible)
          if (fileCount <= 1) {
            n = 0;
          } else {
            int newIndex;
            do {
              newIndex = random(0, fileCount);
            } while (newIndex == currentPlayingIndex);
            n = newIndex;
          }
        } else {
          // Sequential mode: next song
          n++;
          if (n >= fileCount) n = 0;
        }
        nextS = 1;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('p')) {
        resetClock();
        isPlaying = false;
        textPos = 90;
        if (playMode == 1) {
          // Random mode: select a random song (avoid current playing song if possible)
          if (fileCount <= 1) {
            n = 0;
          } else {
            int newIndex;
            do {
              newIndex = random(0, fileCount);
            } while (newIndex == currentPlayingIndex);
            n = newIndex;
          }
        } else {
          // Sequential mode: previous song
          n--;
          if (n < 0) n = fileCount - 1;
        }
        nextS = 1;
      }
      if (M5Cardputer.Keyboard.isKeyPressed(';')) {
        n--;
        if (n < 0)
          n = fileCount - 1;  // Loop to last song
      }
      if (M5Cardputer.Keyboard.isKeyPressed('.')) {
        n++;
        if (n >= fileCount)
          n = 0;  // Loop to first song
      }
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
        resetClock();
        stoped = false;
        isPlaying = false;
        textPos = 90;
        nextS = 1;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('m')) {
        // 'm' key: Toggle playback mode (Sequential -> Random -> Single Repeat -> Sequential)
        playMode++;
        if (playMode > 2) playMode = 0;
        Serial.printf("Play mode changed to: %d\n", playMode);
      }
      if (M5Cardputer.Keyboard.isKeyPressed('s')) {
        // 's' key: Screen off/on toggle
        if (screenOff) {
          // Screen on: Restore brightness before screen off
          screenOff = false;
          bri = savedBrightness;
          M5Cardputer.Display.wakeup();
          M5Cardputer.Display.setBrightness(brightness[bri]);
          Serial.println("Screen ON - restored brightness");
        } else {
          // Screen off: Save current brightness and turn off screen
          savedBrightness = bri;
          screenOff = true;
          M5Cardputer.Display.sleep();
          Serial.println("Screen OFF - saved brightness");
        }
      }
      if (M5Cardputer.Keyboard.isKeyPressed('d')) {
        // 'd' key: Show delete confirmation dialog
        if (!showDeleteDialog && fileCount > 0 && n < fileCount) {
          showDeleteDialog = true;
          Serial.printf("Delete dialog shown for: %s\n", audioFiles[n].c_str());
        }
      }
      if (showDeleteDialog) {
        if (M5Cardputer.Keyboard.isKeyPressed('y')) {
          // 'y' key: Confirm delete
          deleteCurrentFile();
          showDeleteDialog = false;
        }
        if (M5Cardputer.Keyboard.isKeyPressed('c')) {
          // 'c' key: Cancel delete
          showDeleteDialog = false;
          Serial.println("Delete cancelled");
        }
      }
      if (M5Cardputer.Keyboard.isKeyPressed('f')) {
        // 'f' key: Capture screenshot
        captureScreenshot();
      }
    }
    // If screen is off, skip drawing to save CPU
    if (!screenOff) {
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
    if (volUp) {
      audio.setVolume(volume);
      isPlaying = true;
      volUp = false;
    }

    if (nextS) {
      audio.stopSong();
      Serial.printf("Task_Audio: next track requested: %s\n", audioFiles[n].c_str());
      if (SD.exists(audioFiles[n])) {
        audio.connecttoFS(SD, audioFiles[n].c_str());
        currentPlayingIndex = n;  // Update actual playing index
        // Reset audio info cache when switching songs (will be updated after decoder initializes)
        cachedAudioInfo = "";
        lastAudioInfoUpdate = millis();  // Reset timer to allow decoder initialization time
      } else {
        Serial.printf("Task_Audio: file not found: %s\n", audioFiles[n].c_str());
      }
      isPlaying = true;
      nextS = 0;
    }

    if (isPlaying && codec_initialized && !stoped) {
      audio.loop();

      if (millis() - lastLog >= 500) {
        int ampState = -1;
        if (ampEnablePin >= 0) {
          ampState = digitalRead(ampEnablePin);
        }
        Serial.printf("Task_Audio: audio.loop() heartbeat  codec_initialized=%d AMP_EN=%d ES_ADDR=0x%02X\n", codec_initialized ? 1 : 0, ampState, ES8311_ADDR);
        lastLog = millis();
      }

      vTaskDelay(playDelay);
    } else {
      vTaskDelay(idleDelay);
    }
  }
}
// Function to play a song from a given URL or file path
void playSong(const char *source) {
  //audio.stopSong(); // Stop any current playback
  audio.connecttohost(source);  // Open and play the new song from a URL
}
// Function to stop playback
void stopSong() {
  audio.stopSong();
}
// Function to open and prepare a song (without autoplay)
void openSong(const char *source) {
  audio.connecttohost(source);  // Opens the source, similar to play
}
void listFiles(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);
  // Ensure dirname starts with '/'
  String dir = String(dirname);
  if (!dir.startsWith("/")) dir = String("/") + dir;
  File root = fs.open(dir.c_str());
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }
  File file = root.openNextFile();
  while (file && fileCount < MAX_FILES) {
    if (file.isDirectory()) {
      // Build subdirectory path ensuring single leading '/'
      String sub = String(file.name());
      if (!sub.startsWith("/")) sub = dir + String("/") + sub;
      Serial.print("DIR : ");
      Serial.println(sub.c_str());
      if (levels) {
        listFiles(fs, sub.c_str(), levels - 1);
      }
    } else {
      // Build full file path
      String fname = String(file.name());
      if (!fname.startsWith("/")) fname = dir + String("/") + fname;
      // Filter by supported extensions: .mp3 and .wav
      String lower = fname; lower.toLowerCase();
      int dot = lower.lastIndexOf('.') ;
      bool supported = false;
      if (dot >= 0) {
        String ext = lower.substring(dot + 1);
        if (ext == "mp3" || ext == "wav") supported = true;
      }
      if (supported) {
        Serial.print("FILE: ");
        Serial.println(fname.c_str());
        audioFiles[fileCount] = fname;
        Serial.printf("Stored audioFiles[%d] = %s\n", fileCount, audioFiles[fileCount].c_str());
        fileCount++;
      } else {
        Serial.printf("SKIP (unsupported): %s\n", fname.c_str());
      }
    }
    file = root.openNextFile();
  }
}
void audio_eof_mp3(const char *info) {
  resetClock();
  Serial.print("eof_mp3     ");
  Serial.println(info);
  
  // Determine next song based on playback mode
  if (playMode == 0) {
    // Sequential playback: next song
    currentPlayingIndex++;
    if (currentPlayingIndex >= fileCount) currentPlayingIndex = 0;
  } else if (playMode == 1) {
    // Random playback: random selection
    currentPlayingIndex = random(0, fileCount);
  } else if (playMode == 2) {
    // Single repeat: don't change index, continue playing current song
    // currentPlayingIndex remains unchanged
  }
  
  n = currentPlayingIndex;  // Sync selected index to playing index
  Serial.printf("eof: opening next file: %s (index %d, mode %d)\n", 
                audioFiles[currentPlayingIndex].c_str(), currentPlayingIndex, playMode);
  if (SD.exists(audioFiles[currentPlayingIndex])) {
    audio.connecttoFS(SD, audioFiles[currentPlayingIndex].c_str());
    // Reset audio info cache when auto-switching songs (will be updated after decoder initializes)
    cachedAudioInfo = "";
    lastAudioInfoUpdate = millis();  // Reset timer to allow decoder initialization time
  } else {
    Serial.printf("eof: next file not found: %s\n", audioFiles[currentPlayingIndex].c_str());
  }
}

// Delete currently selected file
void deleteCurrentFile() {
  if (fileCount == 0 || n >= fileCount) {
    Serial.println("No file to delete");
    return;
  }
  
  int deleteIndex = n;  // File index to delete (currently selected)
  String fileToDelete = audioFiles[deleteIndex];
  Serial.printf("Attempting to delete: %s (index %d)\n", fileToDelete.c_str(), deleteIndex);
  
  // Record if playing before delete, and which song is playing
  bool wasPlaying = (isPlaying && !stoped);
  int playingIndexBeforeDelete = currentPlayingIndex;  // Playing song index before delete
  bool deletingPlayingSong = (deleteIndex == playingIndexBeforeDelete);  // Whether deleting the currently playing song
  
  // If deleting currently playing song, stop first
  if (wasPlaying && deletingPlayingSong) {
    audio.stopSong();
  }
  
  // Delete file from SD card
  if (SD.exists(fileToDelete)) {
    if (SD.remove(fileToDelete)) {
      Serial.printf("File deleted successfully: %s\n", fileToDelete.c_str());
    } else {
      Serial.printf("Failed to delete file: %s\n", fileToDelete.c_str());
      return;
    }
  } else {
    Serial.printf("File not found on SD: %s\n", fileToDelete.c_str());
  }
  
  // Remove file from list
  for (int i = deleteIndex; i < fileCount - 1; i++) {
    audioFiles[i] = audioFiles[i + 1];
  }
  fileCount--;
  
  // Adjust playing index: if delete index <= playing index, playing index needs to decrease by 1
  int playingIndexAfterDelete = playingIndexBeforeDelete;
  if (deleteIndex <= playingIndexBeforeDelete) {
    playingIndexAfterDelete--;  // Playing index moves forward
    if (playingIndexAfterDelete < 0 && fileCount > 0) playingIndexAfterDelete = 0;
    if (playingIndexAfterDelete >= fileCount && fileCount > 0) playingIndexAfterDelete = fileCount - 1;
  }
  
  // Adjust current selected index n
  if (deleteIndex < n) {
    n--;  // Deleted file is before current position, index moves forward
  } else if (deleteIndex == n) {
    // Deleted the currently selected song, need to adjust selected index
    if (n >= fileCount) {
      n = fileCount - 1;
    }
    if (n < 0) {
      n = 0;
    }
  }
  // If deleteIndex > n, n remains unchanged (deleted file is after current position)
  
  // Ensure n is within valid range
  if (n < 0) n = 0;
  if (n >= fileCount && fileCount > 0) n = fileCount - 1;
  
  // If deleting currently playing song, need to switch to new song
  if (deletingPlayingSong) {
    // Playing index before delete has been adjusted, now need to switch to adjusted index
    if (fileCount > 0 && playingIndexAfterDelete >= 0 && playingIndexAfterDelete < fileCount) {
      n = playingIndexAfterDelete;  // Make selected index follow playing index
      currentPlayingIndex = playingIndexAfterDelete;  // Update global variable
      resetClock();
      textPos = 90;
      nextS = 1;
      // If playing before delete, continue playing new song after delete
      if (wasPlaying) {
        isPlaying = true;
        stoped = false;
      }
      Serial.printf("Switched to new current file: %s (index %d)\n", audioFiles[n].c_str(), n);
    } else {
      // No more files, stop playback
      isPlaying = false;
      stoped = true;
      currentPlayingIndex = 0;
      Serial.println("No more files available");
    }
  } else {
    // Deleted song was not playing, continue playing current song, don't switch
    // Only need to update currentPlayingIndex to adjusted index
    currentPlayingIndex = playingIndexAfterDelete;
    Serial.printf("Deleted file (index %d) was not playing (was index %d, now %d), continuing with: %s\n", 
                  deleteIndex, playingIndexBeforeDelete, playingIndexAfterDelete, 
                  fileCount > 0 && playingIndexAfterDelete < fileCount ? audioFiles[playingIndexAfterDelete].c_str() : "none");
  }
}

// Capture screenshot and save to SD card
void captureScreenshot() {
  const int SCREEN_WIDTH = 240;
  const int SCREEN_HEIGHT = 135;
  
  // Create /screen directory if it doesn't exist
  if (!SD.exists("/screen")) {
    SD.mkdir("/screen");
    Serial.println("Created /screen directory");
  }
  
  // Generate filename with timestamp
  String timestamp = rtc.getTime("%Y%m%d_%H%M%S");
  String filename = "/screen/screenshot_" + timestamp + ".bmp";
  
  // Read pixel data from sprite (RGB565 format)
  uint16_t* pixelBuffer = (uint16_t*)heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (!pixelBuffer) {
    Serial.println("Failed to allocate memory for screenshot");
    return;
  }
  
  // Read entire sprite content (sprite contains all drawn content)
  sprite.readRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, pixelBuffer);
  
  // Open file for writing
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.printf("Failed to create file: %s\n", filename.c_str());
    heap_caps_free(pixelBuffer);
    return;
  }
  
  // BMP file header (14 bytes)
  uint8_t bmpHeader[14] = {
    'B', 'M',  // Signature
    0, 0, 0, 0,  // File size (will be filled later)
    0, 0, 0, 0,  // Reserved
    54, 0, 0, 0  // Offset to pixel data (54 bytes)
  };
  
  // BMP info header (40 bytes)
  uint32_t rowSize = ((SCREEN_WIDTH * 3 + 3) / 4) * 4;  // Row size must be multiple of 4
  uint32_t imageSize = rowSize * SCREEN_HEIGHT;
  uint32_t fileSize = 54 + imageSize;
  
  uint8_t bmpInfo[40] = {
    40, 0, 0, 0,  // Info header size
    (uint8_t)(SCREEN_WIDTH & 0xFF), (uint8_t)((SCREEN_WIDTH >> 8) & 0xFF), (uint8_t)((SCREEN_WIDTH >> 16) & 0xFF), (uint8_t)((SCREEN_WIDTH >> 24) & 0xFF),  // Width
    (uint8_t)(SCREEN_HEIGHT & 0xFF), (uint8_t)((SCREEN_HEIGHT >> 8) & 0xFF), (uint8_t)((SCREEN_HEIGHT >> 16) & 0xFF), (uint8_t)((SCREEN_HEIGHT >> 24) & 0xFF),  // Height
    1, 0,  // Planes
    24, 0,  // Bits per pixel (24-bit RGB)
    0, 0, 0, 0,  // Compression (none)
    (uint8_t)(imageSize & 0xFF), (uint8_t)((imageSize >> 8) & 0xFF), (uint8_t)((imageSize >> 16) & 0xFF), (uint8_t)((imageSize >> 24) & 0xFF),  // Image size
    0, 0, 0, 0,  // X pixels per meter
    0, 0, 0, 0,  // Y pixels per meter
    0, 0, 0, 0,  // Colors used
    0, 0, 0, 0   // Important colors
  };
  
  // Update file size in header
  bmpHeader[2] = fileSize & 0xFF;
  bmpHeader[3] = (fileSize >> 8) & 0xFF;
  bmpHeader[4] = (fileSize >> 16) & 0xFF;
  bmpHeader[5] = (fileSize >> 24) & 0xFF;
  
  // Write headers
  file.write(bmpHeader, 14);
  file.write(bmpInfo, 40);
  
  // Convert RGB565 to BGR888 and write pixel data (BMP stores pixels bottom-to-top)
  uint8_t* rowBuffer = (uint8_t*)heap_caps_malloc(rowSize, MALLOC_CAP_DMA);
  if (!rowBuffer) {
    Serial.println("Failed to allocate memory for row buffer");
    file.close();
    heap_caps_free(pixelBuffer);
    return;
  }
  
  for (int y = SCREEN_HEIGHT - 1; y >= 0; y--) {  // BMP is bottom-to-top
    int rowOffset = 0;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      uint16_t pixel = pixelBuffer[y * SCREEN_WIDTH + x];
      // readRect returns swap565 format (byte-swapped RGB565)
      // Swap bytes first to convert swap565 to rgb565: low byte and high byte are swapped
      uint16_t rgb565 = ((pixel & 0xFF) << 8) | ((pixel >> 8) & 0xFF);
      
      // Now extract from standard RGB565 format: RRRRR GGGGGG BBBBB
      // RGB565 bit layout: RRRRRGGG GGGBBBBB (bits 15-0)
      uint8_t r5 = (rgb565 >> 11) & 0x1F;  // bits 15-11: red 5 bits
      uint8_t g6 = (rgb565 >> 5) & 0x3F;   // bits 10-5: green 6 bits
      uint8_t b5 = rgb565 & 0x1F;          // bits 4-0: blue 5 bits
      
      // Convert to 8-bit using proper bit expansion (matches M5GFX library)
      // R: 5-bit -> 8-bit: (r5 << 3) | (r5 >> 2)
      // G: 6-bit -> 8-bit: (g6 << 2) | (g6 >> 4)
      // B: 5-bit -> 8-bit: (b5 << 3) | (b5 >> 2)
      uint8_t r = (r5 << 3) | (r5 >> 2);
      uint8_t g = (g6 << 2) | (g6 >> 4);
      uint8_t b = (b5 << 3) | (b5 >> 2);
      
      // BMP format uses BGR order
      rowBuffer[rowOffset++] = b;  // B
      rowBuffer[rowOffset++] = g;  // G
      rowBuffer[rowOffset++] = r;  // R
    }
    // Pad row to multiple of 4 bytes
    while (rowOffset < rowSize) {
      rowBuffer[rowOffset++] = 0;
    }
    file.write(rowBuffer, rowSize);
  }
  
  file.close();
  heap_caps_free(pixelBuffer);
  heap_caps_free(rowBuffer);
  
  Serial.printf("Screenshot saved: %s\n", filename.c_str());
}

