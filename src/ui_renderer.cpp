#include <FS.h>
#include <SD.h>
#include <cstdio>  // For snprintf
#include "../include/ui_renderer.hpp"
#include "../include/config.hpp"
#include "../include/image_utils.hpp"
#include "../include/audio_manager.hpp"
#include <ESP32Time.h>
#include "font.h"

namespace UiRenderer {

// Helper function to extract display name from full file path
static String extractDisplayName(const String& fullPath) {
  String fileName = fullPath;
  int lastSlash = fileName.lastIndexOf('/');
  if (lastSlash >= 0) {
    fileName = fileName.substring(lastSlash + 1);
  }
  int lastDot = fileName.lastIndexOf('.');
  if (lastDot >= 0) {
    fileName = fileName.substring(0, lastDot);
  }
  return fileName;
}

void drawId3Page(M5Canvas& sprite,
                 AppState& appState,
                 const unsigned short* grays,
                 const lgfx::U8g2font* (*detectAndGetFont)(const String&)) {
  sprite.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
  const int coverX = COVER_X;
  const int coverY = COVER_Y;
  const int coverW = COVER_WIDTH;
  const int coverH = COVER_HEIGHT;
  if (appState.id3CoverBuf && appState.id3CoverSize > 0) {
    bool isJpeg = (appState.id3CoverSize >= 2 && appState.id3CoverBuf[0] == 0xFF && appState.id3CoverBuf[1] == 0xD8);
    bool isPng  = (appState.id3CoverSize >= 8 && appState.id3CoverBuf[0] == 0x89 && appState.id3CoverBuf[1] == 0x50 && appState.id3CoverBuf[2] == 0x4E && appState.id3CoverBuf[3] == 0x47);
    if (isJpeg) {
      sprite.drawJpg(appState.id3CoverBuf, appState.id3CoverSize, coverX, coverY, coverW, coverH);
    } else if (isPng) {
      sprite.drawPng(appState.id3CoverBuf, appState.id3CoverSize, coverX, coverY, coverW, coverH);
    } else {
      sprite.fillRect(coverX, coverY, coverW, coverH, grays[4]);
      sprite.drawRect(coverX, coverY, coverW, coverH, grays[10]);
      sprite.setTextColor(grays[14], grays[4]);
      sprite.setTextDatum(4);
      sprite.drawString(PLACEHOLDER_NO_COVER, coverX + coverW/2, coverY + coverH/2);
      sprite.setTextDatum(0);
    }
  } else if (appState.id3CoverSize == 0 && appState.id3CoverPos > 0) {
    File f = SD.open(appState.audioFiles[appState.currentPlayingIndex]);
    if (f) {
      if (f.seek(appState.id3CoverPos)) {
        const size_t scanMax = (appState.id3CoverLen > 0 && appState.id3CoverLen < COVER_SCAN_MAX) ? appState.id3CoverLen : (size_t)COVER_SCAN_MAX;
        size_t startOff = 0;
        ImageFormat fmt = ImageFormat::Unknown;
        findImageStart(f, scanMax, startOff, fmt);
        uint32_t imgW = 0, imgH = 0;
        getImageSize(f, appState.id3CoverPos + startOff, fmt, imgW, imgH);
        f.seek(appState.id3CoverPos + startOff);
        if (fmt != ImageFormat::Unknown) {
          float scale = 1.0f;
          if (imgW > 0 && imgH > 0) {
            float sx2 = (float)coverW / (float)imgW;
            float sy2 = (float)coverH / (float)imgH;
            scale = sx2 < sy2 ? sx2 : sy2;
            if (scale <= 0.0f || scale > 1.0f) scale = 1.0f;
          } else {
            scale = 0.5f;
          }
          if (fmt == ImageFormat::JPEG) {
            sprite.drawJpg(&f, coverX, coverY, coverW, coverH, 0, 0, scale, 0.0f);
          } else if (fmt == ImageFormat::PNG) {
            sprite.drawPng(&f, coverX, coverY, coverW, coverH, 0, 0, scale, 0.0f);
          } else if (fmt == ImageFormat::BMP) {
            sprite.drawBmp(&f, coverX, coverY, coverW, coverH, 0, 0, scale, 0.0f);
          } else if (fmt == ImageFormat::QOI) {
            sprite.drawQoi(&f, coverX, coverY, coverW, coverH, 0, 0, scale, 0.0f);
          }
        } else {
          sprite.fillRect(coverX, coverY, coverW, coverH, grays[4]);
          sprite.drawRect(coverX, coverY, coverW, coverH, grays[10]);
          sprite.setTextColor(grays[14], grays[4]);
          sprite.setTextDatum(4);
          sprite.drawString(PLACEHOLDER_NO_COVER, coverX + coverW/2, coverY + coverH/2);
          sprite.setTextDatum(0);
        }
      } else {
        sprite.fillRect(coverX, coverY, coverW, coverH, grays[4]);
        sprite.drawRect(coverX, coverY, coverW, coverH, grays[10]);
      }
      f.close();
    } else {
      sprite.fillRect(coverX, coverY, coverW, coverH, grays[4]);
      sprite.drawRect(coverX, coverY, coverW, coverH, grays[10]);
    }
  } else {
    sprite.fillRect(coverX, coverY, coverW, coverH, grays[4]);
    sprite.drawRect(coverX, coverY, coverW, coverH, grays[10]);
    sprite.setTextColor(grays[14], grays[4]);
    sprite.setTextDatum(4);
    sprite.drawString(PLACEHOLDER_NO_COVER, coverX + coverW/2, coverY + coverH/2);
    sprite.setTextDatum(0);
  }

  const int albumY = ALBUM_TEXT_Y;
  const int albumH = ALBUM_TEXT_HEIGHT;
  (void)albumH;
  sprite.fillRect(coverX, albumY, coverW, albumH, BLACK);
  sprite.setTextColor(WHITE, BLACK);
  const lgfx::U8g2font* albumFont = detectAndGetFont(appState.id3Album);
  if (albumFont) sprite.setFont(albumFont); else sprite.setTextFont(0);
  int16_t tw = sprite.textWidth(appState.id3Album);
  int clipW = coverW;
  if (tw <= clipW) {
    String albumDraw = appState.id3Album.length() ? appState.id3Album : String(PLACEHOLDER_UNKNOWN_ALBUM);
    sprite.drawString(albumDraw, coverX, albumY);
  } else {
    if (appState.id3AlbumSelectTime == 0) appState.id3AlbumSelectTime = millis();
    if (millis() - appState.id3AlbumSelectTime >= ID3_SCROLL_DELAY && appState.graphSpeed == 0) {
      appState.id3AlbumScrollPos -= SCROLL_STEP;
      if (appState.id3AlbumScrollPos + tw < 0) appState.id3AlbumScrollPos = clipW;
    }
    sprite.setClipRect(coverX, albumY, coverW, albumH);
    sprite.drawString(appState.id3Album, coverX + appState.id3AlbumScrollPos, albumY);
    sprite.clearClipRect();
  }

  sprite.setTextFont(0);
  const lgfx::U8g2font* artistFont = detectAndGetFont(appState.id3Artist);
  if (artistFont) sprite.setFont(artistFont); else sprite.setTextFont(0);
  sprite.setTextColor(WHITE, BLACK);
  {
    String artistDraw = appState.id3Artist.length() ? appState.id3Artist : String(PLACEHOLDER_UNKNOWN_ARTIST);
    sprite.drawString(artistDraw, ARTIST_X, ARTIST_Y);
  }
  const lgfx::U8g2font* titleFont = detectAndGetFont(appState.id3Title);
  if (titleFont) sprite.setFont(titleFont); else sprite.setTextFont(0);
  sprite.setTextColor(grays[2], BLACK);
  sprite.drawString(appState.id3Title, TITLE_X, TITLE_Y);
  sprite.pushSprite(0, 0);
}

void drawMainView(M5Canvas& sprite,
                  AppState& appState,
                  const unsigned short* grays,
                  unsigned short& gray,
                  unsigned short& light,
                  int& sliderPos,
                  ESP32Time& rtc,
                  int (*getBatteryPercent)(),
                  const lgfx::U8g2font* (*detectAndGetFont)(const String&)) {
  if (appState.graphSpeed == 0) {
    gray = grays[15];
    light = grays[11];
    sprite.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, gray);
    sprite.fillRect(LIST_BOX_X, LIST_BOX_Y, LIST_BOX_WIDTH, LIST_BOX_HEIGHT, BLACK);
    sprite.fillRect(SCROLLBAR_X, SCROLLBAR_Y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, SCROLLBAR_COLOR);
    sliderPos = map(appState.currentSelectedIndex, 0, appState.fileCount, SCROLLBAR_Y, SCROLLBAR_Y + SCROLLBAR_HEIGHT - SCROLLBAR_THUMB_HEIGHT);
    sprite.fillRect(SCROLLBAR_X, sliderPos, SCROLLBAR_WIDTH, SCROLLBAR_THUMB_HEIGHT, grays[2]);
    sprite.fillRect(SCROLLBAR_X + SCROLLBAR_THUMB_INDICATOR_OFFSET, sliderPos + SCROLLBAR_THUMB_INDICATOR_OFFSET, SCROLLBAR_THUMB_INDICATOR_WIDTH, SCROLLBAR_THUMB_INDICATOR_HEIGHT, grays[16]);
    sprite.fillRect(STATUS_BAR_ORANGE_LINE1_X, STATUS_BAR_ORANGE_LINE1_Y, STATUS_BAR_ORANGE_LINE1_WIDTH, 2, ORANGE);
    sprite.fillRect(STATUS_BAR_ORANGE_LINE2_X, STATUS_BAR_ORANGE_LINE1_Y, STATUS_BAR_ORANGE_LINE1_WIDTH, 2, ORANGE);
    sprite.fillRect(STATUS_BAR_ORANGE_LINE3_X, STATUS_BAR_ORANGE_LINE1_Y, STATUS_BAR_ORANGE_LINE3_WIDTH, 2, ORANGE);
    sprite.fillRect(STATUS_BAR_ORANGE_LINE3_X, STATUS_BAR_GRAY_LINE_Y, STATUS_BAR_ORANGE_LINE3_WIDTH, STATUS_BAR_GRAY_LINE_HEIGHT, grays[4]);
    sprite.drawFastVLine(BORDER_LEFT_X, BORDER_LEFT_Y, BORDER_LEFT_HEIGHT, light);
    sprite.drawFastVLine(BORDER_RIGHT_X, BORDER_LEFT_Y, BORDER_LEFT_HEIGHT, light);
    sprite.drawFastHLine(BORDER_BOTTOM_X, BORDER_BOTTOM_Y, BORDER_BOTTOM_WIDTH, light);
    sprite.drawFastHLine(0, BORDER_TOP_Y, BORDER_TOP_WIDTH, light);
    sprite.drawFastHLine(0, BORDER_BOTTOM_EDGE_Y, BORDER_TOP_WIDTH, light);
    sprite.fillRect(CONTROL_PANEL_X, 0, CONTROL_PANEL_WIDTH, SCREEN_HEIGHT, BLACK);
    sprite.fillRect(CONTROL_PANEL_INFO_X, CONTROL_PANEL_INFO_Y, CONTROL_PANEL_INFO_WIDTH, CONTROL_PANEL_INFO_HEIGHT, BLACK);
    sprite.fillRect(CONTROL_PANEL_INFO_X, CONTROL_PANEL_MODE_Y, CONTROL_PANEL_INFO_WIDTH, CONTROL_PANEL_MODE_HEIGHT, BLACK);
    sprite.fillTriangle(PLAY_TRIANGLE_X1, PLAY_TRIANGLE_Y1, PLAY_TRIANGLE_X1, PLAY_TRIANGLE_Y2, PLAY_TRIANGLE_X2, PLAY_TRIANGLE_Y_CENTER, GREEN);
    sprite.fillRect(STOP_RECT_X, STOP_RECT_Y, STOP_RECT_SIZE, STOP_RECT_SIZE, RED);
    sprite.drawFastVLine(CONTROL_PANEL_SEPARATOR_X, 0, SCREEN_HEIGHT, light);
    sprite.drawFastVLine(CONTROL_PANEL_RIGHT_EDGE, 0, SCREEN_HEIGHT, light);
    sprite.drawFastVLine(CONTROL_PANEL_LEFT_EDGE, 0, SCREEN_HEIGHT, light);
    sprite.drawFastVLine(CONTROL_PANEL_INFO_X, CONTROL_PANEL_INFO_Y, CONTROL_PANEL_INFO_HEIGHT, light);
    sprite.drawFastHLine(CONTROL_PANEL_INFO_X, CONTROL_PANEL_INFO_Y, CONTROL_PANEL_INFO_WIDTH, light);
    for (int i = 0; i < BUTTON_COUNT; i++)
      sprite.fillRoundRect(BUTTON_BASE_X + (i * BUTTON_SPACING), BUTTON_BASE_Y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_ROUND_RADIUS, grays[4]);
    sprite.fillRect(BUTTON_ICON_PREV_X, BUTTON_ICON_PREV_Y1, BUTTON_ICON_PREV_WIDTH, BUTTON_ICON_PREV_HEIGHT, grays[13]);
    sprite.fillRect(BUTTON_ICON_PREV_X, BUTTON_ICON_PREV_Y2, BUTTON_ICON_PREV_WIDTH, BUTTON_ICON_PREV_HEIGHT, grays[13]);
    sprite.fillTriangle(BUTTON_ICON_NEXT_X, BUTTON_ICON_NEXT_Y1, BUTTON_ICON_NEXT_X, BUTTON_ICON_NEXT_Y2, BUTTON_ICON_NEXT_X2, BUTTON_ICON_NEXT_Y_CENTER, grays[13]);
    sprite.fillTriangle(BUTTON_ICON_PREV_X, BUTTON_ICON_NEXT_Y_CENTER, BUTTON_ICON_PREV_X, BUTTON_ICON_STOP_Y_CENTER, 217, 109, grays[13]);
    if (!appState.stopped) {
      sprite.fillRect(BUTTON_ICON_PLAY_X1, BUTTON_ICON_PLAY_Y, BUTTON_ICON_PLAY_WIDTH, BUTTON_ICON_PLAY_HEIGHT, grays[13]);
      sprite.fillRect(BUTTON_ICON_PLAY_X2, BUTTON_ICON_PLAY_Y, BUTTON_ICON_PLAY_WIDTH, BUTTON_ICON_PLAY_HEIGHT, grays[13]);
    } else {
      sprite.fillTriangle(BUTTON_ICON_STOP_X1, BUTTON_ICON_STOP_Y1, BUTTON_ICON_STOP_X1, BUTTON_ICON_STOP_Y2, BUTTON_ICON_STOP_X2, BUTTON_ICON_STOP_Y_CENTER, grays[13]);
    }
    sprite.fillRoundRect(VOLUME_BAR_X, VOLUME_BAR_Y, VOLUME_BAR_WIDTH, VOLUME_BAR_HEIGHT, 2, YELLOW);
    int volumePos = VOLUME_BAR_START_X + (appState.volume * VOLUME_BAR_RANGE / VOLUME_MAX);
    sprite.fillRoundRect(volumePos, VOLUME_SLIDER_Y, VOLUME_SLIDER_WIDTH, VOLUME_SLIDER_HEIGHT, 2, grays[2]);
    sprite.fillRoundRect(volumePos + VOLUME_SLIDER_INNER_X_OFFSET, VOLUME_BAR_Y + VOLUME_SLIDER_INNER_Y_OFFSET, VOLUME_SLIDER_INNER_WIDTH, VOLUME_SLIDER_INNER_HEIGHT, 2, grays[10]);
    sprite.fillRoundRect(BRIGHTNESS_BAR_X, BRIGHTNESS_BAR_Y, BRIGHTNESS_BAR_WIDTH, BRIGHTNESS_BAR_HEIGHT, 2, MAGENTA);
    sprite.fillRoundRect(BRIGHTNESS_BAR_X + (appState.brightnessIndex * BRIGHTNESS_SLIDER_STEP), BRIGHTNESS_SLIDER_Y, BRIGHTNESS_SLIDER_WIDTH, BRIGHTNESS_SLIDER_HEIGHT, 2, grays[2]);
    sprite.fillRoundRect(BRIGHTNESS_BAR_X + (appState.brightnessIndex * BRIGHTNESS_SLIDER_STEP) + BRIGHTNESS_SLIDER_INNER_X_OFFSET, BRIGHTNESS_BAR_Y + BRIGHTNESS_SLIDER_INNER_Y_OFFSET, BRIGHTNESS_SLIDER_INNER_WIDTH, BRIGHTNESS_SLIDER_INNER_HEIGHT, 2, grays[10]);
    sprite.drawRect(BATTERY_X, BATTERY_Y, BATTERY_WIDTH, BATTERY_HEIGHT, GREEN);
    sprite.fillRect(BATTERY_TERMINAL_X, BATTERY_TERMINAL_Y, BATTERY_TERMINAL_WIDTH, BATTERY_TERMINAL_HEIGHT, GREEN);
    unsigned long now = millis();
    if (!appState.stopped && (now - appState.lastGraphUpdate >= GRAPH_UPDATE_INTERVAL)) {
      for (int i = 0; i < GRAPH_BAR_COUNT; i++) {
        appState.graphBars[i] = random(1, GRAPH_BAR_MAX);
      }
      appState.lastGraphUpdate = now;
    }
    for (int i = 0; i < GRAPH_BAR_COUNT; i++) {
      for (int j = 0; j < appState.graphBars[i]; j++)
        sprite.fillRect(GRAPH_BASE_X + (i * GRAPH_BAR_SPACING), GRAPH_BASE_Y - j * GRAPH_BAR_HEIGHT_STEP, GRAPH_BAR_WIDTH, GRAPH_BAR_HEIGHT, grays[4]);
    }
    if (appState.lastSelectedIndex != appState.currentSelectedIndex) {
      appState.lastSelectedIndex = appState.currentSelectedIndex;
      appState.selectedTime = now;
      appState.selectedScrollPos = SCROLL_INITIAL_POS;
    }
    
    sprite.setTextDatum(0);
    if (appState.currentSelectedIndex < LIST_SCROLL_THRESHOLD)
      for (int i = 0; i < LIST_VISIBLE_LINES; i++) {
        if (i < appState.fileCount) {
          if (i == appState.currentPlayingIndex) {
            sprite.setTextColor(RED, BLACK);
          } else if (i == appState.currentSelectedIndex) {
            sprite.setTextColor(WHITE, BLACK);
          } else {
            sprite.setTextColor(GREEN, BLACK);
          }
          String fileName = extractDisplayName(appState.audioFiles[i]);
          const lgfx::U8g2font* detectedFont = detectAndGetFont(fileName);
          if (detectedFont) {
            sprite.setFont(detectedFont);
          } else {
            sprite.setTextFont(0);
          }
          if (i == appState.currentSelectedIndex && (now - appState.selectedTime >= SELECTED_SCROLL_DELAY)) {
            if (appState.graphSpeed == 0) {
              appState.selectedScrollPos = appState.selectedScrollPos - SCROLL_STEP;
              int textWidth = fileName.length() * TEXT_WIDTH_ESTIMATE_PX;
              if (appState.selectedScrollPos + textWidth < TEXT_LEFT) {
                appState.selectedScrollPos = TEXT_RIGHT;
              }
              if (appState.selectedScrollPos > TEXT_RIGHT) {
                appState.selectedScrollPos = TEXT_RIGHT;
              }
            }
            sprite.setClipRect(LIST_BOX_X, LIST_BOX_Y, LIST_BOX_WIDTH, LIST_BOX_HEIGHT);
            sprite.drawString(fileName, appState.selectedScrollPos, LIST_TEXT_START_Y + (i * LIST_LINE_HEIGHT));
            sprite.clearClipRect();
          } else {
            String displayName = fileName;
            if (displayName.length() > FILENAME_DISPLAY_MAX_LENGTH) {
              displayName = displayName.substring(0, FILENAME_DISPLAY_MAX_LENGTH);
            }
            sprite.drawString(displayName, LIST_TEXT_START_X, LIST_TEXT_START_Y + (i * LIST_LINE_HEIGHT));
          }
        }
      }
    int yos = 0;
    if (appState.currentSelectedIndex >= 3)
      for (int i = appState.currentSelectedIndex - 3; i < appState.currentSelectedIndex - 3 + 7; i++) {
        if (i < appState.fileCount) {
          if (i == appState.currentPlayingIndex) {
            sprite.setTextColor(RED, BLACK);
          } else if (i == appState.currentSelectedIndex) {
            sprite.setTextColor(WHITE, BLACK);
          } else {
            sprite.setTextColor(GREEN, BLACK);
          }
          String fileName = extractDisplayName(appState.audioFiles[i]);
          const lgfx::U8g2font* detectedFont = detectAndGetFont(fileName);
          if (detectedFont) {
            sprite.setFont(detectedFont);
          } else {
            sprite.setTextFont(0);
          }
          if (i == appState.currentSelectedIndex && (now - appState.selectedTime >= SELECTED_SCROLL_DELAY)) {
            if (appState.graphSpeed == 0) {
              appState.selectedScrollPos = appState.selectedScrollPos - SCROLL_STEP;
              int textWidth = fileName.length() * TEXT_WIDTH_ESTIMATE_PX;
              if (appState.selectedScrollPos + textWidth < TEXT_LEFT) {
                appState.selectedScrollPos = TEXT_RIGHT;
              }
              if (appState.selectedScrollPos > TEXT_RIGHT) {
                appState.selectedScrollPos = TEXT_RIGHT;
              }
            }
            sprite.setClipRect(LIST_BOX_X, LIST_BOX_Y, LIST_BOX_WIDTH, LIST_BOX_HEIGHT);
            sprite.drawString(fileName, appState.selectedScrollPos, LIST_TEXT_START_Y + (yos * LIST_LINE_HEIGHT));
            sprite.clearClipRect();
          } else {
            String displayName = fileName;
            if (displayName.length() > FILENAME_DISPLAY_MAX_LENGTH) {
              displayName = displayName.substring(0, FILENAME_DISPLAY_MAX_LENGTH);
            }
            sprite.drawString(displayName, 8, 10 + (yos * 16));
          }
        }
        yos++;
      }
    sprite.setTextFont(0);
    sprite.setTextColor(grays[1], gray);
    sprite.drawString("WINAMP", 150, 4);
    sprite.setTextColor(grays[2], gray);
    sprite.drawString("LIST", 58, 0);
    sprite.setTextColor(grays[4], gray);
    sprite.drawString("VOL", 150, 80);
    sprite.drawString("LIG", 150, 122);
    if (appState.isPlaying) {
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
    if (!appState.stopped) {
      if (now - appState.lastTimeUpdate >= TIME_UPDATE_INTERVAL) {
        appState.cachedTimeStr = rtc.getTime().substring(3, 8);
        appState.lastTimeUpdate = now;
      }
      sprite.drawString(appState.cachedTimeStr, 172, 18);
    }
    sprite.setTextFont(0);
    if (now - appState.lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL) {
      appState.batteryPercent = getBatteryPercent();
      appState.lastBatteryUpdate = now;
    }
    sprite.setTextDatum(3);
    // Optimized: use char buffer instead of String concatenation
    char batteryStr[8];
    snprintf(batteryStr, sizeof(batteryStr), "%d%%", appState.batteryPercent);
    sprite.drawString(batteryStr, 220, 121);
    sprite.setTextColor(BLACK, grays[4]);
    sprite.drawString("M", 220, 96);
    sprite.drawString("N", 198, 96);
    sprite.drawString("P", 176, 96);
    sprite.drawString("A", 154, 96);
    sprite.setTextColor(BLACK, grays[5]);
    sprite.drawString(">>", 202, 103);
    sprite.drawString("<<", 180, 103);
    sprite.setTextFont(0);
    sprite.setTextColor(GREEN, BLACK);
    sprite.setTextDatum(0);
    String modeText = "";
    if (appState.playMode == PlaybackMode::Sequential) {
      modeText = "SEQ";
    } else if (appState.playMode == PlaybackMode::Random) {
      modeText = "RND";
    } else if (appState.playMode == PlaybackMode::SingleRepeat) {
      modeText = "ONE";
    }
    sprite.drawString(modeText, 150, 63);
    if (appState.isPlaying && !appState.stopped && (now - appState.lastAudioInfoUpdate >= AUDIO_INFO_UPDATE_INTERVAL)) {
      uint32_t sampleRate = AudioManager::getSampleRate();
      uint8_t bitsPerSample = AudioManager::getBitsPerSample();
      if (sampleRate > 0 && bitsPerSample > 0) {
        // Optimized: use char buffer instead of String concatenation
        float sampleRateKHz = sampleRate / 1000.0f;
        char audioInfoStr[16];
        if (sampleRateKHz == (int)sampleRateKHz) {
          // Integer kHz, no decimal needed
          snprintf(audioInfoStr, sizeof(audioInfoStr), "%d/%d", (int)sampleRateKHz, bitsPerSample);
        } else {
          // Has decimal part, show 1 decimal place
          snprintf(audioInfoStr, sizeof(audioInfoStr), "%.1f/%d", sampleRateKHz, bitsPerSample);
        }
        appState.cachedAudioInfo = String(audioInfoStr);
        appState.lastAudioInfoUpdate = now;
      } else {
        if (appState.cachedAudioInfo.length() == 0) {
          appState.cachedAudioInfo = "";
        }
      }
    }
    if (appState.cachedAudioInfo.length() > 0) {
      sprite.setTextDatum(2);
      sprite.drawString(appState.cachedAudioInfo, 232, 63);
      sprite.setTextDatum(0);
    }
    if (appState.showDeleteDialog) {
      sprite.fillRect(20, 40, 200, 70, BLACK);
      sprite.drawRect(20, 40, 200, 70, WHITE);
      sprite.setTextFont(0);
      sprite.setTextColor(WHITE, BLACK);
      sprite.setTextDatum(0);
      sprite.drawString("Delete song?", 30, 45);
      if (appState.currentSelectedIndex < appState.fileCount) {
        String fileName = extractDisplayName(appState.audioFiles[appState.currentSelectedIndex]);
        if (fileName.length() > FILENAME_DISPLAY_MAX_LENGTH) {
          fileName = fileName.substring(0, FILENAME_DISPLAY_MAX_LENGTH);
        }
        const lgfx::U8g2font* detectedFont = detectAndGetFont(fileName);
        if (detectedFont) {
          sprite.setFont(detectedFont);
        } else {
          sprite.setTextFont(0);
        }
        sprite.drawString(fileName, 30, 57);
        sprite.setTextFont(0);
      }
      sprite.drawString("Y:Yes  C:Cancel", 30, 75);
    }
    sprite.pushSprite(0, 0);
  }
  appState.graphSpeed++;
  if (appState.graphSpeed == 4) appState.graphSpeed = 0;
}

}  // namespace UiRenderer


