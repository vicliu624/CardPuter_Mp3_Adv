#pragma once

#include <cstddef>  // for size_t

// Configuration constants and placeholders
// Step 1: Extract constants and magic numbers from M5mp3.cpp

// Version information
#define FIRMWARE_VERSION_MAJOR 2
#define FIRMWARE_VERSION_MINOR 2
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION_STRING "2.2.0"

// Screen dimensions
constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 135;

// Hardware pins (Cardputer)
constexpr int CARDPUTER_I2C_SDA = 8;   // G8
constexpr int CARDPUTER_I2C_SCL = 9;   // G9
// Advanced (ES8311) I2S pins and controls
constexpr int CARDPUTER_ADV_I2S_BCLK = 41;
constexpr int CARDPUTER_ADV_I2S_LRCK = 43;
constexpr int CARDPUTER_ADV_I2S_DOUT = 42;
constexpr int CARDPUTER_ADV_HP_DET_PIN = 17;  // LOW when headphones inserted
constexpr int CARDPUTER_ADV_AMP_EN_PIN = 46;  // HIGH enables amplifier
// Standard (AW88298) I2S pins
constexpr int CARDPUTER_STD_I2S_BCLK = 41;
constexpr int CARDPUTER_STD_I2S_LRCK = 43;
constexpr int CARDPUTER_STD_I2S_DOUT = 42;

// I2C device addresses and clocks
constexpr uint8_t ES8311_ADDR = 0x18;
constexpr uint8_t AW88298_ADDR = 0x36;
constexpr uint8_t AW9523_ADDR  = 0x58;
constexpr uint8_t TCA8418_ADDR = 0x34;
constexpr uint32_t ES8311_I2C_FREQ = 400000UL;

// UI layout constants
constexpr int LIST_BOX_X = 4;
constexpr int LIST_BOX_Y = 8;
constexpr int LIST_BOX_WIDTH = 130;
constexpr int LIST_BOX_HEIGHT = 122;
constexpr int TEXT_LEFT = 8;
constexpr int TEXT_RIGHT = 128;

// ID3 page layout
constexpr int COVER_X = 8;
constexpr int COVER_Y = 8;
constexpr int COVER_WIDTH = 96;
constexpr int COVER_HEIGHT = 96;
// Album text Y position: ensure it's below the cover with enough space for font height
// COVER_Y + COVER_HEIGHT = 8 + 96 = 104 (bottom of cover)
// Add spacing (2px) + font height (16px) to ensure text baseline is below cover
constexpr int ALBUM_TEXT_Y = COVER_Y + COVER_HEIGHT + 2 + 16;  // 106 + 16 = 122
constexpr int ALBUM_TEXT_HEIGHT = 16;
constexpr int ARTIST_X = 120;
// Artist Y position: align text top with cover top
// COVER_Y = 8, font height typically 16px, so baseline should be at COVER_Y + font_height
constexpr int ARTIST_Y = COVER_Y + 16;  // 8 + 16 = 24 (text top aligns with cover top at Y=8)
constexpr int TITLE_X = 120;
constexpr int TITLE_Y = 26;
constexpr int CONTENT_TYPE_X = 120;
constexpr int CONTENT_TYPE_Y = 42;
constexpr int ID3_ICON_SIZE = 19;  // Font size (16) + 3 pixels
constexpr int ID3_ICON_GAP = 8;  // Space between icons
constexpr int ID3_ICON_ROUND_RADIUS = 3;  // Round corner radius for icons
constexpr int PROGRESS_BAR_Y = SCREEN_HEIGHT - 1;  // Bottom of screen
constexpr int PROGRESS_BAR_HEIGHT = 1;
constexpr int ID3_ICONS_Y = PROGRESS_BAR_Y - PROGRESS_BAR_HEIGHT - 4 - ID3_ICON_SIZE;  // 4 pixels above progress bar
constexpr int ID3_TIME_FONT_HEIGHT = 8;  // Default font height (smaller than DSEG7)
constexpr int ID3_TIME_Y = ID3_ICONS_Y - 2 - ID3_TIME_FONT_HEIGHT;  // 2 pixels above icons
// Time position: centered in right area (120-240, width=120)
// Time string "MM:SS" width ~40px with default font, centered: X = 120 + (120-40)/2 = 200
constexpr int ID3_TIME_X = 200;  // Centered in right area
// Calculate icon positions: centered in right area (120-240, width=120)
// Three icons: 19*3 + 8*2 = 73 pixels total, centered: start at 120 + (120-73)/2 = 144
constexpr int ID3_ICON_PREV_X = 144;
constexpr int ID3_ICON_PLAY_X = ID3_ICON_PREV_X + ID3_ICON_SIZE + ID3_ICON_GAP;  // 171
constexpr int ID3_ICON_NEXT_X = ID3_ICON_PLAY_X + ID3_ICON_SIZE + ID3_ICON_GAP;  // 198

