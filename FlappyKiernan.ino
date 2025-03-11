/*
 * Flappy Kiernan - A Flappy Bird-like game for ESP32-2432S028R (CYD)
 * Using LovyanGFX library for flicker-free rendering
 * 
 * This implementation uses hardware-accelerated sprite rendering and double buffering
 * to provide a completely flicker-free gaming experience.
 * 
 * PART 1 - Main definitions, setup and game state
 */

#define LGFX_USE_V1             // LovyanGFX version 1.x
#include <LovyanGFX.hpp>
#include <SPIFFS.h>
#include <XPT2046_Touchscreen.h>

// ============= 1. DISPLAY CONFIGURATION =============
class LGFX_CYD : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX_CYD() {
    { // Set up the bus
      auto cfg = _bus_instance.config();
      
      // Use the pins that worked in your previous version
      cfg.spi_host = HSPI_HOST;  // HSPI bus for display
      cfg.spi_mode = 0;
      cfg.freq_write = 27000000; // 27MHz - more reliable speed
      cfg.freq_read = 1600000;  // 16MHz
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      
      // Correct display SPI pins for CYD
      cfg.pin_sclk = 14;  // SCLK
      cfg.pin_mosi = 13;  // MOSI
      cfg.pin_miso = 12;  // MISO
      cfg.pin_dc = 2;     // Data/Command
      
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    
    { // Set up the panel
      auto cfg = _panel_instance.config();
      
      cfg.pin_cs = 15;     // CS
      cfg.pin_rst = -1;    // RST, -1 if not used
      cfg.pin_busy = -1;   // BUSY, -1 if not used
      
      // Important: For the CYD, width and height might need to be swapped
      // based on orientation
      cfg.memory_width = 240;   // ILI9341 native width
      cfg.memory_height = 320;  // ILI9341 native height
      cfg.panel_width = 240;    
      cfg.panel_height = 320;
      
      // These may need adjustment if display orientation is wrong
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      
      _panel_instance.config(cfg);
    }
    
