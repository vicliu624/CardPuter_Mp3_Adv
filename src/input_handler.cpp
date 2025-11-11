#include <Arduino.h>
#include "M5Cardputer.h"
#include "../include/input_handler.hpp"
#include "../include/config.hpp"

// Forward declaration
extern void resetClock();

namespace InputHandler {

static void cyclePlayMode(AppState& appState) {
  int modeInt = static_cast<int>(appState.playMode);
  modeInt++;
  if (modeInt > 2) modeInt = 0;
  appState.playMode = static_cast<PlaybackMode>(modeInt);
      LOG_PRINTF("Play mode changed to: %d\n", modeInt);
}

bool processBasicToggles(AppState& appState) {
  bool needRedraw = false;

  // 'm' key: toggle playback mode
  if (M5Cardputer.Keyboard.isKeyPressed('m')) {
    cyclePlayMode(appState);
    needRedraw = true;
  }

  // 's' key: screen on/off toggle with brightness save/restore
  if (M5Cardputer.Keyboard.isKeyPressed('s')) {
    if (appState.screenOff) {
      // Screen on
      M5Cardputer.Display.setBrightness(BRIGHTNESS_VALUES[appState.brightnessIndex]);
      appState.screenOff = false;
      LOG_PRINTLN("Screen ON - restored brightness");
    } else {
      // Screen off
      appState.savedBrightness = appState.brightnessIndex;
      M5Cardputer.Display.setBrightness(0);
      appState.screenOff = true;
      LOG_PRINTLN("Screen OFF - saved brightness");
    }
    needRedraw = true;
  }

  // 'i' key: toggle ID3 page and reset album scroll
  if (M5Cardputer.Keyboard.isKeyPressed('i')) {
    appState.showID3Page = !appState.showID3Page;
    if (appState.showID3Page) {
      appState.id3AlbumScrollPos = 0;
      appState.id3AlbumSelectTime = millis();
    }
    needRedraw = true;
  }

  return needRedraw;
}

bool processPlaybackAndList(AppState& appState) {
  bool needRedraw = false;

  // ';' previous in list
  if (M5Cardputer.Keyboard.isKeyPressed(';')) {
    appState.currentSelectedIndex--;
    if (appState.currentSelectedIndex < 0)
      appState.currentSelectedIndex = appState.fileCount > 0 ? appState.fileCount - 1 : 0;
    needRedraw = true;
  }
  // '.' next in list
  if (M5Cardputer.Keyboard.isKeyPressed('.')) {
    appState.currentSelectedIndex++;
    if (appState.currentSelectedIndex >= appState.fileCount)
      appState.currentSelectedIndex = 0;
    needRedraw = true;
  }
  // 'n' next song (respect random)
  if (M5Cardputer.Keyboard.isKeyPressed('n')) {
    resetClock();
    if (appState.playMode == PlaybackMode::Random) {
      if (appState.fileCount <= 1) {
        appState.currentSelectedIndex = 0;
      } else {
        int newIndex;
        do {
          newIndex = random(0, appState.fileCount);
        } while (newIndex == appState.currentPlayingIndex);
        appState.currentSelectedIndex = newIndex;
      }
    } else {
      appState.currentSelectedIndex++;
      if (appState.currentSelectedIndex >= appState.fileCount) appState.currentSelectedIndex = 0;
    }
    appState.isPlaying = false;
    // Note: stopped is not set here - it will be set to false in Task_Audio after nextS is processed
    appState.nextS = 1;
    needRedraw = true;
  }
  // 'p' previous song (respect random)
  if (M5Cardputer.Keyboard.isKeyPressed('p')) {
    resetClock();
    if (appState.playMode == PlaybackMode::Random) {
      if (appState.fileCount <= 1) {
        appState.currentSelectedIndex = 0;
      } else {
        int newIndex;
        do {
          newIndex = random(0, appState.fileCount);
        } while (newIndex == appState.currentPlayingIndex);
        appState.currentSelectedIndex = newIndex;
      }
    } else {
      appState.currentSelectedIndex--;
      if (appState.currentSelectedIndex < 0) appState.currentSelectedIndex = appState.fileCount - 1;
    }
    appState.isPlaying = false;
    // Note: stopped is not set here - it will be set to false in Task_Audio after nextS is processed
    appState.nextS = 1;
    needRedraw = true;
  }
  // Enter: request play selected
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    resetClock();
    appState.isPlaying = false;
    appState.stopped = false;
    appState.nextS = 1;
    needRedraw = true;
  }
  return needRedraw;
}

bool processDeleteAndScreenshot(AppState& appState, const Actions& actions) {
  bool needRedraw = false;
  // 'd' open dialog
  if (M5Cardputer.Keyboard.isKeyPressed('d')) {
    if (!appState.showDeleteDialog && appState.fileCount > 0 && appState.currentSelectedIndex < appState.fileCount) {
      appState.showDeleteDialog = true;
      LOG_PRINTF("Delete dialog shown for: %s\n", appState.audioFiles[appState.currentSelectedIndex].c_str());
      needRedraw = true;
    }
  }
  if (appState.showDeleteDialog) {
    if (M5Cardputer.Keyboard.isKeyPressed('y')) {
      if (actions.deleteCurrentFile) actions.deleteCurrentFile();
      appState.showDeleteDialog = false;
      needRedraw = true;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('c')) {
      appState.showDeleteDialog = false;
      LOG_PRINTLN("Delete cancelled");
      needRedraw = true;
    }
  }
  // 'f' screenshot
  if (M5Cardputer.Keyboard.isKeyPressed('f')) {
    if (actions.captureScreenshot) actions.captureScreenshot();
    needRedraw = true;
  }
  return needRedraw;
}

}  // namespace InputHandler


