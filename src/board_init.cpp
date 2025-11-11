#include "../include/board_init.hpp"
#include "../include/audio_manager.hpp"
#include "../include/config.hpp"
#include <Wire.h>
#include "M5Cardputer.h"
#include <utility/Keyboard/KeyboardReader/IOMatrix.h>
#include <utility/Keyboard/KeyboardReader/TCA8418.h>
#include "driver/i2s.h"
#include <math.h>

namespace BoardInit {

// Pin constants and device addresses are provided by config.hpp

// Local I2C probe helper
static bool probeI2CDevice(uint8_t address, uint32_t freq = 100000) {
  Wire.setClock(freq);
  Wire.beginTransmission(address);
  return Wire.endTransmission(true) == 0;
}

// Simple I2C scan for debugging
static void i2c_scan() {
  LOG_PRINTLN("I2C scan start");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      LOG_PRINTF(" I2C device found at 0x%02X\n", addr);
    }
  }
  LOG_PRINTLN("I2C scan done");
}

// ES8311 write helper
static bool es8311_write(uint8_t reg, uint8_t val) {
  uint8_t data = val;
  if (!M5.In_I2C.writeRegister(ES8311_ADDR, reg, &data, 1, ES8311_I2C_FREQ)) {
    LOG_PRINTF("ES8311 I2C write failed reg 0x%02X\n", reg);
    return false;
  }
  delay(2);
  return true;
}

// Simple boot-time sine test
static void playTestTone(int bclkPin, int lrckPin, int doutPin, 
                         uint32_t freq_hz, uint32_t duration_ms, 
                         uint32_t sample_rate = 44100, uint16_t amplitude = 12000) {
  LOG_PRINTF("Playing test tone: %lu Hz for %lu ms\n", (unsigned long)freq_hz, (unsigned long)duration_ms);
  if (bclkPin < 0 || lrckPin < 0 || doutPin < 0) {
    LOG_PRINTLN("Test tone skipped: audio pins not configured");
    return;
  }
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = (int)sample_rate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
    LOG_PRINTLN("i2s_driver_install failed (I2S0) - skipping test tone");
    return;
  }
  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = bclkPin,
    .ws_io_num = lrckPin,
    .data_out_num = doutPin,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    LOG_PRINTLN("i2s_set_pin failed - skipping test tone");
    i2s_driver_uninstall(I2S_NUM_0);
    return;
  }
  i2s_set_clk(I2S_NUM_0, sample_rate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  const size_t frames_per_buf = 256;
  int16_t buf[frames_per_buf * 2];
  double phase = 0.0;
  double phase_inc = 2.0 * M_PI * (double)freq_hz / (double)sample_rate;
  uint32_t elapsed = 0;
  uint32_t chunk_ms = (uint32_t)((1000.0 * frames_per_buf) / (double)sample_rate);
  if (chunk_ms == 0) chunk_ms = 6;
  while (elapsed < duration_ms) {
    for (size_t i = 0; i < frames_per_buf; ++i) {
      int16_t s = (int16_t)(sin(phase) * amplitude);
      phase += phase_inc;
      if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
      buf[2 * i + 0] = s;
      buf[2 * i + 1] = s;
    }
    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, (const char *)buf, sizeof(buf), &bytes_written, portMAX_DELAY);
    elapsed += chunk_ms;
  }
  i2s_driver_uninstall(I2S_NUM_0);
  LOG_PRINTLN("Test tone done");
}

Variant detectVariant() {
  auto boardType = M5.getBoard();
  if (boardType == m5::board_t::board_M5CardputerADV) return Variant::Advanced;
  if (boardType == m5::board_t::board_M5Cardputer) return Variant::Standard;

  Wire.end();
  delay(10);
  Wire.begin(CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 100000);
  Wire.setTimeOut(50);
  delay(5);

  if (probeI2CDevice(TCA8418_ADDR, 400000)) {
    return Variant::Advanced;
  }
  if (probeI2CDevice(ES8311_ADDR, 400000)) {
    return Variant::Advanced;
  }
  if (probeI2CDevice(AW88298_ADDR, 400000)) {
    return Variant::Standard;
  }
  return Variant::Unknown;
}