    { // Set up backlight
      auto cfg = _light_instance.config();
      cfg.pin_bl = 21;     // Backlight pin
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    
    setPanel(&_panel_instance);
  }
};

// ============= 2. DEFINE CONSTANTS =============
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Game physics constants
#define GRAVITY 0.15         // Gentle gravity for better gameplay
#define JUMP_FORCE -2.7    // Jump force 
#define PIPE_WIDTH 40
#define PIPE_GAP 95
#define PIPE_SPEED 1
#define MAX_PIPES 3
#define PLAYER_WIDTH 30     // Player sprite size
#define PLAYER_HEIGHT 30
#define GROUND_HEIGHT 10

// Touchscreen pins for ESP32-2432S028R
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

// Screen refresh optimization
#define FRAME_RATE 60    // Higher frame rate for smooth gameplay

// Define colors
// Not accurate due to a switch from 16B Color to 8B for Memory Savings
#define SKY_COLOR 0x5DFF
#define PIPE_COLOR 0x07E0
#define GROUND_COLOR 0x8260
#define TEXT_COLOR 0xFFFF
#define SCORE_COLOR 0xFFE0
#define GOLD_COLOR 0xFEA0
#define DARK_GREY 0x7BEF
#define TFT_LIGHTGREEN 0x9FE0  // A lighter green color
#define PINK_COLOR 0xF81F  //not accurate
#define BLACK_COLOR 0x0000
#define CHARACTER_COLOR 0xF1

// ============= 3. DEFINE ENUMS AND STRUCTS =============
// Game state enum
enum GameState {
  MENU,
  COUNTDOWN,
  PLAYING,
  GAME_OVER
};

// Pipe structure
struct Pipe {
  int x;
  int gapY;
  bool counted;
  float speed; // New variable to store pipe-specific speed
};
// Player structure
struct Player {
  float x;
  float y;
  float velocity;
};

// ============= 4. GLOBAL VARIABLES =============
// Display & sprite instances
LGFX_CYD tft;
lgfx::LGFX_Sprite screenSprite(&tft);  // Main buffer for drawing
lgfx::LGFX_Sprite playerSprite(&screenSprite); // Player sprite

// Game state variables
GameState gameState = MENU;
Player player;
Pipe pipes[MAX_PIPES];
unsigned long lastFrameTime;
int score = 0;
int highScore = 0;
bool touchActive = false;
bool prevTouchActive = false;

// Touchscreen variables
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
int touchX, touchY, touchZ; // Touch coordinates

// Countdown tracking
unsigned long countdownStartTime = 0;

// ============= 5. FUNCTION PROTOTYPES =============
void resetGame();
void handleTouch();
void updateGame(float deltaTime);
void renderMenu();
void renderCountdown();
void renderGame();
void renderGameOver();
void saveHighScore();

// Player sprite data
// Include in Part 2
extern const uint16_t kiernan_sprite[]; 

// ============= 6. SETUP FUNCTION =============
void setup() {
  Serial.begin(115200);
  Serial.println("Flappy Kiernan Starting with LovyanGFX...");

  // Display initialization
  tft.init();
  delay(120);
  tft.setRotation(1); // Landscape mode
  tft.setBrightness(255);
  Serial.println("Display initialized successfully.");

  // Check available heap memory
  Serial.print("Free heap memory: ");
  Serial.println(ESP.getFreeHeap());

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed! Proceeding without saved data.");
  } else {
    Serial.println("SPIFFS initialized successfully.");

    // Load high score from SPIFFS
    if (SPIFFS.exists("/highscore.dat")) {
      File f = SPIFFS.open("/highscore.dat", "r");
      if (f && f.size() >= sizeof(highScore)) {
        f.read((uint8_t*)&highScore, sizeof(highScore));
        //resetHighScore();
        Serial.print("Loaded high score: ");
        Serial.println(highScore);
        f.close();
      }
    }
  }
    // Create and verify sprites with reduced color depth
    screenSprite.setColorDepth(8); // <-- Add this line here!
    if (!screenSprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT)) {
      Serial.println("Failed to create main screen sprite even at 8-bit color!");
      while (true); // Halt execution for debugging
}     else {
  Serial.println("Main screen sprite created successfully with 8-bit color depth.");
}

  if (!playerSprite.createSprite(PLAYER_WIDTH, PLAYER_HEIGHT)) {
    Serial.println("Failed to create player sprite!");
    while (true); // halt execution
  }

  // Load player sprite data
  playerSprite.pushImage(0, 0, PLAYER_WIDTH, PLAYER_HEIGHT, kiernan_sprite);

  // Touchscreen initialization
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);
  Serial.println("Touchscreen initialized.");

  // Seed random number generator
  randomSeed(analogRead(34));

  // Game state initialization
  resetGame();
  lastFrameTime = millis();

  // Display splash screen briefly to test sprites
  screenSprite.fillScreen(TFT_BLUE);
  screenSprite.setTextColor(TFT_YELLOW);
  screenSprite.setTextSize(3);
  screenSprite.setCursor(40, 40);
  screenSprite.print("FLAPPY KIERNAN");

  // Draw Kiernan sprite at the center
  screenSprite.pushImage(SCREEN_WIDTH/2 - PLAYER_WIDTH/2, 90, PLAYER_WIDTH, PLAYER_HEIGHT, kiernan_sprite);

  screenSprite.setTextColor(TFT_GREEN);
  screenSprite.setTextSize(2);
  screenSprite.setCursor(40, 200);
  screenSprite.print("LOADING...");
  screenSprite.pushSprite(0, 0);
  delay(1500);

  Serial.println("Setup complete!");
}

