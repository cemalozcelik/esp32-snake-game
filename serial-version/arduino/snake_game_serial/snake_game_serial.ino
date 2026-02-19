#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 3;
      cfg.pin_mosi = 45;
      cfg.pin_miso = 46;
      cfg.pin_dc   = 47;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs   = 14;
      cfg.pin_rst  = 21;
      cfg.pin_busy = -1;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 48;
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

// Game settings
const int gridSize = 10;
const int gridWidth = 32;
const int gridHeight = 22;

// Snake
int snakeX[200];
int snakeY[200];
int snakeLength = 3;
int dirX = 1;
int dirY = 0;

// Food
int foodX;
int foodY;

// Game state
int score = 0;
int speed = 150;
bool gameOver = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  tft.init();
  tft.setRotation(1);
  tft.setBrightness(255);
  
  randomSeed(analogRead(0));
  
  showStartScreen();
  
  Serial.println("=== ESP32 SNAKE GAME ===");
  Serial.println("W = Up");
  Serial.println("A = Left");
  Serial.println("S = Down");
  Serial.println("D = Right");
  Serial.println("R = Restart");
}

void showStartScreen() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(4);
  tft.drawString("SNAKE", 100, 50);
  
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.drawString("GAME", 120, 100);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("Control with W/A/S/D", 80, 150);
  tft.drawString("Press any key to start", 70, 170);
  
  while(Serial.available() == 0) {
    delay(100);
  }
  while(Serial.available() > 0) Serial.read();
  
  startGame();
}

void startGame() {
  gameOver = false;
  score = 0;
  snakeLength = 3;
  speed = 150;
  
  snakeX[0] = gridWidth / 2;
  snakeY[0] = gridHeight / 2;
  snakeX[1] = snakeX[0] - 1;
  snakeY[1] = snakeY[0];
  snakeX[2] = snakeX[1] - 1;
  snakeY[2] = snakeY[1];
  
  dirX = 1;
  dirY = 0;
  
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 20, TFT_DARKGREY);
  updateScore();
  
  spawnFood();
  
  Serial.println("Game started!");
}

void updateScore() {
  tft.fillRect(5, 2, 150, 16, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 3);
  tft.print("Score: ");
  tft.print(score);
  
  tft.setCursor(200, 3);
  tft.print("Spd: ");
  tft.print(11 - speed/30);
}

void spawnFood() {
  bool validPosition = false;
  
  while(!validPosition) {
    foodX = random(0, gridWidth);
    foodY = random(2, gridHeight);
    
    validPosition = true;
    for(int i = 0; i < snakeLength; i++) {
      if(snakeX[i] == foodX && snakeY[i] == foodY) {
        validPosition = false;
        break;
      }
    }
  }
  
  drawFood();
}

void drawFood() {
  tft.fillRect(foodX * gridSize, foodY * gridSize, gridSize-1, gridSize-1, TFT_RED);
}

void drawSnake() {
  tft.fillRect(snakeX[0] * gridSize, snakeY[0] * gridSize, gridSize-1, gridSize-1, TFT_GREEN);
  
  for(int i = 1; i < snakeLength; i++) {
    tft.fillRect(snakeX[i] * gridSize, snakeY[i] * gridSize, gridSize-1, gridSize-1, TFT_DARKGREEN);
  }
}

void eraseSnakeTail() {
  tft.fillRect(snakeX[snakeLength-1] * gridSize, 
               snakeY[snakeLength-1] * gridSize, 
               gridSize-1, gridSize-1, TFT_BLACK);
}

void moveSnake() {
  eraseSnakeTail();
  
  for(int i = snakeLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i-1];
    snakeY[i] = snakeY[i-1];
  }
  
  snakeX[0] += dirX;
  snakeY[0] += dirY;
  
  if(snakeX[0] < 0 || snakeX[0] >= gridWidth || 
     snakeY[0] < 2 || snakeY[0] >= gridHeight) {
    gameOver = true;
    Serial.println("Hit the wall!");
    return;
  }
  
  for(int i = 1; i < snakeLength; i++) {
    if(snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
      gameOver = true;
      Serial.println("Hit yourself!");
      return;
    }
  }
  
  if(snakeX[0] == foodX && snakeY[0] == foodY) {
    snakeLength++;
    score += 10;
    
    if(speed > 30) speed -= 10;
    
    updateScore();
    spawnFood();
    
    Serial.print("Food eaten! Score: ");
    Serial.println(score);
  }
  
  drawSnake();
}

void handleInput() {
  if(Serial.available() > 0) {
    char command = Serial.read();
    
    switch(command) {
      case 'w':
      case 'W':
        if(dirY != 1) {
          dirX = 0;
          dirY = -1;
        }
        break;
        
      case 's':
      case 'S':
        if(dirY != -1) {
          dirX = 0;
          dirY = 1;
        }
        break;
        
      case 'a':
      case 'A':
        if(dirX != 1) {
          dirX = -1;
          dirY = 0;
        }
        break;
        
      case 'd':
      case 'D':
        if(dirX != -1) {
          dirX = 1;
          dirY = 0;
        }
        break;
        
      case 'r':
      case 'R':
        if(gameOver) {
          startGame();
        }
        break;
    }
  }
}

void showGameOver() {
  tft.fillRect(60, 80, 200, 80, TFT_BLACK);
  tft.drawRect(60, 80, 200, 80, TFT_RED);
  
  tft.setTextColor(TFT_RED);
  tft.setTextSize(3);
  tft.drawString("GAME OVER", 70, 90);
  
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(80, 120);
  tft.print("Score: ");
  tft.print(score);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("Press R to restart", 95, 145);
  
  Serial.println("=============================");
  Serial.println("       GAME OVER!");
  Serial.print("       SCORE: ");
  Serial.println(score);
  Serial.println("Press R to play again");
  Serial.println("=============================");
}

void loop() {
  if(!gameOver) {
    handleInput();
    moveSnake();
    delay(speed);
  } else {
    showGameOver();
    
    while(gameOver) {
      handleInput();
      delay(100);
    }
  }
}