bool initStandardAudio(int& bclkPin, int& lrckPin, int& doutPin,
                       int& hpDetectPin, int& ampEnablePin,
                       bool& codecInitialized, int volume) {
  LOG_PRINTLN("initCardputerStdAudio(): configuring AW88298 path for Cardputer v1.1");

  Wire.end();
  delay(20);
  Wire.begin(CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 100000);
  Wire.setTimeOut(50);
  delay(5);

  // Enable amplifier via IO expander if present (AW9523 on original Cardputer)
  bool expanderOk = true;
  if (!M5.In_I2C.bitOn(AW9523_ADDR, 0x02, 0b00000100, 400000)) {
    LOG_PRINTLN("[WARN] Unable to toggle AW9523 output; continuing regardless");
    expanderOk = false;
  }

  auto writeAw = [](uint8_t reg, uint16_t value) {
    uint8_t payload[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    if (!M5.In_I2C.writeRegister(AW88298_ADDR, reg, payload, 2, 400000)) {
      LOG_PRINTF("AW88298 write failed reg=0x%02X\n", reg);
      return false;
    }
    delay(2);
    return true;
  };

  static constexpr uint8_t rate_tbl[] = {4, 5, 6, 8, 10, 11, 15, 20, 22, 44};
  size_t reg0x06_value = 0;
  size_t rate = (44100 + 1102) / 2205;
  while (reg0x06_value + 1 < sizeof(rate_tbl) && rate > rate_tbl[reg0x06_value]) {
    ++reg0x06_value;
  }
  reg0x06_value |= 0x14C0;

  bool awOk = true;
  awOk &= writeAw(0x61, 0x0673);
  awOk &= writeAw(0x04, 0x4040);
  awOk &= writeAw(0x05, 0x0008);
  awOk &= writeAw(0x06, static_cast<uint16_t>(reg0x06_value));
  awOk &= writeAw(0x0C, 0x0064);

  bclkPin = CARDPUTER_STD_I2S_BCLK;
  lrckPin = CARDPUTER_STD_I2S_LRCK;
  doutPin = CARDPUTER_STD_I2S_DOUT;
  hpDetectPin = -1;
  ampEnablePin = -1;
  codecInitialized = true;

  AudioManager::setPinout(bclkPin, lrckPin, doutPin);
  AudioManager::setVolume(volume);
  AudioManager::setBalance(0);

  if (!awOk) {
    LOG_PRINTLN("[WARN] AW88298 initialisation reported errors; audio may be muted");
  }
  return awOk || expanderOk;
}

bool initAdvancedCodec(int& bclkPin, int& lrckPin, int& doutPin,
                       int& hpDetectPin, int& ampEnablePin,
                       bool& codecInitialized, int volume) {
  LOG_PRINTLN("initCardputerAdvCodec(): preparing ES8311 on Cardputer-Adv");

  Wire.end();
  delay(50);
  Wire.begin(CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 100000);
  Wire.setTimeOut(50);
  delay(10);
  i2c_scan();

  bclkPin = CARDPUTER_ADV_I2S_BCLK;
  lrckPin = CARDPUTER_ADV_I2S_LRCK;
  doutPin = CARDPUTER_ADV_I2S_DOUT;
  hpDetectPin = CARDPUTER_ADV_HP_DET_PIN;
  ampEnablePin = CARDPUTER_ADV_AMP_EN_PIN;

  if (hpDetectPin >= 0) {
    pinMode(hpDetectPin, INPUT_PULLUP);
  }
  if (ampEnablePin >= 0) {
    pinMode(ampEnablePin, OUTPUT);
    digitalWrite(ampEnablePin, LOW);
    LOG_PRINTF("AMP_EN: held LOW on pin %d until codec init\n", ampEnablePin);
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

  codecInitialized = ok;
  if (!codecInitialized) {
    LOG_PRINTLN("ES8311 init sequence failed");
    return false;
  }

  LOG_PRINTLN("ES8311 init sequence done");
  LOG_PRINTF("Using Cardputer-Adv audio pins: I2C SDA=%d SCL=%d, BCLK=%d LRCK=%d DOUT=%d\n", 
                CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, bclkPin, lrckPin, doutPin);

  if (hpDetectPin >= 0) {
    bool hpInsertedBoot = (digitalRead(hpDetectPin) == LOW);
    if (hpInsertedBoot) {
      LOG_PRINTLN("Headphones detected at boot -> keeping speaker AMP OFF");
      if (ampEnablePin >= 0) {
        digitalWrite(ampEnablePin, LOW);
      }
    } else {
      LOG_PRINTLN("No headphones -> speaker AMP enabled");
      if (ampEnablePin >= 0) {
        digitalWrite(ampEnablePin, HIGH);
      }
    }
  } else if (ampEnablePin >= 0) {
    digitalWrite(ampEnablePin, HIGH);
  }

  playTestTone(bclkPin, lrckPin, doutPin, 440, 1500, 44100, 12000);

  AudioManager::setPinout(bclkPin, lrckPin, doutPin);
  AudioManager::setVolume(volume);
  AudioManager::setBalance(0);

  return true;
}

bool initAudioForDetectedVariant(Variant detected,
                                 int& bclkPin, int& lrckPin, int& doutPin,
                                 int& hpDetectPin, int& ampEnablePin,
                                 bool& codecInitialized, int volume) {
  codecInitialized = false;
  bool ok = false;
  if (detected == Variant::Advanced) {
    ok = initAdvancedCodec(bclkPin, lrckPin, doutPin, hpDetectPin, ampEnablePin, codecInitialized, volume);
    if (!ok) {
      LOG_PRINTLN("[WARN] ADV init failed, attempting standard path");
      ok = initStandardAudio(bclkPin, lrckPin, doutPin, hpDetectPin, ampEnablePin, codecInitialized, volume);
    }
  } else if (detected == Variant::Standard) {
    ok = initStandardAudio(bclkPin, lrckPin, doutPin, hpDetectPin, ampEnablePin, codecInitialized, volume);
  } else {
    ok = initAdvancedCodec(bclkPin, lrckPin, doutPin, hpDetectPin, ampEnablePin, codecInitialized, volume);
    if (!ok) {
      LOG_PRINTLN("[WARN] ADV init fallback failed, trying standard path");
      ok = initStandardAudio(bclkPin, lrckPin, doutPin, hpDetectPin, ampEnablePin, codecInitialized, volume);
    }
  }
  return ok;
}

void configureKeyboard(Variant variant) {
  bool useAdvKeyboard = false;
  auto boardType = M5.getBoard();

  if (variant == Variant::Advanced) {
    useAdvKeyboard = true;
  } else if (variant == Variant::Standard) {
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
        useAdvKeyboard = (boardType == m5::board_t::board_M5CardputerADV);
      }
    }
  }

  LOG_PRINTF("configureKeyboardDriver(): using %s keyboard driver\n",
                useAdvKeyboard ? "TCA8418" : "IO matrix");

  if (useAdvKeyboard) {
    std::unique_ptr<KeyboardReader> reader(new TCA8418KeyboardReader());
    M5Cardputer.Keyboard.begin(std::move(reader));
  } else {
    std::unique_ptr<KeyboardReader> reader(new IOMatrixKeyboardReader());
    M5Cardputer.Keyboard.begin(std::move(reader));
  }
}

}  // namespace BoardInit