// ============= 7. MAIN LOOP =============
void loop() {
  // Use a consistent frame timing approach
  static unsigned long targetTime = 0;
  static unsigned long lastFrameStart = 0;
  unsigned long currentTime = millis();
  
  // Only process a new frame when the target frame time has elapsed
  if (currentTime >= targetTime) {
    // Calculate frame time for physics
    float deltaTime = (currentTime - lastFrameStart) / 1000.0;
    lastFrameStart = currentTime;
    
    // Cap deltaTime to prevent large jumps
    if (deltaTime > 0.1) deltaTime = 0.1;
    
    // Handle touch input
    handleTouch();
    
    // Update game state before rendering to avoid tearing
    switch (gameState) {
      case MENU:
        // Just prepare for rendering
        break;
      case COUNTDOWN:
        // Check if countdown is complete
        if (millis() - countdownStartTime >= 3000) {
          gameState = PLAYING;
          Serial.println("Countdown complete, starting game!");
        }
        break;
      case PLAYING:
        updateGame(deltaTime);
        break;
      case GAME_OVER:
        // Just prepare for rendering
        break;
    }
    
    // Clear sprite buffer once for each frame
    screenSprite.fillScreen(gameState == MENU ? tft.color565(0, 0, 60) : 
                        (gameState == GAME_OVER ? TFT_BLACK : SKY_COLOR));
    
    // Render current game state
    switch (gameState) {
      case MENU:
        renderMenu();
        break;
      case COUNTDOWN:
        renderCountdown();
        break;
      case PLAYING:
        renderGame();
        break;
      case GAME_OVER:
        renderGameOver();
        break;
    }
    
    // Push sprite to display (flicker-free update)
    screenSprite.pushSprite(0, 0);
    
    // Set next frame time with consistent interval
    targetTime = currentTime + (1000 / FRAME_RATE);
  } else {
    // Yield to allow ESP32 background tasks to run when waiting for next frame
    yield();
  }
}
/*
 * Flappy Kiernan - Part 2 
 * Game mechanics and player sprite
 * 
 * This file contains:
 * - Sprite data for the player character
 * - Game mechanics functions (resetGame, updateGame)
 * - Touch handling function
 * - High score saving function
 */

// Define Kiernan sprite (Kirby-like character, 30x30)
const uint16_t kiernan_sprite[] PROGMEM = {
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR,
  SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, 0xFFFF, 0xFFFF, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, 0xFFFF, 0xFFFF, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, 0xFFFF, 0xFFFF, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, 0xFFFF, 0xFFFF, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, 0xE124, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, 0xE124, 0xE124, 0xE124, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, 0xE124, 0xE124, 0xE124, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, 0xE124, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR,
  SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, 0xF800, 0xF800, BLACK_COLOR, BLACK_COLOR, 0xF800, 0xF800, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, BLACK_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, CHARACTER_COLOR, BLACK_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, BLACK_COLOR, BLACK_COLOR, BLACK_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR, SKY_COLOR,
  };


// ============= 2. GAME MECHANICS FUNCTIONS =============
// Function to save high score to SPIFFS
// Function to reset high score
void resetHighScore() {
  highScore = 0; // Reset the value in memory
  
  // Delete the highscore file from SPIFFS
  if (SPIFFS.exists("/highscore.dat")) {
    SPIFFS.remove("/highscore.dat");
    Serial.println("High score file deleted!");
  }
  
  // Create a new file with zero value
  File f = SPIFFS.open("/highscore.dat", "w");
  if (f) {
    f.write((uint8_t*)&highScore, sizeof(highScore));
    f.close();
    Serial.println("High score reset to zero!");
  }
}
void saveHighScore() {
  File f = SPIFFS.open("/highscore.dat", "w");
  if (f) {
    f.write((uint8_t*)&highScore, sizeof(highScore));
    f.close();
    Serial.println("High score saved!");
  } else {
    Serial.println("Error: Failed to open high score file for writing");
  }
}

// Reset game function
void resetGame() {
  // Initialize player
  player.x = SCREEN_WIDTH / 4;
  player.y = SCREEN_HEIGHT / 2;
  player.velocity = 0;
  
  // Initialize pipes with proper spacing
  for (int i = 0; i < MAX_PIPES; i++) {
    // Space pipes evenly with increasing distance
    pipes[i].x = SCREEN_WIDTH + (i * (SCREEN_WIDTH + PIPE_WIDTH)) / 2;
    
    // Create random but fair gap positions
    // Ensure the gap isn't too close to the top or bottom
    int minGapPos = PIPE_GAP;
    int maxGapPos = SCREEN_HEIGHT - PIPE_GAP - GROUND_HEIGHT;
    pipes[i].gapY = random(minGapPos, maxGapPos);
    
    // Ensure consecutive pipes don't have drastically different gap heights
    if (i > 0) {
      int prevGapY = pipes[i-1].gapY;
      int maxDifference = PIPE_GAP / 2;
      
      if (abs(pipes[i].gapY - prevGapY) > maxDifference) {
        // If difference is too large, bring it closer to previous gap
        if (pipes[i].gapY > prevGapY) {
          pipes[i].gapY = prevGapY + maxDifference;
        } else {
          pipes[i].gapY = prevGapY - maxDifference;
        }
      }
    }
    
    // Initialize pipes with default speed
    pipes[i].speed = PIPE_SPEED;
    pipes[i].counted = false;
  }
  
  // Reset score
  score = 0;
}

