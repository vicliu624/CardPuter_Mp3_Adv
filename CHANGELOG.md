# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.2.0] - 2025-01-17

### Added
- Enhanced ID3 information page with playback controls:
  - Current playback time display (MM:SS format, smaller font, centered in right area)
  - Minimalist progress bar (1-pixel high, green, at bottom of screen)
  - Control icons (Previous: |◀, Play/Pause: ▶/⏸, Next: ▶|) with rounded square backgrounds
  - Control icons positioned at bottom right, 4 pixels above progress bar
  - ContentType metadata display (below title, only if exists)
- Screenshots section in README.md with main view and ID3 page examples

### Enhanced
- ID3 page layout improvements:
  - Dynamic positioning for Artist, Title, and ContentType to prevent overlap
  - Artist name aligned with cover top edge
  - Title and ContentType positioned dynamically based on previous element
  - Album name scrolling with proper clipRect to show full text height
- Control icons design:
  - Rounded square backgrounds (19x19 pixels) with gray color
  - Icons drawn using drawing primitives for flexibility
  - Properly centered and spaced icons
  - Play/Pause icon toggles based on playback state

### Fixed
- Screenshot color conversion issue (removed redundant byte swap causing incorrect colors)
- Album name text clipping when scrolling (adjusted clipRect to include full font height)
- Text overlap issues on ID3 page (dynamic Y position calculation)
- Scroll trigger logic (only scrolls when text width exceeds available width)

## [2.1.0] - 2025-11-11

### Added
- Major code structure refactoring and modularization
- Created 8 new modules for better code organization:
  - `config.hpp`: Centralized configuration constants
  - `app_state.hpp`: Centralized application state structure
  - `input_handler.hpp/cpp`: Keyboard input handling
  - `ui_renderer.hpp/cpp`: UI rendering logic
  - `audio_manager.hpp/cpp`: Audio playback control
  - `board_init.hpp/cpp`: Hardware initialization
  - `file_manager.hpp/cpp`: File operations
  - `image_utils.hpp/cpp`: Image utilities
- Unified logging system with compile-time switches
- Performance optimizations (snprintf, memory management)

### Changed
- Reduced `M5mp3.cpp` from ~1800 lines to ~420 lines (76% reduction)
- Improved code maintainability and organization
- Better separation of concerns

### Fixed
- Type compatibility issues with M5Canvas and ESP32Time
- Memory optimization for screenshot capture
- String operation performance improvements

## [2.0.0] - 2025-11-07

### Added
- Multi-language character support (Chinese, Japanese, Korean)
- Screen power management (S key toggle)
- Song deletion feature with confirmation dialog
- Playback mode system (SEQ/RND/ONE)
- Fine volume control (-/= keys)
- Smart scrolling for selected songs
- Dual index system (selected vs playing)
- Screenshot feature (F key)
- ID3 information display page (I key)
- Sample rate and bit depth display

### Enhanced
- File organization (changed default directory to `/music`)
- User interface improvements
- List navigation (circular navigation)
- Performance optimizations (caching, throttling)
- Volume control enhancement

### Removed
- Top scrolling song title area
- B key random playback function

## [1.0.0] - 2025-11-06

### Added
- Initial release
- Basic MP3/WAV playback support
- File list navigation
- Volume and brightness control
- Battery and time display

