#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char* AP_SSID     = "SnakeGame";
const char* AP_PASSWORD = "12345678";

// ── Display ───────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 3;  cfg.pin_mosi = 45;
      cfg.pin_miso = 46; cfg.pin_dc   = 47;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 14; cfg.pin_rst = 21; cfg.pin_busy = -1;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 48; cfg.invert = false;
      cfg.freq = 44100; cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

AsyncWebServer httpServer(80);
AsyncWebSocket ws("/ws");

// ── Game constants ────────────────────────────────────────
const int GRID_SIZE = 10;
const int GRID_W    = 32;   // 320 / 10
const int GRID_H    = 22;   // 240 / 10
const int HUD_ROWS  = 2;    // top 20px reserved for HUD

// ── Direction buffer (queue — prevents missed corner turns) ──
struct DirCmd { int x, y; };
const int DIR_BUF_SIZE = 4;
DirCmd dirBuffer[DIR_BUF_SIZE];
int dirBufHead = 0, dirBufTail = 0;

bool dirBufEmpty() { return dirBufHead == dirBufTail; }
bool dirBufFull()  { return ((dirBufTail + 1) % DIR_BUF_SIZE) == dirBufHead; }
void dirBufPush(int x, int y) {
  if (!dirBufFull()) {
    dirBuffer[dirBufTail] = {x, y};
    dirBufTail = (dirBufTail + 1) % DIR_BUF_SIZE;
  }
}
DirCmd dirBufPop() {
  DirCmd d = dirBuffer[dirBufHead];
  dirBufHead = (dirBufHead + 1) % DIR_BUF_SIZE;
  return d;
}

// ── Snake ─────────────────────────────────────────────────
int snakeX[500], snakeY[500];
int snakeLength = 3;
int dirX = 1, dirY = 0;
int foodX, foodY;

// ── Game state ────────────────────────────────────────────
int  score       = 0;
int  speed       = 150;  // ms per tick
bool gameOver    = false;
bool gameStarted = false;
unsigned long lastMove = 0;

// ── Forward declarations ──────────────────────────────────
void startGame();

// ── WebSocket broadcast helpers ───────────────────────────
void broadcastScore() {
  String json = "{\"score\":" + String(score) + "}";
  ws.textAll(json);
}
void broadcastStatus(const String& s) {
  String json = "{\"status\":\"" + s + "\"}";
  ws.textAll(json);
}