// Handle touch input
void handleTouch() {
  // Implement touch debouncing with timing
  static unsigned long lastTouchTime = 0;
  unsigned long currentTime = millis();
  
  // Check touch with the XPT2046 library
  bool touched = false;
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    
    // Calibrate Touchscreen points with map function to the correct width and height
    touchX = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    touchY = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    touchZ = p.z;
    touched = true;
    
    // Debug touch coordinates
    static unsigned long lastLogTime = 0;
    if (currentTime - lastLogTime > 500) {
      Serial.print("Touch detected at X=");
      Serial.print(touchX);
      Serial.print(", Y=");
      Serial.print(touchY);
      Serial.print(", Z=");
      Serial.println(touchZ);
      lastLogTime = currentTime;
    }
  }
  
  // Detect new touch with improved debouncing
  touchActive = touched;
  bool newTouch = touchActive && !prevTouchActive;
  prevTouchActive = touchActive;
  
  // Process touch events with appropriate debounce times for each game state
  if (newTouch && (currentTime - lastTouchTime > 300)) { // General debounce
    lastTouchTime = currentTime;
    
    switch (gameState) {
      case MENU:
        // Start countdown instead of going directly to playing
        gameState = COUNTDOWN;
        resetGame();
        countdownStartTime = millis(); // Record when countdown started
        Serial.println("Starting countdown!");
        break;
        
      //case COUNTDOWN:
        // Allow skipping the countdown with a touch
       // gameState = PLAYING;
       // Serial.println("Countdown skipped!");
       // break;
        
      case PLAYING:
        // Jump when screen touched
        player.velocity = JUMP_FORCE;
        Serial.println("Flap!");
        break;
        
      case GAME_OVER:
        // Go to menu
        gameState = MENU;
        Serial.println("Returning to menu");
        break;
    }
  }
}

