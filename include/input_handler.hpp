#pragma once

#include <Arduino.h>
#include "app_state.hpp"

namespace InputHandler {

// Handle basic keyboard toggles and selection-independent actions.
// Currently handles:
// - 'm': cycle playback mode (SEQ -> RND -> ONE)
// - 's': screen on/off toggle with brightness restore/save
// - 'i': toggle ID3 page and reset its scroll timer
//
// Returns true if any UI needs immediate redraw.
bool processBasicToggles(AppState& appState);

// External actions that require calling back into app code
struct Actions {
  void (*captureScreenshot)() = nullptr;
  void (*deleteCurrentFile)() = nullptr;
};

// Handle list navigation and playback related keys:
// - ';' '.' : move selection up/down (wrap around)
// - 'n' 'p' : next/previous (respect playMode for random/seq)
// - Enter   : request play selected
//
// Returns true if anything changed requiring redraw.
bool processPlaybackAndList(AppState& appState);

// Handle delete dialog and screenshot keys:
// - 'd' : open delete dialog
// - 'y' : confirm delete (calls actions.deleteCurrentFile if provided)
// - 'c' : cancel delete
// - 'f' : capture screenshot (calls actions.captureScreenshot if provided)
//
// Returns true if UI needs redraw.
bool processDeleteAndScreenshot(AppState& appState, const Actions& actions);

}  // namespace InputHandler


