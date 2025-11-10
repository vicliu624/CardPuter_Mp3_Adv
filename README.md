# CardPuter MP3 Player

An advanced MP3 player based on M5Cardputer, featuring Chinese character display, multiple playback modes, and rich control functions.

## Features

### Audio Playback
- Supports MP3 and WAV formats
- Automatically reads audio files from `/music` directory (falls back to root directory if not found)
- Supports up to 100 songs
- Three playback modes: Sequential, Random, Single Repeat

### Multi-Language Character Support
- Full UTF-8 encoding support for displaying Chinese, Japanese, and Korean song names
- Automatic language detection and font selection:
  - **Korean**: Uses `efontKR_12` font (detects Hangul syllables U+AC00-U+D7AF)
  - **Japanese**: Uses `efontJA_12` font (detects Hiragana U+3040-U+309F, Katakana U+30A0-U+30FF, and Kanji)
  - **Chinese**: Uses `efontCN_12` font (detects CJK Unified Ideographs U+4E00-U+9FFF)
  - **English/Others**: Uses default system font
- Font selection priority: Korean > Japanese > Chinese > Default
- Song list automatically adapts to multi-language display (16-pixel line height)

### User Interface
- **Left LIST Area**: Displays song list (up to 7 lines)
  - Red: Currently playing song
  - White: Currently selected song
  - Green: Other songs
  - Selected song name scrolls from right to left after staying selected for more than 1 second
- **Right WINAMP Area**: Displays playback status, volume, brightness, battery, etc.
- Playback mode display: Shows current mode (SEQ/RND/ONE) above volume display

### Smart Scrolling
- Selected song name automatically scrolls to display full filename
- Scrolling range strictly limited within LIST box, won't exceed or overlap scrollbar
- File names automatically remove path and extension for cleaner interface

## Key Controls

### Playback Control
- **A** - Play/Pause toggle
- **N** - Next song
- **P** - Previous song
- **ENTER** - Play currently selected song

### Volume Control
- **V** - Cycle volume levels (step 5, range 0-21)
- **-** - Decrease volume (step 1)
- **=** - Increase volume (step 1)

### List Navigation
- **;** - Navigate up (circular, jumps to last song when at first)
- **.** - Navigate down (circular, jumps to first song when at last)

### Playback Mode
- **M** - Toggle playback mode
  - SEQ: Sequential playback
  - RND: Random playback
  - ONE: Single repeat

### Screen Control
- **L** - Cycle screen brightness (5 levels)
- **S** - Screen off/on toggle (saves brightness when off, restores when on)

### File Management
- **D** - Show delete confirmation dialog
  - **Y** - Confirm delete currently selected song
  - **C** - Cancel delete

## Technical Features

### Performance Optimization
- Battery display caching (updates every 30 seconds)
- Time display caching (updates every 1 second)
- Spectrum graph update throttling (200ms interval)
- Text scrolling optimization (updates every 4 frames)
- Screen refresh rate optimization (20fps, 50ms delay)

### Playback Logic
- Smart index management: Distinguishes between selected index and playing index
- Automatic index adjustment when deleting songs, doesn't affect current playback
- Automatically switches to next song when deleting currently playing song
- Continues playing current song when deleting non-playing songs

### Hardware Support
- Supports M5Cardputer Standard and Advanced versions
- Automatically detects hardware version and configures appropriate audio driver
- Supports headphone detection with automatic amplifier state switching
- Uses ESP32-audioI2S library (version 2.0.0)

## File Structure

```
/music/          # Recommended music file directory
  ├── song1.mp3
  ├── song2.wav
  └── ...
```

## Dependencies

- M5Cardputer (^1.0.3)
- ESP32-audioI2S (2.0.0) - https://github.com/schreibfaul1/ESP32-audioI2S.git
- ESP32Time (^2.0.6)
- FastLED (^3.3.3)
- Adafruit NeoPixel (^1.10.6)

## Build Instructions

Use PlatformIO to compile and flash.

## Changelog

### Version 2.0 - Major Feature Additions and Improvements

#### New Features

