#include <heltec.h>
#include "HT_ST7789spi.h"
#include <Adafruit_GFX.h>
#include "SPI.h"

#define st7789_CS_Pin        39
#define st7789_REST_Pin      40
#define st7789_DC_Pin        47
#define st7789_SCLK_Pin      38
#define st7789_MOSI_Pin      48
#define st7789_LED_K_Pin     17
#define st7789_VTFT_CTRL_Pin  7

#define TEXT_SIZE   2
#define CHAR_W     (6 * TEXT_SIZE)
#define CHAR_H     (8 * TEXT_SIZE)
#define LINE_H     (CHAR_H + 2)
#define MARGIN_L   10
#define MARGIN_R   10
#define SCREEN_W   320
#define SCREEN_H   170
#define RIGHT_EDGE (SCREEN_W - MARGIN_R)
#define LAST_N     32

static HT_ST7789 *st7789 = NULL;
static SPIClass *gspi_lcd = NULL;

static bool isPrime(uint32_t n) {
  if (n < 2) return false;
  if (n == 2) return true;
  if (n % 2 == 0) return false;
  for (uint32_t i = 3; i * i <= n; i += 2) {
    if (n % i == 0) return false;
  }
  return true;
}

static void drawText(int16_t x, int16_t y, const char *text, uint16_t color) {
  st7789->setCursor(x, y);
  st7789->setTextColor(color);
  st7789->setTextWrap(false);
  st7789->print(text);
}

static void drawMark(int16_t x, int16_t y, bool ok) {
  if (ok) {
    uint16_t c = ST7789_GREEN;
    st7789->drawLine(x + 2,  y + 8,  x + 6,  y + 12, c);
    st7789->drawLine(x + 3,  y + 8,  x + 7,  y + 12, c);
    st7789->drawLine(x + 6,  y + 12, x + 14, y + 2,  c);
    st7789->drawLine(x + 7,  y + 12, x + 15, y + 2,  c);
  } else {
    uint16_t c = ST7789_RED;
    st7789->drawLine(x + 2,  y + 2,  x + 14, y + 14, c);
    st7789->drawLine(x + 3,  y + 2,  x + 15, y + 14, c);
    st7789->drawLine(x + 14, y + 2,  x + 2,  y + 14, c);
    st7789->drawLine(x + 15, y + 2,  x + 3,  y + 14, c);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(st7789_VTFT_CTRL_Pin, OUTPUT);
  digitalWrite(st7789_VTFT_CTRL_Pin, LOW);
  delay(20);

  gspi_lcd = new SPIClass(HSPI);
  st7789 = new HT_ST7789(240, 320, gspi_lcd, st7789_CS_Pin, st7789_DC_Pin, st7789_REST_Pin);
  gspi_lcd->begin(st7789_SCLK_Pin, -1, st7789_MOSI_Pin, st7789_CS_Pin);
  pinMode(gspi_lcd->pinSS(), OUTPUT);
  st7789->init(170, 320);

  ledcAttach(st7789_LED_K_Pin, 1000, 8);
  ledcWrite(st7789_LED_K_Pin, 55);

  st7789->setRotation(1);
  st7789->fillScreen(ST7789_BLACK);
  st7789->setTextSize(TEXT_SIZE);

  drawText(MARGIN_L, 2,                "Finding primes:", ST7789_GREEN);
  drawText(MARGIN_L, 2 + 2 * LINE_H,   "Count:",          ST7789_WHITE);
  drawText(MARGIN_L, 2 + 3 * LINE_H,   "Last:",           ST7789_WHITE);
}

void loop() {
  static uint32_t candidate  = 2;
  static uint32_t primeCount = 0;
  static uint32_t lastPrimes[LAST_N] = {0};
  static uint8_t  lastCount  = 0;
  char buf[32];

  bool ok = isPrime(candidate);
  if (ok) {
    primeCount++;
    if (lastCount < LAST_N) {
      lastPrimes[lastCount++] = candidate;
    } else {
      for (uint8_t i = 1; i < LAST_N; i++) lastPrimes[i - 1] = lastPrimes[i];
      lastPrimes[LAST_N - 1] = candidate;
    }
    Serial.printf("Prime #%lu: %lu\n", primeCount, candidate);
  }

  st7789->setTextSize(TEXT_SIZE);

  int16_t yCand = 2 + 1 * LINE_H;
  st7789->fillRect(0, yCand, SCREEN_W, CHAR_H, ST7789_BLACK);
  sprintf(buf, "%lu ->", candidate);
  drawText(MARGIN_L, yCand, buf, ST7789_CYAN);
  int16_t markX = MARGIN_L + ((int16_t)strlen(buf) + 1) * CHAR_W;
  drawMark(markX, yCand, ok);

  int16_t yCount = 2 + 2 * LINE_H;
  int16_t countX = MARGIN_L + 7 * CHAR_W; // after "Count: "
  st7789->fillRect(countX, yCount, SCREEN_W - countX, CHAR_H, ST7789_BLACK);
  sprintf(buf, "%lu", primeCount);
  drawText(countX, yCount, buf, ST7789_YELLOW);

  int16_t yListStart = 2 + 4 * LINE_H;
  st7789->fillRect(0, yListStart, SCREEN_W, SCREEN_H - yListStart, ST7789_BLACK);
  st7789->setTextColor(ST7789_CYAN);

  uint8_t fitCount = 0;
  {
    int16_t lx = MARGIN_L;
    int16_t ly = yListStart;
    for (int16_t i = (int16_t)lastCount - 1; i >= 0; i--) {
      char tok[16];
      snprintf(tok, sizeof(tok), "%lu, ", lastPrimes[i]);
      int16_t w = (int16_t)strlen(tok) * CHAR_W;
      if (lx + w > RIGHT_EDGE) {
        lx = MARGIN_L;
        ly += LINE_H;
        if (ly + CHAR_H > SCREEN_H) break;
      }
      lx += w;
      fitCount++;
    }
  }

  int16_t lx = MARGIN_L;
  int16_t ly = yListStart;
  for (uint8_t k = 0; k < fitCount; k++) {
    int16_t i = (int16_t)lastCount - 1 - k;
    char tok[16];
    bool last = (k + 1 == fitCount);
    snprintf(tok, sizeof(tok), last ? "%lu" : "%lu, ", lastPrimes[i]);
    int16_t w = (int16_t)strlen(tok) * CHAR_W;
    if (lx + w > RIGHT_EDGE) {
      lx = MARGIN_L;
      ly += LINE_H;
    }
    st7789->setCursor(lx, ly);
    st7789->print(tok);
    lx += w;
  }

  candidate++;
  delay(1000);
}