// Playback mode display
constexpr int MODE_X = 150;
constexpr int MODE_Y = 63;

// File limits
constexpr int MAX_FILES = 100;

// Cover image scanning
constexpr size_t COVER_SCAN_MAX = 4096;  // 4KB scan limit
constexpr size_t JPEG_SCAN_MAX = 4096;
constexpr size_t COVER_HEADER_BUF_SIZE = 32;

// Timing intervals (ms)
constexpr unsigned long BATTERY_UPDATE_INTERVAL = 30000;
constexpr unsigned long TIME_UPDATE_INTERVAL = 1000;
constexpr unsigned long GRAPH_UPDATE_INTERVAL = 200;
constexpr unsigned long AUDIO_INFO_UPDATE_INTERVAL = 500;
constexpr unsigned long SELECTED_SCROLL_DELAY = 1000;
constexpr unsigned long ID3_SCROLL_DELAY = 1000;

// Placeholder texts
constexpr const char* PLACEHOLDER_UNKNOWN_ARTIST = "Unknown Artist";
constexpr const char* PLACEHOLDER_UNKNOWN_ALBUM = "Unknown Album";
constexpr const char* PLACEHOLDER_NO_COVER = "No Cover";

// SD card paths
constexpr const char* MUSIC_DIR = "/music";
constexpr const char* SCREEN_DIR = "/screen";

// Volume and brightness
constexpr int VOLUME_MIN = 0;
constexpr int VOLUME_MAX = 21;
constexpr int BRIGHTNESS_LEVELS = 5;
constexpr int BRIGHTNESS_VALUES[BRIGHTNESS_LEVELS] = {60, 120, 180, 220, 255};

// Playback modes
enum class PlaybackMode {
  Sequential = 0,
  Random = 1,
  SingleRepeat = 2
};

// Logging system (compile-time switch)
// Set to 0 to disable all logging for production builds
#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING 1
#endif

// Debug logging (can be disabled separately from general logging)
#ifndef ENABLE_DEBUG_LOGS
#define ENABLE_DEBUG_LOGS 0  // Set to 1 to enable DEBUG logs
#endif

#if ENABLE_DEBUG_LOGS && ENABLE_LOGGING
#define DEBUG_PRINTF(fmt, ...) LOG_PRINTF(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(fmt, ...) ((void)0)
#endif

#if ENABLE_LOGGING
#define LOG_INIT(baud) Serial.begin(baud)
#define LOG_PRINT(x) Serial.print(x)
#define LOG_PRINTLN(x) Serial.println(x)
#define LOG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG_INIT(baud) ((void)0)
#define LOG_PRINT(x) ((void)0)
#define LOG_PRINTLN(x) ((void)0)
#define LOG_PRINTF(fmt, ...) ((void)0)
#endif

// UI magic numbers (extracted from ui_renderer.cpp and M5mp3.cpp)
// Scrollbar
constexpr int SCROLLBAR_X = 129;
constexpr int SCROLLBAR_Y = 8;
constexpr int SCROLLBAR_WIDTH = 5;
constexpr int SCROLLBAR_HEIGHT = 122;
constexpr int SCROLLBAR_COLOR = 0x0841;
constexpr int SCROLLBAR_THUMB_HEIGHT = 20;
constexpr int SCROLLBAR_THUMB_INDICATOR_WIDTH = 1;
constexpr int SCROLLBAR_THUMB_INDICATOR_HEIGHT = 12;
constexpr int SCROLLBAR_THUMB_INDICATOR_OFFSET = 4;

