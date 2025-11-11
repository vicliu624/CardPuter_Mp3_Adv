#pragma once

#include <Arduino.h>
#include <FS.h>
#include "M5Cardputer.h"
#include "app_state.hpp"

// Step 8: Hardware initialization extraction (scaffold)
// This module will encapsulate board/codec init and keyboard driver config.
// For now, just provide the interfaces; implementation will be migrated stepwise.

namespace BoardInit {

enum class Variant { Unknown, Standard, Advanced };

// Detect Cardputer variant (ADV with ES8311 / Standard with AW88298)
Variant detectVariant();

// Initialize audio path for Standard board (AW88298-driven)
bool initStandardAudio(int& bclkPin, int& lrckPin, int& doutPin,
                       int& hpDetectPin, int& ampEnablePin,
                       bool& codecInitialized, int volume);

// Initialize codec path for Advanced board (ES8311)
bool initAdvancedCodec(int& bclkPin, int& lrckPin, int& doutPin,
                       int& hpDetectPin, int& ampEnablePin,
                       bool& codecInitialized, int volume);

// High-level init selecting Standard/Advanced path
bool initAudioForDetectedVariant(Variant detected,
                                 int& bclkPin, int& lrckPin, int& doutPin,
                                 int& hpDetectPin, int& ampEnablePin,
                                 bool& codecInitialized, int volume);

// Configure keyboard driver depending on variant (TCA8418 vs IO matrix)
void configureKeyboard(Variant variant);

}  // namespace BoardInit