// ─────────────────────────────────────────────────────────
//  Mobile controller HTML
//  - clamp() keeps layout correct on both phone and desktop
// ─────────────────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Snake Controller</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    width: 100%; height: 100%;
    background: #0a0a0a; color: #fff;
    font-family: 'Segoe UI', sans-serif;
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    touch-action: none; user-select: none;
    overflow: hidden;
  }
  /* max-width keeps everything from going huge on desktop */
  .container {
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    gap: clamp(8px, 2vh, 16px);
    width: 100%;
    max-width: 480px;
    padding: 16px;
  }
  h1 { font-size: clamp(1.4rem, 6vw, 2.2rem); color: #00e676; letter-spacing: 6px; }
  #status { font-size: clamp(0.75rem, 3vw, 1rem); color: #aaa; min-height: 1.4em; text-align: center; }
  #score  { font-size: clamp(0.9rem, 4vw, 1.3rem); color: #ffd600; }
  .dpad {
    display: grid;
    grid-template-columns: repeat(3, clamp(70px, 22vw, 110px));
    grid-template-rows:    repeat(3, clamp(70px, 22vw, 110px));
    gap: clamp(4px, 2vw, 10px);
  }
  .btn {
    background: #1e1e1e; border: 2px solid #444;
    border-radius: 16px; font-size: clamp(1.4rem, 7vw, 2.4rem);
    display: flex; align-items: center; justify-content: center;
    cursor: pointer; -webkit-tap-highlight-color: transparent;
    transition: background 0.08s, transform 0.06s;
  }
  .btn.pressed, .btn:active { background: #00e676; color: #000; transform: scale(0.91); }
  .btn.center {
    background: #111; border-color: #00e676;
    font-size: clamp(0.6rem, 2.8vw, 0.85rem);
    color: #00e676; letter-spacing: 1px; font-weight: bold;
  }
  .btn.center.pressed, .btn.center:active { background: #00e676; color: #000; }
  .empty { visibility: hidden; }
  #startBtn {
    padding: clamp(10px, 3vw, 16px) clamp(28px, 10vw, 52px);
    background: #00e676; color: #000; border: none;
    border-radius: 40px; font-size: clamp(0.9rem, 4vw, 1.2rem);
    font-weight: bold; letter-spacing: 2px; cursor: pointer;
    box-shadow: 0 0 24px #00e67699;
    -webkit-tap-highlight-color: transparent;
  }
  #startBtn:active { transform: scale(0.95); background: #00c853; }
  #dot {
    width: 9px; height: 9px; border-radius: 50%;
    background: #f44; display: inline-block;
    margin-right: 6px; vertical-align: middle;
  }
  #dot.on { background: #00e676; }
</style>
</head>
<body>
<div class="container">
  <h1>&#127019; SNAKE</h1>
  <div id="status"><span id="dot"></span><span id="statusTxt">Connecting...</span></div>
  <div id="score">Score: 0</div>
  <div class="dpad">
    <div class="empty"></div>
    <div class="btn" data-cmd="U">&#8593;</div>
    <div class="empty"></div>
    <div class="btn" data-cmd="L">&#8592;</div>
    <div class="btn center" data-cmd="RESTART">REST<br>ART</div>
    <div class="btn" data-cmd="R">&#8594;</div>
    <div class="empty"></div>
    <div class="btn" data-cmd="D">&#8595;</div>
    <div class="empty"></div>
  </div>
  <button id="startBtn" onclick="sendCmd('S')">&#9654; START</button>
</div>
<script>
  const dot       = document.getElementById('dot');
  const statusTxt = document.getElementById('statusTxt');
  const scoreEl   = document.getElementById('score');
  let socket;
  function connect() {
    socket = new WebSocket('ws://' + location.hostname + '/ws');
    socket.onopen  = () => { dot.className='on'; statusTxt.textContent='Connected \u2713'; };
    socket.onclose = () => { dot.className=''; statusTxt.textContent='Reconnecting...'; setTimeout(connect, 2000); };
    socket.onerror = () => { statusTxt.textContent='Error'; };
    socket.onmessage = (e) => {
      try {
        const d = JSON.parse(e.data);
        if (d.score !== undefined) scoreEl.textContent = 'Score: ' + d.score;
        if (d.status) statusTxt.textContent = d.status;
      } catch(err) {}
    };
  }
  connect();
  function sendCmd(cmd) { if (socket && socket.readyState === 1) socket.send(cmd); }
  document.querySelectorAll('.btn[data-cmd]').forEach(btn => {
    const cmd = btn.dataset.cmd;
    btn.addEventListener('touchstart', e => { e.preventDefault(); sendCmd(cmd); btn.classList.add('pressed'); });
    btn.addEventListener('touchend',   e => { e.preventDefault(); btn.classList.remove('pressed'); });
    btn.addEventListener('mousedown',  () => { sendCmd(cmd); btn.classList.add('pressed'); });
    btn.addEventListener('mouseup',    () => btn.classList.remove('pressed'));
  });
  document.addEventListener('keydown', e => {
    const map = { ArrowUp:'U', ArrowDown:'D', ArrowLeft:'L', ArrowRight:'R',
                  w:'U', s:'D', a:'L', d:'R', r:'RESTART', Enter:'S' };
    if (map[e.key]) { e.preventDefault(); sendCmd(map[e.key]); }
  });
</script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────
//  WebSocket event handler
// ─────────────────────────────────────────────────────────
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (!info->final || info->index != 0 || info->len != len) return;
  if (info->opcode != WS_TEXT) return;

  String msg = "";
  for (size_t i = 0; i < len; i++) msg += (char)data[i];
  msg.trim();

  if (msg == "S")       { if (!gameStarted || gameOver) { gameStarted = true; startGame(); } return; }
  if (msg == "RESTART") { if (gameOver) { gameStarted = true; startGame(); } return; }
  if (msg == "U" && dirY != 1)  dirBufPush( 0, -1);
  if (msg == "D" && dirY != -1) dirBufPush( 0,  1);
  if (msg == "L" && dirX != 1)  dirBufPush(-1,  0);
  if (msg == "R" && dirX != -1) dirBufPush( 1,  0);
}

// ─────────────────────────────────────────────────────────
//  Display helpers
// ─────────────────────────────────────────────────────────
void showStartScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);  tft.setTextSize(4); tft.drawString("SNAKE", 80, 40);
  tft.setTextColor(TFT_YELLOW); tft.setTextSize(2); tft.drawString("WiFi Edition", 80, 100);
  tft.setTextColor(TFT_CYAN);   tft.setTextSize(1);
  tft.drawString("WiFi: SnakeGame  Pass: 12345678", 20, 145);
  tft.drawString("Open: 192.168.4.1", 80, 160);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("Press START on the web page", 50, 185);
}

void updateHUD() {
  tft.fillRect(0, 0, 320, 20, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE); tft.setTextSize(2);
  tft.setCursor(5, 2);   tft.print("Score:"); tft.print(score);
  tft.setCursor(200, 2); tft.print("Spd:"); tft.print(map(speed, 150, 30, 1, 10));
}

