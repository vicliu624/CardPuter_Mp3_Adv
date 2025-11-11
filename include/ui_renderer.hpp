#pragma once

#include <Arduino.h>
#include <FS.h>
#include "M5Cardputer.h"
#include "app_state.hpp"

// Forward declarations
class Audio;
class ESP32Time;

namespace UiRenderer {

// Render the ID3 information page into the given sprite.
// Requires access to the gray color table and detectAndGetFont routine.
void drawId3Page(M5Canvas& sprite,
                 AppState& appState,
                 const unsigned short* grays,
                 const lgfx::U8g2font* (*detectAndGetFont)(const String&));

// Render the main view (file list, status bar, controls, etc.)
// Requires access to RTC, battery function, and font detection.
// Audio access is now through AudioManager.
void drawMainView(M5Canvas& sprite,
                  AppState& appState,
                  const unsigned short* grays,
                  unsigned short& gray,
                  unsigned short& light,
                  int& sliderPos,
                  ESP32Time& rtc,
                  int (*getBatteryPercent)(),
                  const lgfx::U8g2font* (*detectAndGetFont)(const String&));

}  // namespace UiRenderer


