#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <Adafruit_NeoPixel.h>

#define HX711_DOUT 4
#define HX711_SCK  SCK

#define LED_PIN    3
#define LED_COUNT  24

#define BTN_RIGHT  9
#define BTN_LEFT   8
#define BTN_SELECT 1
#define BUZZER_PIN 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
HX711 scale;
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

float calibration_factor = -714.07;
float target_weight = 0;       // grams
bool target_reached = false;
unsigned long last_btn_time = 0;
int select_press_count = 0;
unsigned long last_select_time = 0;
float smoothed_weight = 0;

// Ingredient color: R, G, B
struct IngColor { uint8_t r, g, b; };

// App states
enum State { STATE_MENU, STATE_ADD_GLASS, STATE_POURING, STATE_UNWIGHED, STATE_FINISH, STATE_SETTINGS };
State appState = STATE_MENU;

// Ingredient: weight=0 means unweighed (e.g. bitters, garnish)
struct Ingredient {
  const char* name;
  float weight;  // grams, 0 = unweighed/manual
};

struct Drink {
  const char* name;
  Ingredient weighed[6];    // measured ingredients
  int numWeighed;
  const char* unweighed[4]; // non-measured steps (bitters, garnish, etc.)
  int numUnweighed;
  const char* finish;       // "Stir", "Shake", etc.
};

// Weights in ml (= grams). Rounded to 25ml/0.25oz increments.
Drink drinks[] = {
  {
    "Daiquiri",
    {{"White Rum", 50}, {"Lime Juice", 25}, {"Simple Syrup", 25}},
    3,  // 50ml/2oz, 25ml/1oz, 25ml/0.75oz
    {},
    0,
    "Shake & strain"
  },
  {
    "Gin & Tonic",
    {{"Gin", 50}, {"Tonic Water", 125}},
    2,  // 50ml/2oz, 125ml/4oz
    {"Lime wedge"},
    1,
    "Stir gently"
  },
  {
    "Margarita",
    {{"Tequila", 50}, {"Lime Juice", 25}, {"Triple Sec", 25}},
    3,  // 50ml/2oz, 25ml/1oz, 25ml/1oz
    {"Salt rim"},
    1,
    "Shake & strain"
  },
  {
    "Martini",
    {{"Gin", 75}, {"Dry Vermouth", 25}},
    2,  // 75ml/2.5oz, 25ml/0.5oz
    {"Olive or twist"},
    1,
    "Stir & strain"
  },
  {
    "Mojito",
    {{"White Rum", 50}, {"Lime Juice", 25}, {"Simple Syrup", 25}, {"Soda Water", 50}},
    4,  // 50ml, 25ml, 25ml, 50ml
    {"Muddle mint", "Mint sprig"},
    2,
    "Stir gently"
  },
  {
    "Moscow Mule",
    {{"Vodka", 50}, {"Lime Juice", 25}, {"Ginger Beer", 125}},
    3,  // 50ml, 25ml, 125ml
    {"Lime wedge"},
    1,
    "Stir gently"
  },
  {
    "Negroni",
    {{"Gin", 25}, {"Campari", 25}, {"Sweet Vermouth", 25}},
    3,  // 25ml/1oz each
    {"Orange peel"},
    1,
    "Stir & strain"
  },
  {
    "Old Fashioned",
    {{"Bourbon", 50}, {"Simple Syrup", 10}},
    2,  // 50ml/2oz, splash
    {"2 dashes Bitters", "Orange peel"},
    2,
    "Stir gently"
  },
  {
    "Paloma",
    {{"Tequila", 50}, {"Lime Juice", 25}, {"Grapefruit Soda", 125}},
    3,  // 50ml, 25ml, 125ml
    {"Salt rim", "Lime wedge"},
    2,
    "Stir gently"
  },
  {
    "Whiskey Sour",
    {{"Bourbon", 50}, {"Lemon Juice", 25}, {"Simple Syrup", 25}},
    3,  // 50ml, 25ml, 25ml
    {"Cherry garnish"},
    1,
    "Shake & strain"
  }
};
const int NUM_DRINKS = 10;

