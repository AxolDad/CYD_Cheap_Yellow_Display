# Flappy Kiernan

An engaging Flappy Bird-like game designed specifically for the ESP32-2432S028R "Cheap Yellow Display" (CYD) board. This game features smooth animations, responsive touch controls, and persistent high scores.

![Flappy Kiernan Game](https://github.com/yourusername/flappy-kiernan/raw/main/images/game_screenshot.jpg)

## Overview

Flappy Kiernan is an arcade-style game where players control a character (Kiernan) through a series of pipes by tapping on the touchscreen. The game features:

- Flicker-free animations using hardware-accelerated sprite rendering
- Responsive touchscreen controls
- Persistent high score tracking using SPIFFS
- Dynamic difficulty adjustment
- Smooth countdown and game over transitions

## Hardware Requirements

- ESP32-2432S028R "Cheap Yellow Display" (CYD) Board
- USB Cable for programming and power

## Libraries Used

This project relies on the following libraries:

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) - High-performance graphics library providing flicker-free rendering through hardware acceleration and double buffering
- [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) - Library for interfacing with the XPT2046 touchscreen controller
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32) - Arduino core for the ESP32
- [SPIFFS](https://github.com/espressif/arduino-esp32/tree/master/libraries/SPIFFS) - SPI Flash File System for storing high scores

## Installation

1. Install the required libraries:
   - LovyanGFX: Install through Arduino Library Manager or clone from GitHub
   - XPT2046_Touchscreen: Install through Arduino Library Manager or clone from GitHub
   - ESP32 Arduino Core: Follow [these instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)

2. Connect your ESP32-2432S028R board to your computer

3. Select the appropriate board in Arduino IDE: "ESP32 Dev Module"

4. Compile and upload the sketch to your device

## Project Structure

The code is organized into three main sections:

1. **Main Definitions, Setup, and Game State**
   - Display configuration
   - Constants definitions
   - Game data structures
   - Setup and main loop

2. **Game Mechanics and Player Sprite**
   - Player sprite data
   - Game mechanics functions
   - Touch handling
   - High score saving

3. **Rendering Functions**
   - Menu screen rendering
   - Countdown animation
   - Main game rendering
   - Game over screen

## Pin Configuration for ESP32-2432S028R (CYD)

### Display (ILI9341)
- SCLK: GPIO14
- MOSI: GPIO13
- MISO: GPIO12
- DC: GPIO2
- CS: GPIO15
- Backlight: GPIO21

### Touchscreen (XPT2046)
- T_IRQ: GPIO36
- T_DIN: GPIO32
- T_OUT: GPIO39
- T_CLK: GPIO25
- T_CS: GPIO33

## Credits and Acknowledgments

- Original concept inspired by Flappy Bird by Dong Nguyen
- Display and touch handling techniques adapted from [Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/)
- Optimization strategies inspired by the [ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) project by Brian Lough
- Special thanks to the authors of LovyanGFX for providing the double-buffering capabilities that make flicker-free animation possible

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Future Improvements

- Add sound effects using the onboard buzzer
- Implement difficulty levels
- Add more visual elements and animations
- Create custom sprites for pipes and background elements

## Troubleshooting

- **Display shows nothing**: Ensure the backlight pin (GPIO21) is properly controlled. The backlight is active when the pin is pulled LOW.
- **Touch not working**: Check the touch SPI pins and make sure the touch controller is initialized before the display.
- **Game performance issues**: Reduce the sprite color depth or size to improve rendering performance.

## Contributing

Contributions to improve Flappy Kiernan are welcome! Please fork the repository and submit a pull request.