// Update game function - improved version with consistent difficulty progression
void updateGame(float deltaTime) {
  // Scale calculations by deltaTime for consistent physics regardless of frame rate
  float scaledGravity = GRAVITY * deltaTime * 60.0; // Scale to target 60fps
  
  // Update player with frame-rate independent physics
  player.velocity += scaledGravity;
  player.y += player.velocity * deltaTime * 60.0;
  
  // Apply terminal velocity to prevent excessive falling speed
  const float MAX_VELOCITY = 15.0;
  if (player.velocity > MAX_VELOCITY) {
    player.velocity = MAX_VELOCITY;
  }
  
  // Constrain player within screen bounds (add a small buffer at the top)
  if (player.y < 10) {
    player.y = 10;
    player.velocity = 0;
  }
  
  // Check collision with ground
  if (player.y + PLAYER_HEIGHT > SCREEN_HEIGHT - GROUND_HEIGHT) {
    player.y = SCREEN_HEIGHT - GROUND_HEIGHT - PLAYER_HEIGHT;
    gameState = GAME_OVER;
    Serial.println("Game Over: Hit ground");
  }
  
  // Calculate global difficulty parameters based on score
  float globalSpeed;
  int globalGapSize;
  
  // Determine difficulty level based on score
  if (score < 10) {
    // Beginner level (0-9)
    globalSpeed = PIPE_SPEED;
    globalGapSize = PIPE_GAP;
  } else if (score < 20) {
    // Intermediate level (10-19)
    globalSpeed = PIPE_SPEED * 1.1f;
    globalGapSize = PIPE_GAP - 5;
  } else if (score < 30) {
    // Advanced level (20-29)
    globalSpeed = PIPE_SPEED * 1.2f;
    globalGapSize = PIPE_GAP - 12;
  } else {
    // Expert level (30+)
    globalSpeed = PIPE_SPEED * 1.25f;
    globalGapSize = max(PIPE_GAP - 18, 70); // More forgiving minimum gap
  }
  
  // Apply global speed to all pipes for consistency
  // Only update speeds that have significantly changed to avoid disruption
  static float lastAppliedSpeed = 0;
  if (abs(lastAppliedSpeed - globalSpeed) > 0.05) {
    for (int i = 0; i < MAX_PIPES; i++) {
      pipes[i].speed = globalSpeed;
    }
    lastAppliedSpeed = globalSpeed;
  }
  
  // Update pipes with frame-rate independent movement
  for (int i = 0; i < MAX_PIPES; i++) {
    // Move the pipe using its own speed
    pipes[i].x -= pipes[i].speed * deltaTime * 60.0;
    
    // When a pipe moves off screen and is being reset
    if (pipes[i].x < -PIPE_WIDTH) {
      // Find the rightmost pipe
      int rightmostX = 0;
      for (int j = 0; j < MAX_PIPES; j++) {
        if (pipes[j].x > rightmostX) {
          rightmostX = pipes[j].x;
        }
      }
      
      // Place this pipe after the rightmost pipe with improved spacing
      pipes[i].x = rightmostX + SCREEN_WIDTH / 2;
      
      // Create challenging but fair gap positions
      // Ensure there's always enough room for the gap placement
      int minGapPos = max(20, globalGapSize/2); // Minimum allowed gap position from top
      int maxGapPos = SCREEN_HEIGHT - globalGapSize - GROUND_HEIGHT - 20; // Buffer from bottom
      
      // Generate new gap Y position
      pipes[i].gapY = random(minGapPos, maxGapPos);
      
      // Ensure consecutive pipes don't have drastically different gap heights
      int prevPipeIndex = -1;
      int secondRightmostX = 0;
      
      // Find the previous pipe (the one that's now the second-rightmost)
      for (int j = 0; j < MAX_PIPES; j++) {
        if (j != i && pipes[j].x > secondRightmostX && pipes[j].x < rightmostX) {
          secondRightmostX = pipes[j].x;
          prevPipeIndex = j;
        }
      }
      
      // If we found a previous pipe, ensure the gap isn't too different
      if (prevPipeIndex != -1) {
        int prevGapY = pipes[prevPipeIndex].gapY;
        int maxDifference = globalGapSize / 2; // Scale maximum difference with gap size
        
        if (abs(pipes[i].gapY - prevGapY) > maxDifference) {
          // If difference is too large, bring it closer to previous gap
          if (pipes[i].gapY > prevGapY) {
            pipes[i].gapY = prevGapY + maxDifference;
          } else {
            pipes[i].gapY = prevGapY - maxDifference;
          }
        }
      }
      
      // Reset counted flag for the new pipe
      pipes[i].counted = false;
    }
    
    // Check if player has passed this pipe
    if (!pipes[i].counted && player.x > pipes[i].x + PIPE_WIDTH) {
      pipes[i].counted = true;
      score++;
      if (score > highScore) {
        highScore = score;
        // Save high score to SPIFFS for persistence
        saveHighScore();
      }
      Serial.print("Score: ");
      Serial.println(score);
      
      // Debug to make sure we're continuing after high score
      if (score >= highScore) {
        Serial.println("New high score reached! Game continuing...");
      }
    }
    
    // Improved collision detection with more precise hitboxes and level-appropriate gap size
    // Create a slightly smaller hitbox for player to make the game feel more fair
    float playerHitboxX = player.x + 5;
    float playerHitboxY = player.y + 3; 
    float playerHitboxWidth = PLAYER_WIDTH - 10;
    float playerHitboxHeight = PLAYER_HEIGHT - 6;
    
    // Check collision with pipe - use global gap size for consistent collision detection
    bool inPipeHeight = playerHitboxY < pipes[i].gapY || playerHitboxY + playerHitboxHeight > pipes[i].gapY + globalGapSize;
    bool inPipeWidth = playerHitboxX + playerHitboxWidth > pipes[i].x && playerHitboxX < pipes[i].x + PIPE_WIDTH;
    
    if (inPipeWidth && inPipeHeight) {
      gameState = GAME_OVER;
      Serial.print("Game Over: Hit pipe ");
      Serial.println(i);
    }
  }
}
/*
 * Flappy Kiernan - Part 3
 * Rendering functions for each game state
 * 
 * This file contains all the rendering functions:
 * - renderMenu(): Game menu with animation
 * - renderCountdown(): Countdown animation before game starts
 * - renderGame(): Main game rendering
 * - renderGameOver(): Game over screen
 */