int menuSelection = 0;
int currentIngredient = 0;
int currentUnweighed = 0;
float glassWeight = 0;
float cumulativeWeight = 0;

// Settings
bool muted = false;
uint8_t screenBrightness = 255;  // 0-255
uint8_t ledBrightness = 50;     // 0-255
int settingsSelection = 0;
bool settingsEditing = false;   // true when adjusting a value
const int NUM_SETTINGS = 5;    // Mute, Screen, LED, Units, Back

// Units: 0=ml, 1=fl oz
int unitMode = 0;
const char* unitLabels[] = {"ml", "oz"};
const int NUM_UNITS = 2;
unsigned long selectHoldStart = 0;
bool selectHeld = false;
bool selectConsumed = false;    // prevents hold from triggering click
State stateBeforeSettings = STATE_MENU;

void playTone(int pin, int freq, int duration) {
  if (muted) { delay(duration); return; }
  ledcAttach(pin, freq, 8);
  ledcWrite(pin, 128);  // 50% duty cycle
  delay(duration);
  ledcWrite(pin, 0);
  ledcDetach(pin);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

// Start a tone without blocking
void startTone(int pin, int freq) {
  ledcAttach(pin, freq, 8);
  ledcWrite(pin, 128);
}

void stopTone(int pin) {
  ledcWrite(pin, 0);
  ledcDetach(pin);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void updateBootDisplay(float fillProgress, unsigned long elapsed) {
  int baseLevel = 63 - (int)(fillProgress * 63);

  display.clearDisplay();

  for (int x = 0; x < 128; x++) {
    float wave = sin((x * 0.08) + (elapsed * 0.005)) * 3.0
               + sin((x * 0.15) + (elapsed * 0.008)) * 2.0;
    int surfaceY = baseLevel + (int)wave;
    if (surfaceY < 0) surfaceY = 0;
    if (surfaceY > 63) surfaceY = 63;
    if (surfaceY < 63) {
      display.drawLine(x, surfaceY, x, 63, SSD1306_WHITE);
    }
  }

  display.setTextSize(2);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(10, 16);
  display.print("Smart");
  display.setCursor(4, 36);
  display.print("Coaster");

  display.display();

  int litLeds = (int)(fillProgress * LED_COUNT);
  ring.clear();
  for (int i = 0; i < litLeds; i++) {
    uint32_t color = ring.ColorHSV((uint16_t)(i * 65536L / LED_COUNT), 255, 150);
    ring.setPixelColor(i, color);
  }
  ring.show();
}

void bootAnimation() {
  // Play chime first (clean hardware PWM)
  playTone(BUZZER_PIN, 523, 100);
  delay(30);
  playTone(BUZZER_PIN, 659, 100);
  delay(30);
  playTone(BUZZER_PIN, 784, 100);
  delay(30);
  playTone(BUZZER_PIN, 1047, 250);

  // Then run OLED fill + LED animation
  unsigned long animStart = millis();
  unsigned long animDuration = 1500;

  while (millis() - animStart < animDuration) {
    float progress = (float)(millis() - animStart) / animDuration;
    if (progress > 1.0) progress = 1.0;
    updateBootDisplay(progress, millis() - animStart);
  }

  // Final: blink green 3 times
  for (int i = 0; i < 3; i++) {
    ring.fill(ring.Color(0, 150, 0));
    ring.show();
    delay(200);
    ring.clear();
    ring.show();
    delay(200);
  }
}

// Convert grams to current unit and print to display
float convertWeight(float grams) {
  switch (unitMode) {
    case 1: return grams / 29.5735;  // fl oz
    default: return grams;           // ml (~= grams for liquids)
  }
}

int weightDecimals() {
  switch (unitMode) {
    case 1: return 1;  // oz - 1 decimal
    default: return 0; // ml - no decimals
  }
}

void printWeight(float grams) {
  float val = convertWeight(grams);
  display.print(val, weightDecimals());
  display.print(unitLabels[unitMode]);
}

void beep(unsigned long duration) {
  playTone(BUZZER_PIN, 2700, duration);
}

void chimeAndBlink() {
  ring.fill(ring.Color(0, 150, 0));
  ring.show();
  playTone(BUZZER_PIN, 1047, 80);
  delay(50);
  playTone(BUZZER_PIN, 1319, 80);
  delay(200);
  ring.clear();
  ring.show();
}

// Track per-LED brightness for smooth transitions
uint8_t led_brightness[LED_COUNT] = {0};


IngColor getIngredientColor(const char* name) {
  // Spirits - amber/warm
  if (strstr(name, "Bourbon") || strstr(name, "Whiskey"))  return {180, 100, 0};
  if (strstr(name, "Tequila"))   return {200, 180, 0};
  if (strstr(name, "Gin"))       return {100, 150, 200};
  if (strstr(name, "Vodka"))     return {150, 150, 200};
  if (strstr(name, "Rum"))       return {180, 80, 0};
  if (strstr(name, "Campari"))   return {200, 0, 30};
  // Mixers - bright/fresh
  if (strstr(name, "Lime"))      return {0, 200, 50};
  if (strstr(name, "Lemon"))     return {200, 200, 0};
  if (strstr(name, "Grapefruit"))return {200, 80, 80};
  if (strstr(name, "Simple"))    return {200, 180, 100};
  if (strstr(name, "Triple"))    return {200, 120, 0};
  if (strstr(name, "Vermouth"))  return {150, 0, 0};
  // Sodas/water - fizzy
  if (strstr(name, "Soda") || strstr(name, "Tonic") || strstr(name, "Ginger"))
    return {150, 200, 200};
  // Default blue
  return {0, 100, 200};
}

void updateRingProgress(float weight) {
  IngColor col = {0, 100, 200}; // default
  if (appState == STATE_POURING && currentIngredient < drinks[menuSelection].numWeighed) {
    col = getIngredientColor(drinks[menuSelection].weighed[currentIngredient].name);
  }

  if (target_weight <= 0 || weight <= 0) {
    for (int i = 0; i < LED_COUNT; i++) {
      if (led_brightness[i] > 0) {
        led_brightness[i] = (led_brightness[i] > 20) ? led_brightness[i] - 20 : 0;
      }
    }
  } else {
    float progress = weight / target_weight;
    if (progress > 1.0) progress = 1.0;

    int lit = (int)(progress * LED_COUNT);

    for (int i = 0; i < LED_COUNT; i++) {
      if (i < lit) {
        if (led_brightness[i] < 150) {
          int val = led_brightness[i] + 25;
          led_brightness[i] = (val > 150) ? 150 : (uint8_t)val;
        }
      } else if (i == lit) {
        float frac = (progress * LED_COUNT) - lit;
        uint8_t target_b = (uint8_t)(150 * frac);
        if (led_brightness[i] < target_b) { int val = led_brightness[i] + 25; led_brightness[i] = (val > target_b) ? target_b : (uint8_t)val; }
        else if (led_brightness[i] > target_b) led_brightness[i] = (led_brightness[i] > 25) ? led_brightness[i] - 25 : target_b;
      } else {
        if (led_brightness[i] > 0) {
          led_brightness[i] = (led_brightness[i] > 25) ? led_brightness[i] - 25 : 0;
        }
      }
    }
  }

  for (int i = 0; i < LED_COUNT; i++) {
    float b = led_brightness[i] / 150.0;
    ring.setPixelColor(i, ring.Color(
      (uint8_t)(col.r * b),
      (uint8_t)(col.g * b),
      (uint8_t)(col.b * b)
    ));
  }
  ring.show();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  ring.begin();
  ring.setBrightness(50);
  ring.clear();
  ring.show();

  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(calibration_factor);
  scale.tare();
  Serial.println("HX711 initialized & tared");

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  bootAnimation();
  ring.clear();
  ring.show();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.display();
  appState = STATE_MENU;
}

void readScale() {
  if (scale.is_ready()) {
    float raw = scale.get_units(1);
    smoothed_weight = smoothed_weight * 0.5 + raw * 0.5;
    smoothed_weight = round(smoothed_weight);
    if (smoothed_weight < 0) smoothed_weight = 0;
  }
}

bool btnPressed(int pin) {
  // For select button, only fire on release (and only if it wasn't a long hold)
  if (pin == BTN_SELECT) return false;  // handled by updateSelectHold
  if (millis() - last_btn_time > 150 && digitalRead(pin) == LOW) {
    last_btn_time = millis();
    beep(30);
    return true;
  }
  return false;
}

bool selectClicked = false;

void updateSelectHold() {
  selectClicked = false;
  if (digitalRead(BTN_SELECT) == LOW) {
    if (!selectHeld) {
      selectHeld = true;
      selectHoldStart = millis();
      selectConsumed = false;
    } else if (!selectConsumed && millis() - selectHoldStart > 2000) {
      selectConsumed = true;
      settingsSelection = 0;
      settingsEditing = false;
      stateBeforeSettings = appState;
      appState = STATE_SETTINGS;
      beep(50);
    }
  } else {
    // Button released
    if (selectHeld && !selectConsumed && millis() - selectHoldStart < 2000) {
      if (millis() - last_btn_time > 150) {
        selectClicked = true;
        last_btn_time = millis();
        beep(30);
      }
    }
    selectHeld = false;
    selectConsumed = false;
  }
}

bool selectPressed() {
  return selectClicked;
}

// Scroll long text: starts left-aligned, then smoothly bounces
void drawScrollText(int x, int y, int maxW, const char* text, int textSize) {
  int charW = (textSize == 2) ? 12 : 6;
  int textW = strlen(text) * charW;
  display.setTextSize(textSize);
  if (textW <= maxW) {
    display.setCursor(x, y);
    display.print(text);
  } else {
    int scrollRange = textW - maxW;
    // Start from left, pause 800ms, then smooth sine scroll
    int pauseMs = 800;
    int period = 2000 + scrollRange * 15;
    int totalCycle = pauseMs + period;
    int elapsed = millis() % totalCycle;
    int offset = 0;
    if (elapsed > pauseMs) {
      float phase = (float)(elapsed - pauseMs) / period;
      // Sine wave starting from 0 (left aligned)
      offset = (int)(scrollRange * (0.5 - 0.5 * cos(phase * 2 * PI)));
    }
    display.setCursor(x - offset, y);
    display.print(text);
  }
}

// ---- MENU STATE ----
int menuSlideX = 0;       // current slide position
int menuSlideTarget = 0;  // target slide position

void loopMenu() {
  if (btnPressed(BTN_RIGHT)) {
    menuSelection = (menuSelection + 1) % NUM_DRINKS;
    menuSlideTarget = -128;  // slide left
    menuSlideX = 128;        // new content enters from right
  }
  if (btnPressed(BTN_LEFT)) {
    menuSelection = (menuSelection - 1 + NUM_DRINKS) % NUM_DRINKS;
    menuSlideTarget = 128;
    menuSlideX = -128;
  }
  if (selectPressed()) {
    currentIngredient = 0;
    target_reached = false;
    appState = STATE_ADD_GLASS;
    return;
  }

  // Animate slide toward center
  if (menuSlideX > 0) menuSlideX -= 16;
  if (menuSlideX < 0) menuSlideX += 16;
  if (abs(menuSlideX) < 16) menuSlideX = 0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Select Drink:");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Draw current drink name centered with slide offset
  display.setTextSize(1);
  const char* name = drinks[menuSelection].name;
  int textW = strlen(name) * 6;  // size 1 = 6px per char
  int x = (128 - textW) / 2 + menuSlideX;
  display.setCursor(x, 28);
  display.print(name);

  // Navigation arrows
  display.setTextSize(2);
  display.setCursor(2, 28);
  display.print("<");
  display.setCursor(116, 28);
  display.print(">");

  // Dots indicator
  for (int i = 0; i < NUM_DRINKS; i++) {
    int dotX = 64 - (NUM_DRINKS * 5) + i * 10;
    if (i == menuSelection) {
      display.fillCircle(dotX, 55, 3, SSD1306_WHITE);
    } else {
      display.drawCircle(dotX, 55, 3, SSD1306_WHITE);
    }
  }

  display.display();
}

// ---- ADD GLASS STATE ----
void loopAddGlass() {
  readScale();

  if (selectPressed()) {
    // Glass is on, tare it out
    scale.tare();
    smoothed_weight = 0;
    cumulativeWeight = 0;
    currentIngredient = 0;
    target_weight = 0;
    target_reached = false;
    for (int i = 0; i < LED_COUNT; i++) led_brightness[i] = 0;
    ring.clear();
    ring.show();

    // Show first ingredient briefly
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(drinks[menuSelection].name);
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println("First up:");
    display.setTextSize(2);
    display.setCursor(0, 30);
    display.println(drinks[menuSelection].weighed[0].name);
    display.display();
    delay(1500);

    appState = STATE_POURING;
    return;
  }

  // Blink ring amber while waiting
  uint8_t pulse = (uint8_t)(abs((int)(millis() % 1000) - 500) * 150 / 500);
  ring.fill(ring.Color(pulse, pulse / 2, 0));
  ring.show();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(drinks[menuSelection].name);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(4, 20);
  display.println("Add your");
  display.setCursor(4, 40);
  display.println("glass");
  display.display();
}

// ---- POURING STATE ----
void loopPouring() {
  readScale();

  Drink &drink = drinks[menuSelection];
  Ingredient &ing = drink.weighed[currentIngredient];

  // Weight for this ingredient only (subtract previous cumulative)
  float ingWeight = smoothed_weight - cumulativeWeight;
  if (ingWeight < 0) ingWeight = 0;
  float ingTarget = ing.weight;

  // Update ring based on per-ingredient progress
  if (!target_reached) {
    float savedTarget = target_weight;
    target_weight = ingTarget;
    updateRingProgress(ingWeight);
    target_weight = savedTarget;
  }

  // Display first (so bar shows full before chime)
  float progress = (ingTarget > 0) ? ingWeight / ingTarget : 0;
  if (progress > 1.0) progress = 1.0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(drink.name);
  display.print(" ");
  display.print(currentIngredient + 1);
  display.print("/");
  display.print(drink.numWeighed);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Scroll ingredient name if too long
  char addBuf[32];
  snprintf(addBuf, sizeof(addBuf), "Add %s", ing.name);
  drawScrollText(0, 14, 128, addBuf, 1);

  display.setTextSize(2);
  display.setCursor(0, 26);
  printWeight(ingWeight);
  display.print("/");
  printWeight(ingTarget);

  display.drawRect(0, 52, 128, 12, SSD1306_WHITE);
  display.fillRect(2, 54, (int)(124 * progress), 8, SSD1306_WHITE);

  display.display();

  // Check target AFTER display update
  if (!target_reached && ingWeight >= ingTarget) {
    target_reached = true;
    chimeAndBlink();

    currentIngredient++;
    if (currentIngredient >= drink.numWeighed) {
      currentUnweighed = 0;
      if (drink.numUnweighed > 0) {
        appState = STATE_UNWIGHED;
      } else {
        appState = STATE_FINISH;
      }
      return;
    }

    // Next weighed ingredient
    cumulativeWeight = smoothed_weight;
    target_reached = false;
    for (int i = 0; i < LED_COUNT; i++) led_brightness[i] = 0;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(43, 10);
    display.print("Next:");
    display.setTextSize(1);
    const char* ingName = drink.weighed[currentIngredient].name;
    int nameW = strlen(ingName) * 6;
    display.setCursor((128 - nameW) / 2, 30);
    display.print(ingName);
    display.display();
    delay(1500);
    return;
  }
}

// ---- UNWEIGHED STEPS STATE ----
void loopUnweighed() {
  Drink &drink = drinks[menuSelection];

  // Pulse ring purple for manual steps
  uint8_t pulse = (uint8_t)(abs((int)(millis() % 1000) - 500) * 150 / 500);
  ring.fill(ring.Color(pulse / 2, 0, pulse));
  ring.show();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(drink.name);
  display.print(" - Add:");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  drawScrollText(0, 20, 128, drink.unweighed[currentUnweighed], 2);
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print("[OK] when done");
  display.display();

  if (selectPressed()) {
    currentUnweighed++;
    if (currentUnweighed >= drink.numUnweighed) {
      appState = STATE_FINISH;
    }
  }
}

// ---- FINISH STATE ----
void loopFinish() {
  Drink &drink = drinks[menuSelection];

  // Pulse ring green
  uint8_t pulse = (uint8_t)(abs((int)(millis() % 1200) - 600) * 150 / 600);
  ring.fill(ring.Color(0, pulse, 0));
  ring.show();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(drink.name);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  drawScrollText(0, 20, 128, drink.finish, 2);
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print("[OK] to finish");
  display.display();

  if (selectPressed()) {
    // Done! Back to menu
    ring.fill(ring.Color(0, 150, 0));
    ring.show();
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print("Cheers!");
    display.display();
    // Celebratory descending-ascending fanfare
    playTone(BUZZER_PIN, 784, 100);
    delay(30);
    playTone(BUZZER_PIN, 988, 100);
    delay(30);
    playTone(BUZZER_PIN, 1175, 100);
    delay(30);
    playTone(BUZZER_PIN, 1568, 300);
    delay(100);
    playTone(BUZZER_PIN, 1175, 80);
    delay(30);
    playTone(BUZZER_PIN, 1568, 400);
    delay(1000);
    ring.clear();
    ring.show();
    appState = STATE_MENU;
  }
}

// ---- SETTINGS STATE ----
void loopSettings() {
  if (settingsEditing) {
    // Left/Right adjust value
    if (btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) {
      int dir = (digitalRead(BTN_RIGHT) == LOW) ? 1 : -1;
      switch (settingsSelection) {
        case 0: muted = !muted; break;
        case 1:
          screenBrightness = (uint8_t)constrain((int)screenBrightness + dir * 5, 5, 255);
          display.ssd1306_command(SSD1306_SETCONTRAST);
          display.ssd1306_command(screenBrightness);
          break;
        case 2:
          ledBrightness = (uint8_t)constrain((int)ledBrightness + dir * 5, 5, 255);
          ring.setBrightness(ledBrightness);
          ring.fill(ring.Color(100, 100, 100));
          ring.show();
          break;
        case 3: // Units
          unitMode = (unitMode + dir + NUM_UNITS) % NUM_UNITS;
          break;
      }
    }
    // Select confirms and goes back to list
    if (selectPressed()) {
      settingsEditing = false;
      ring.clear();
      ring.show();
    }
  } else {
    // Navigate settings list with left/right
    if (btnPressed(BTN_RIGHT)) {
      settingsSelection = (settingsSelection + 1) % NUM_SETTINGS;
    }
    if (btnPressed(BTN_LEFT)) {
      settingsSelection = (settingsSelection - 1 + NUM_SETTINGS) % NUM_SETTINGS;
    }
    if (selectPressed()) {
      if (settingsSelection == 4) {
        // Back to where we came from
        appState = stateBeforeSettings;
        return;
      }
      settingsEditing = true;
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Settings");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  const char* labels[] = {"Sound", "Screen", "LED", "Units", "Back"};
  for (int i = 0; i < NUM_SETTINGS; i++) {
    display.setCursor(10, 14 + i * 10);
    if (i == settingsSelection) {
      display.print(settingsEditing ? "* " : "> ");
    } else {
      display.print("  ");
    }
    display.print(labels[i]);
    display.print(": ");
    switch (i) {
      case 0: display.print(muted ? "OFF" : "ON"); break;
      case 1: {
        int pct = (screenBrightness * 100) / 255;
        display.print(pct); display.print("%");
        break;
      }
      case 2: {
        int pct = (ledBrightness * 100) / 255;
        display.print(pct); display.print("%");
        break;
      }
      case 3: display.print(unitLabels[unitMode]); break;
      case 4: break;
    }
  }

  if (settingsEditing) {
    display.setCursor(0, 56);
    display.print("</>: adjust  [OK]: done");
  }

  display.display();
}

void loop() {
  updateSelectHold();

  switch (appState) {
    case STATE_MENU:      loopMenu();      break;
    case STATE_ADD_GLASS:  loopAddGlass();  break;
    case STATE_POURING:    loopPouring();   break;
    case STATE_UNWIGHED:   loopUnweighed(); break;
    case STATE_FINISH:     loopFinish();    break;
    case STATE_SETTINGS:   loopSettings();  break;
  }
  delay(20);
}