1. **Multi-Language Character Support**
   - Added UTF-8 encoding support for displaying Chinese, Japanese, and Korean song names
   - Implemented automatic language detection with font selection:
     - Korean: `efontKR_12` (Hangul syllables)
     - Japanese: `efontJA_12` (Hiragana, Katakana, Kanji)
     - Chinese: `efontCN_12` (CJK Unified Ideographs)
     - English/Others: Default system font
   - Font selection priority: Korean > Japanese > Chinese > Default
   - Adjusted line height from 12 to 16 pixels to accommodate CJK characters

2. **Screen Power Management**
   - Added screen off/on toggle functionality (S key)
   - Automatically saves current brightness when turning off screen
   - Restores saved brightness when turning screen back on
   - Skips drawing operations when screen is off to save CPU

3. **Song Deletion Feature**
   - Added delete confirmation dialog (D key to show, Y to confirm, C to cancel)
   - Smart deletion logic: continues playing current song if deleted song is not playing
   - Automatically switches to next song if currently playing song is deleted
   - Properly adjusts all indices after deletion

4. **Playback Mode System**
   - Added three playback modes: Sequential (SEQ), Random (RND), Single Repeat (ONE)
   - Mode toggle via M key (replaces original B key random function)
   - Mode indicator displayed in top-right area (replaces scrolling song title)
   - Automatic next song selection based on selected mode
   - Smart N/P key behavior: In random mode, both N and P keys select random songs (avoiding current playing song)

5. **Fine Volume Control**
   - Added `-` key for decreasing volume (step 1)
   - Added `=` key for increasing volume (step 1)
   - Improved volume bar calculation for smooth linear mapping (0-21 range mapped to 60 pixels)
   - Original V key still works for step-based volume cycling (step 5)

6. **Smart Scrolling for Selected Songs**
   - Selected song names scroll from right to left after 1 second delay
   - Scrolling strictly limited within LIST box boundaries (doesn't overlap scrollbar)
   - Automatically resets scroll position when selection changes
   - Optimized update rate (every 4 frames) to reduce CPU usage

7. **Dual Index System**
   - Separated selected index (`n`) from playing index (`currentPlayingIndex`)
   - Enables independent navigation and playback tracking
   - Properly handles index adjustments during song deletion

#### Enhanced Features

1. **File Organization**
   - Changed default music directory from `/mp3s` to `/music`
   - File names displayed without path and extension for cleaner interface
   - Improved file path extraction and display logic

2. **User Interface Improvements**
   - Reduced song list display from 10 lines to 7 lines (better fit with 16px line height)
   - Added color coding: Red for playing song, White for selected song, Green for others
   - Removed top scrolling area (previously showed current playing song title)
   - Replaced with playback mode indicator (SEQ/RND/ONE)

3. **List Navigation**
   - Made list navigation circular: pressing up at first song jumps to last, pressing down at last jumps to first
   - Improved navigation logic for better user experience

4. **Performance Optimizations**
   - Battery display caching: updates every 30 seconds (was real-time)
   - Time display caching: updates every 1 second (was real-time)
   - Spectrum graph throttling: updates every 200ms (was every frame)
   - Text scrolling optimization: updates every 4 frames
   - Screen refresh rate: optimized to 20fps (50ms delay, was 25fps/40ms)
   - Main loop delay: increased to 200ms (was 100ms) to reduce CPU usage

5. **Volume Control Enhancement**
   - Improved volume bar calculation: linear mapping `155 + (volume * 60 / 21)` instead of step-based `(volume / 5) * 17`
   - Ensures smooth movement for each volume unit change

6. **Code Quality**
   - All comments translated to English
   - Improved code organization and structure
   - Better variable naming and documentation

#### Removed Features

- Removed top scrolling song title area (replaced with playback mode display)
- Removed B key random playback function (replaced with M key mode toggle)

#### Technical Changes

- Added multiple global state variables for new features (screenOff, showDeleteDialog, playMode, etc.)
- Implemented caching mechanisms for battery, time, and graph updates
- Added scroll position tracking for selected songs
- Enhanced `deleteCurrentFile()` function with smart index management
- Modified `audio_eof_mp3()` to support different playback modes
- Updated `draw()` function with Chinese font support and improved rendering logic

## Notes

- Ensure SD card is formatted as FAT32
- Audio files are recommended to be placed in `/music` directory
- Deleting songs permanently removes files from SD card, use with caution
- In single repeat mode, the song automatically repeats after finishing