// ============= RENDERING FUNCTIONS =============
// Menu render function with animations
void renderMenu() {
  // Calculate animation frame
  static int lastAnimFrame = -1;
  unsigned long currentTime = millis();
  int animFrame = (currentTime / 150) % 6;
  
  // Title with shadow effect
  screenSprite.setTextColor(DARK_GREY);
  screenSprite.setTextSize(3);
  screenSprite.setCursor(42, 42);
  screenSprite.print("FLAPPY KIERNAN");
  
  screenSprite.setTextColor(TFT_YELLOW);
  screenSprite.setCursor(40, 40);
  screenSprite.print("FLAPPY KIERNAN");
  
  // Instructions with clearer formatting
  screenSprite.setTextColor(TFT_WHITE);
  screenSprite.setTextSize(2);
  
  // Create a semi-transparent text box
  screenSprite.fillRoundRect(15, 133, 290, 50, 5, screenSprite.color565(0, 0, 80));
  screenSprite.drawRoundRect(15, 133, 290, 50, 5, TFT_LIGHTGREY);
  
  // Set text alignment to center
  screenSprite.setTextDatum(lgfx::middle_center);
  // Draw text at the horizontal center of screen
  screenSprite.drawString("Tap to flap &", SCREEN_WIDTH/2, 150);
  screenSprite.drawString("avoid pipes!", SCREEN_WIDTH/2, 170);
  
  // High score
  if (highScore > 0) {
    screenSprite.setTextColor(GOLD_COLOR);
    screenSprite.setTextSize(1);
    
    // Create a small trophy icon
    int trophyX = SCREEN_WIDTH/2 - 75;
    int trophyY = 220;
    screenSprite.fillRect(trophyX, trophyY, 10, 8, GOLD_COLOR);
    screenSprite.fillRect(trophyX+2, trophyY+8, 6, 4, GOLD_COLOR);
    screenSprite.fillRect(trophyX+3, trophyY+12, 4, 2, GOLD_COLOR);
    
    screenSprite.setCursor(trophyX + 15, trophyY + 2);
    screenSprite.print("HIGH SCORE: ");
    screenSprite.print(highScore);
  }
  
  // Calculate sprite position with simple animation
  int spriteY = 90 + (animFrame < 3 ? animFrame : 5 - animFrame);
  
  // Draw Kiernan sprite at animated position
  screenSprite.pushImage(SCREEN_WIDTH/2 - PLAYER_WIDTH/2, spriteY, PLAYER_WIDTH, PLAYER_HEIGHT, kiernan_sprite, SKY_COLOR);
  
  // Animated start prompt with blinking effect
  screenSprite.fillRect(40, 200, 240, 16, screenSprite.color565(0, 0, 50));
  
  if ((currentTime / 500) % 2 == 0) {
    screenSprite.setTextColor(TFT_GREEN);
  } else {
    screenSprite.setTextColor(TFT_LIGHTGREEN);
  }
  screenSprite.setTextSize(2);
  screenSprite.setCursor(40, 200);
  screenSprite.print("TAP SCREEN TO START");
  
  lastAnimFrame = animFrame;
}