// Scroll position
constexpr int SCROLL_INITIAL_POS = 8;
constexpr int SCROLL_STEP = 2;  // Pixels per scroll step

// Status bar decorations
constexpr int STATUS_BAR_ORANGE_LINE1_X = 4;
constexpr int STATUS_BAR_ORANGE_LINE1_Y = 2;
constexpr int STATUS_BAR_ORANGE_LINE1_WIDTH = 50;
constexpr int STATUS_BAR_ORANGE_LINE2_X = 84;
constexpr int STATUS_BAR_ORANGE_LINE3_X = 190;
constexpr int STATUS_BAR_ORANGE_LINE3_WIDTH = 45;
constexpr int STATUS_BAR_GRAY_LINE_Y = 6;
constexpr int STATUS_BAR_GRAY_LINE_HEIGHT = 3;

// Control panel layout
constexpr int CONTROL_PANEL_X = 139;
constexpr int CONTROL_PANEL_WIDTH = 3;
constexpr int CONTROL_PANEL_SEPARATOR_X = 143;
constexpr int CONTROL_PANEL_RIGHT_EDGE = 238;
constexpr int CONTROL_PANEL_LEFT_EDGE = 138;
constexpr int CONTROL_PANEL_INFO_X = 148;
constexpr int CONTROL_PANEL_INFO_Y = 14;
constexpr int CONTROL_PANEL_INFO_WIDTH = 86;
constexpr int CONTROL_PANEL_INFO_HEIGHT = 42;
constexpr int CONTROL_PANEL_MODE_Y = 59;
constexpr int CONTROL_PANEL_MODE_HEIGHT = 16;

// Play/Stop indicator
constexpr int PLAY_TRIANGLE_X1 = 162;
constexpr int PLAY_TRIANGLE_Y1 = 18;
constexpr int PLAY_TRIANGLE_Y2 = 26;
constexpr int PLAY_TRIANGLE_X2 = 168;
constexpr int PLAY_TRIANGLE_Y_CENTER = 22;
constexpr int STOP_RECT_X = 162;
constexpr int STOP_RECT_Y = 30;
constexpr int STOP_RECT_SIZE = 6;

// Control buttons
constexpr int BUTTON_BASE_X = 148;
constexpr int BUTTON_BASE_Y = 94;
constexpr int BUTTON_WIDTH = 18;
constexpr int BUTTON_HEIGHT = 18;
constexpr int BUTTON_SPACING = 22;
constexpr int BUTTON_COUNT = 4;
constexpr int BUTTON_ROUND_RADIUS = 3;

// Button icons
constexpr int BUTTON_ICON_PREV_X = 220;
constexpr int BUTTON_ICON_PREV_Y1 = 104;
constexpr int BUTTON_ICON_PREV_Y2 = 108;
constexpr int BUTTON_ICON_PREV_WIDTH = 8;
constexpr int BUTTON_ICON_PREV_HEIGHT = 2;
constexpr int BUTTON_ICON_NEXT_X = 228;
constexpr int BUTTON_ICON_NEXT_Y1 = 102;
constexpr int BUTTON_ICON_NEXT_Y2 = 106;
constexpr int BUTTON_ICON_NEXT_X2 = 231;
constexpr int BUTTON_ICON_NEXT_Y_CENTER = 105;
constexpr int BUTTON_ICON_PLAY_X1 = 152;
constexpr int BUTTON_ICON_PLAY_X2 = 157;
constexpr int BUTTON_ICON_PLAY_Y = 104;
constexpr int BUTTON_ICON_PLAY_WIDTH = 3;
constexpr int BUTTON_ICON_PLAY_HEIGHT = 6;
constexpr int BUTTON_ICON_STOP_X1 = 156;
constexpr int BUTTON_ICON_STOP_Y1 = 102;
constexpr int BUTTON_ICON_STOP_Y2 = 110;
constexpr int BUTTON_ICON_STOP_X2 = 160;
constexpr int BUTTON_ICON_STOP_Y_CENTER = 106;