void drawFood() {
  tft.fillRect(foodX*GRID_SIZE, foodY*GRID_SIZE, GRID_SIZE-1, GRID_SIZE-1, TFT_RED);
}
void drawSnake() {
  tft.fillRect(snakeX[0]*GRID_SIZE, snakeY[0]*GRID_SIZE, GRID_SIZE-1, GRID_SIZE-1, TFT_GREEN);
  for (int i = 1; i < snakeLength; i++)
    tft.fillRect(snakeX[i]*GRID_SIZE, snakeY[i]*GRID_SIZE, GRID_SIZE-1, GRID_SIZE-1, TFT_DARKGREEN);
}
void eraseSnakeTail() {
  tft.fillRect(snakeX[snakeLength-1]*GRID_SIZE, snakeY[snakeLength-1]*GRID_SIZE,
               GRID_SIZE-1, GRID_SIZE-1, TFT_BLACK);
}
void showGameOverScreen() {
  tft.fillRect(55, 75, 210, 100, TFT_BLACK);
  tft.drawRect(55, 75, 210, 100, TFT_RED);
  tft.setTextColor(TFT_RED);    tft.setTextSize(3); tft.drawString("GAME OVER", 65, 85);
  tft.setTextColor(TFT_YELLOW); tft.setTextSize(2);
  tft.setCursor(80, 120); tft.print("Score: "); tft.print(score);
  tft.setTextColor(TFT_WHITE);  tft.setTextSize(1);
  tft.drawString("Press RESTART on app", 75, 155);
}

// ─────────────────────────────────────────────────────────
//  Game logic
// ─────────────────────────────────────────────────────────
void spawnFood() {
  bool valid = false; int tries = 0;
  while (!valid && tries++ < 300) {
    foodX = random(0, GRID_W); foodY = random(HUD_ROWS, GRID_H);
    valid = true;
    for (int i = 0; i < snakeLength; i++)
      if (snakeX[i]==foodX && snakeY[i]==foodY) { valid=false; break; }
  }
  drawFood();
}

void startGame() {
  dirBufHead = dirBufTail = 0;
  gameOver = false; score = 0; snakeLength = 3; speed = 150;
  dirX = 1; dirY = 0;
  snakeX[0]=GRID_W/2; snakeY[0]=GRID_H/2;
  snakeX[1]=snakeX[0]-1; snakeY[1]=snakeY[0];
  snakeX[2]=snakeX[1]-1; snakeY[2]=snakeY[1];
  tft.fillScreen(TFT_BLACK);
  updateHUD(); spawnFood(); drawSnake();
  broadcastScore(); broadcastStatus("Game started!");
  lastMove = millis();
}

void moveSnake() {
  if (!dirBufEmpty()) {
    DirCmd next = dirBufPop();
    if (!(next.x==-dirX && next.y==0) && !(next.y==-dirY && next.x==0)) {
      dirX = next.x; dirY = next.y;
    }
  }
  eraseSnakeTail();
  for (int i = snakeLength-1; i > 0; i--) { snakeX[i]=snakeX[i-1]; snakeY[i]=snakeY[i-1]; }
  snakeX[0]+=dirX; snakeY[0]+=dirY;

  if (snakeX[0]<0||snakeX[0]>=GRID_W||snakeY[0]<HUD_ROWS||snakeY[0]>=GRID_H) {
    gameOver=true; showGameOverScreen();
    broadcastStatus("Game Over! Score: "+String(score)); return;
  }
  for (int i=1; i<snakeLength; i++) {
    if (snakeX[0]==snakeX[i]&&snakeY[0]==snakeY[i]) {
      gameOver=true; showGameOverScreen();
      broadcastStatus("Game Over! Score: "+String(score)); return;
    }
  }
  if (snakeX[0]==foodX&&snakeY[0]==foodY) {
    snakeLength++; score+=10;
    if (speed>30) speed-=5;
    updateHUD(); spawnFood(); broadcastScore();
  }
  drawSnake();
}

// ─────────────────────────────────────────────────────────
//  setup() & loop()
// ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  tft.init(); tft.setRotation(1); tft.setBrightness(255); tft.fillScreen(TFT_BLACK);
  randomSeed(analogRead(0));

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  httpServer.addHandler(&ws);
  httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });
  httpServer.begin();

  showStartScreen();
  Serial.println("Open 192.168.4.1 in your browser.");
}

void loop() {
  ws.cleanupClients();
  if (!gameStarted || gameOver) return;
  unsigned long now = millis();
  if (now - lastMove >= (unsigned long)speed) { lastMove = now; moveSnake(); }
}