// Countdown function implementation
void renderCountdown() {
  // Calculate how long we've been in countdown mode
  unsigned long currentTime = millis();
  unsigned long countdownElapsed = currentTime - countdownStartTime;
  
  // Total countdown duration: 3 seconds
  const unsigned long COUNTDOWN_DURATION = 3000;
  
  // If countdown is complete, move to playing state
  if (countdownElapsed >= COUNTDOWN_DURATION) {
    gameState = PLAYING;
    return;
  }
  
  // Draw clouds in the background
  uint16_t cloudColor = screenSprite.color565(240, 240, 255);
  screenSprite.fillRoundRect(40, 25, 60, 20, 10, cloudColor);
  screenSprite.fillRoundRect(200, 40, 80, 25, 12, cloudColor);
  screenSprite.fillRoundRect(90, 80, 40, 15, 8, cloudColor);
  
  // Draw ground
  screenSprite.fillRect(0, SCREEN_HEIGHT - GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT, GROUND_COLOR);
  
  // Draw ground pattern
  uint16_t darkBrown = screenSprite.color565(101, 67, 33);
  for (int x = 0; x < SCREEN_WIDTH; x += 20) {
    screenSprite.fillRect(x, SCREEN_HEIGHT - GROUND_HEIGHT, 10, 5, darkBrown);
  }
  
  // Position player at starting position
  screenSprite.pushImage(player.x, player.y, PLAYER_WIDTH, PLAYER_HEIGHT, kiernan_sprite, SKY_COLOR);
  
  // Calculate which number to show (3, 2, 1)
  int countdownNumber;
  if (countdownElapsed < 1000) countdownNumber = 3;
  else if (countdownElapsed < 2000) countdownNumber = 2;
  else countdownNumber = 1;
  
  // Draw large countdown number with shadow effect
  screenSprite.setTextSize(6);
  
  // Shadow
  screenSprite.setTextColor(DARK_GREY);
  screenSprite.setCursor(SCREEN_WIDTH/2 - 22 + 2, SCREEN_HEIGHT/2 - 30 + 2);
  screenSprite.print(countdownNumber);
  
  // Main text
  screenSprite.setTextColor(TFT_WHITE);
  screenSprite.setCursor(SCREEN_WIDTH/2 - 22, SCREEN_HEIGHT/2 - 30);
  screenSprite.print(countdownNumber);
  
  // "Get Ready!" text above the number
  screenSprite.setTextSize(2);
  screenSprite.setTextColor(TFT_BLACK);
  screenSprite.setCursor(SCREEN_WIDTH/2 - 66 + 1, SCREEN_HEIGHT/2 - 70 + 1);
  screenSprite.print("GET READY!");
  
  screenSprite.setTextColor(TFT_YELLOW);
  screenSprite.setCursor(SCREEN_WIDTH/2 - 66, SCREEN_HEIGHT/2 - 70);
  screenSprite.print("GET READY!");
  
  // "Tap to skip" text at bottom
 // screenSprite.setTextSize(1);
 // screenSprite.setTextColor(TFT_LIGHTGREY);
 // screenSprite.setCursor(SCREEN_WIDTH/2 - 35, SCREEN_HEIGHT - 50);
  // screenSprite.print("Tap to skip");
}