// Volume control
constexpr int VOLUME_BAR_X = 172;
constexpr int VOLUME_BAR_Y = 82;
constexpr int VOLUME_BAR_WIDTH = 60;
constexpr int VOLUME_BAR_HEIGHT = 3;
constexpr int VOLUME_BAR_START_X = 155;
constexpr int VOLUME_BAR_RANGE = 60;  // Pixel range for volume (0-21 maps to this range)
constexpr int VOLUME_SLIDER_WIDTH = 10;
constexpr int VOLUME_SLIDER_HEIGHT = 8;
constexpr int VOLUME_SLIDER_Y = 80;
constexpr int VOLUME_SLIDER_INNER_X_OFFSET = 2;
constexpr int VOLUME_SLIDER_INNER_Y_OFFSET = 2;
constexpr int VOLUME_SLIDER_INNER_WIDTH = 6;
constexpr int VOLUME_SLIDER_INNER_HEIGHT = 4;

// Brightness control
constexpr int BRIGHTNESS_BAR_X = 172;
constexpr int BRIGHTNESS_BAR_Y = 124;
constexpr int BRIGHTNESS_BAR_WIDTH = 30;
constexpr int BRIGHTNESS_BAR_HEIGHT = 3;
constexpr int BRIGHTNESS_SLIDER_WIDTH = 10;
constexpr int BRIGHTNESS_SLIDER_HEIGHT = 8;
constexpr int BRIGHTNESS_SLIDER_Y = 122;
constexpr int BRIGHTNESS_SLIDER_STEP = 5;  // Pixels per brightness level
constexpr int BRIGHTNESS_SLIDER_INNER_X_OFFSET = 2;
constexpr int BRIGHTNESS_SLIDER_INNER_Y_OFFSET = 2;
constexpr int BRIGHTNESS_SLIDER_INNER_WIDTH = 6;
constexpr int BRIGHTNESS_SLIDER_INNER_HEIGHT = 4;

// Battery indicator
constexpr int BATTERY_X = 206;
constexpr int BATTERY_Y = 119;
constexpr int BATTERY_WIDTH = 28;
constexpr int BATTERY_HEIGHT = 12;
constexpr int BATTERY_TERMINAL_X = 234;
constexpr int BATTERY_TERMINAL_Y = 122;
constexpr int BATTERY_TERMINAL_WIDTH = 3;
constexpr int BATTERY_TERMINAL_HEIGHT = 6;

// Spectrum graph
constexpr int GRAPH_BASE_X = 172;
constexpr int GRAPH_BASE_Y = 50;
constexpr int GRAPH_BAR_COUNT = 14;
constexpr int GRAPH_BAR_SPACING = 4;
constexpr int GRAPH_BAR_WIDTH = 3;
constexpr int GRAPH_BAR_HEIGHT = 2;
constexpr int GRAPH_BAR_HEIGHT_STEP = 3;  // Vertical spacing between bars
constexpr int GRAPH_BAR_MAX = 5;  // Maximum bar height

// List display
constexpr int LIST_VISIBLE_LINES = 7;
constexpr int LIST_LINE_HEIGHT = 16;
constexpr int LIST_TEXT_START_X = 8;
constexpr int LIST_TEXT_START_Y = 10;
constexpr int LIST_SCROLL_THRESHOLD = 3;  // Start scrolling when selected index >= this
constexpr int FILENAME_DISPLAY_MAX_LENGTH = 20;  // Maximum characters to display for filename
constexpr int TEXT_WIDTH_ESTIMATE_PX = 5;  // Estimated pixels per character for text width calculation

// Border lines
constexpr int BORDER_LEFT_X = 3;
constexpr int BORDER_LEFT_Y = 9;
constexpr int BORDER_LEFT_HEIGHT = 120;
constexpr int BORDER_RIGHT_X = 134;
constexpr int BORDER_BOTTOM_X = 3;
constexpr int BORDER_BOTTOM_Y = 129;
constexpr int BORDER_BOTTOM_WIDTH = 130;
constexpr int BORDER_TOP_Y = 0;
constexpr int BORDER_TOP_WIDTH = 240;
constexpr int BORDER_BOTTOM_EDGE_Y = 134;

// Grayscale palette generation
constexpr int GRAYS_COUNT = 18;
constexpr int GRAYS_START_COLOR = 214;  // Starting RGB value for grayscale palette
constexpr int GRAYS_STEP = 13;          // Decrement step between grayscale levels
constexpr int GRAYS_BLUE_OFFSET = 40;   // Blue channel offset for color565