/// Render game function - optimized version with better visuals
void renderGame() {
  // Draw clouds in the background
  uint16_t cloudColor = screenSprite.color565(240, 240, 255);
  screenSprite.fillRoundRect(40, 25, 60, 20, 10, cloudColor);
  screenSprite.fillRoundRect(200, 40, 80, 25, 12, cloudColor);
  screenSprite.fillRoundRect(90, 80, 40, 15, 8, cloudColor);
  
  // Draw pipes
  for (int i = 0; i < MAX_PIPES; i++) {
    // Top pipe
    screenSprite.fillRect(pipes[i].x, 0, PIPE_WIDTH, pipes[i].gapY, PIPE_COLOR);
    // Bottom pipe
    screenSprite.fillRect(pipes[i].x, pipes[i].gapY + PIPE_GAP, PIPE_WIDTH, 
                SCREEN_HEIGHT - pipes[i].gapY - PIPE_GAP - GROUND_HEIGHT, PIPE_COLOR);
    
    // Pipe caps (darker green)
    uint16_t darkGreen = screenSprite.color565(0, 100, 0);
    // Top pipe cap
    screenSprite.fillRect(pipes[i].x - 3, pipes[i].gapY - 10, PIPE_WIDTH + 6, 10, darkGreen);
    // Bottom pipe cap
    screenSprite.fillRect(pipes[i].x - 3, pipes[i].gapY + PIPE_GAP, PIPE_WIDTH + 6, 10, darkGreen);
  }
  
  // Draw ground
  screenSprite.fillRect(0, SCREEN_HEIGHT - GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT, GROUND_COLOR);
  
  // Draw ground pattern
  uint16_t darkBrown = screenSprite.color565(101, 67, 33);
  for (int x = 0; x < SCREEN_WIDTH; x += 20) {
    screenSprite.fillRect(x, SCREEN_HEIGHT - GROUND_HEIGHT, 10, 5, darkBrown);
  }
  
  // Draw player (Kiernan sprite)
  screenSprite.pushImage(player.x, player.y, PLAYER_WIDTH, PLAYER_HEIGHT, kiernan_sprite, SKY_COLOR);
  
  // Draw score with shadow for better visibility
  screenSprite.setTextColor(TFT_BLACK);
  screenSprite.setTextSize(2);
  screenSprite.setCursor(11, 11);
  screenSprite.print("Score: ");
  screenSprite.print(score);
  
  screenSprite.setTextColor(SCORE_COLOR);
  screenSprite.setCursor(10, 10);
  screenSprite.print("Score: ");
  screenSprite.print(score);
  
  // Add a difficulty indicator based on score - with more visible styling
  // Create a small level indicator background
  uint16_t levelBgColor;
  const char* levelText;
  
  if (score < 10) {
    levelBgColor = TFT_GREEN; // Easy level color
    levelText = "LEVEL: 1";
  } else if (score < 20) {
    levelBgColor = TFT_YELLOW; // Medium level color
    levelText = "LEVEL: 2";
  } else if (score < 30) {
    levelBgColor = TFT_ORANGE; // Hard level color
    levelText = "LEVEL: 3";
  } else {
    levelBgColor = TFT_RED; // Expert level color
    levelText = "LEVEL: MAX";
  }
  
  // Draw level indicator with background
  int levelX = SCREEN_WIDTH - 85;
  int levelY = 8;
  int levelWidth = 75;
  int levelHeight = 15;
  
  // Background box
  screenSprite.fillRoundRect(levelX, levelY, levelWidth, levelHeight, 4, levelBgColor);
  screenSprite.drawRoundRect(levelX, levelY, levelWidth, levelHeight, 4, TFT_WHITE);
  
  // Text
  screenSprite.setTextSize(1);
  screenSprite.setTextColor(TFT_BLACK);
  screenSprite.setCursor(levelX + 5, levelY + 4);
  screenSprite.print(levelText);
}

// Render game over function - improved version with better visuals
void renderGameOver() {
  unsigned long currentTime = millis();
  
  // Ground is a visual anchor
  screenSprite.fillRect(0, SCREEN_HEIGHT - GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT, GROUND_COLOR);
  
  // Game Over panel
  screenSprite.fillRoundRect(10, 60, SCREEN_WIDTH - 20, 120, 10, TFT_NAVY);
  screenSprite.drawRoundRect(10, 60, SCREEN_WIDTH - 20, 120, 10, TFT_WHITE);
  
  // Game Over text
  screenSprite.setTextColor(TFT_RED);
  screenSprite.setTextSize(3);
  screenSprite.setCursor(SCREEN_WIDTH/2 - 110, 80);
  screenSprite.print("GAME OVER");
  
  // Display score
  screenSprite.setTextColor(TFT_WHITE);
  screenSprite.setTextSize(2);
  screenSprite.setCursor(SCREEN_WIDTH/2 - 80, 120);
  screenSprite.print("Score: ");
  screenSprite.print(score);
  
  // Display high score with highlight if new record
  screenSprite.setCursor(SCREEN_WIDTH/2 - 80, 150);
  if (score >= highScore) {
    screenSprite.setTextColor(GOLD_COLOR);
    screenSprite.print("NEW BEST: ");
  } else {
    screenSprite.setTextColor(TFT_WHITE);
    screenSprite.print("Best: ");
  }
  screenSprite.print(highScore);
  
  // Tap to continue with blinking effect
  if ((currentTime / 500) % 2 == 0) {
    screenSprite.setTextColor(TFT_GREEN);
  } else {
    screenSprite.setTextColor(TFT_LIGHTGREEN);
  }
  screenSprite.setCursor(30, 190);
  screenSprite.print("TAP TO PLAY AGAIN");
}
