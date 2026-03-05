// Smart Coaster — ESP32-S3 drink mixing assistant
// Hardware: SSD1306 OLED, HX711 load cell, 24-LED NeoPixel ring, passive buzzer, 3 buttons

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "web_ui.h"

// ---- Pin Definitions ----
#define HX711_DOUT  4
#define HX711_SCK   8
#define LED_PIN     3
#define LED_COUNT   24
#define BTN_RIGHT   9
#define BTN_LEFT    10
#define BTN_SELECT  1
#define BUZZER_PIN  43
#define BATT_PIN    2   // ADC pin for battery voltage divider (D1)

// ---- Display ----
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---- Peripherals ----
HX711 scale;
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
DNSServer dnsServer;

// ---- WiFi AP ----
const char* AP_SSID = "SmartCoaster";
const char* MDNS_HOST = "smartcoaster";  // smartcoaster.local
char apPassword[32] = "cheers123";

// ---- Scale ----
const float CALIBRATION_FACTOR = -714.07f;
float smoothed_weight = 0.0f;

// ---- Battery ----
float battVoltage = -1.0f;
int battPercent = 100;
bool battCharging = false;
bool battChargeError = false;
bool usbConnected = false;
float voltageAtPlugIn = 0;
unsigned long usbPlugInTime = 0;
unsigned long lastBattRead = 0;
const unsigned long BATT_READ_INTERVAL = 1000;  // read every 1s
const unsigned long CHARGE_CHECK_DELAY = 30000; // 30s to verify charging

// ---- App State ----
enum State : uint8_t {
  STATE_MENU, STATE_ADD_GLASS, STATE_POURING,
  STATE_UNWEIGHED, STATE_FINISH, STATE_SETTINGS, STATE_OVERPOUR
};
State appState = STATE_MENU;

// ---- Data Structures ----
struct IngColor { uint8_t r, g, b; };

struct Ingredient {
  String name;
  float ml;   // grams on scale (ml ≈ g for liquids)
  float oz;   // fluid ounces for display
};

struct Drink {
  String name;
  Ingredient weighed[6];
  int numWeighed;
  String unweighed[4];
  int numUnweighed;
  String finish;
};

// ---- Recipe Storage ----
#define MAX_DRINKS 20
Drink drinks[MAX_DRINKS];
int numDrinks = 0;

// ---- Drink-Making State ----
int menuSelection = 0;
int currentIngredient = 0;
int currentUnweighed = 0;
float cumulativeWeight = 0.0f;
bool target_reached = false;
int menuSlideX = 0;

// ---- Pour Detection ----
float lastPourWeight = 0.0f;
unsigned long settleStart = 0;
bool settling = false;
const unsigned long SETTLE_TIME = 1500;
const float SETTLE_THRESHOLD = 1.0f;
const float CLOSE_ENOUGH = 3.0f;
const float WAY_OVER = 15.0f;

// ---- Settings ----
bool muted = false;
int screenLevel = 0;
int ledLevel = 0;
const uint8_t LED_LEVELS[] = {10, 40, 120};
const char* BRIGHT_LABELS[] = {"Dim", "Normal", "Bright"};

// ---- Settings Menu ----
const int NUM_SETTINGS = 7;  // Sound, Screen, LED, WiFi, Pass, Sleep, Back
int settingsSelection = 0;
bool settingsEditing = false;
State stateBeforeSettings = STATE_MENU;

// ---- Button State ----
unsigned long lastBtnTime = 0;
unsigned long selectHoldStart = 0;
bool selectHeld = false;
bool selectConsumed = false;
bool selectClicked = false;

// ---- Sleep ----
const unsigned long SLEEP_TIMEOUT = 300000;  // 5 min
unsigned long lastActivity = 0;

// ---- LED Ring ----
uint8_t ledBright[LED_COUNT];

// ========================
// Default Recipes
// ========================
void loadDefaults() {
  numDrinks = 10;
  drinks[0] = {"Daiquiri",      {{"White Rum",60,2},{"Lime Juice",30,1},{"Simple Syrup",15,0.5}},   3, {},                                0, "Shake & strain"};
  drinks[1] = {"Gin & Tonic",   {{"Gin",45,1.5},{"Tonic Water",150,5}},                             2, {"Lime wedge"},                    1, "Stir gently"};
  drinks[2] = {"Margarita",     {{"Tequila",60,2},{"Lime Juice",30,1},{"Triple Sec",30,1}},         3, {"Salt rim"},                      1, "Shake & strain"};
  drinks[3] = {"Martini",       {{"Gin",75,2.5},{"Dry Vermouth",15,0.5}},                           2, {"Olive or twist"},                1, "Stir & strain"};
  drinks[4] = {"Mojito",        {{"White Rum",60,2},{"Lime Juice",30,1},{"Simple Syrup",15,0.5},{"Soda Water",60,2}}, 4, {"Muddle mint","Mint sprig"}, 2, "Stir gently"};
  drinks[5] = {"Moscow Mule",   {{"Vodka",60,2},{"Lime Juice",15,0.5},{"Ginger Beer",120,4}},      3, {"Lime wedge"},                    1, "Stir gently"};
  drinks[6] = {"Negroni",       {{"Gin",30,1},{"Campari",30,1},{"Sweet Vermouth",30,1}},            3, {"Orange peel"},                   1, "Stir & strain"};
  drinks[7] = {"Old Fashioned", {{"Bourbon",60,2},{"Simple Syrup",8,0.25}},                         2, {"2 dashes Bitters","Orange peel"},2, "Stir gently"};
  drinks[8] = {"Paloma",        {{"Tequila",60,2},{"Lime Juice",15,0.5},{"Grapefruit Soda",120,4}}, 3, {"Salt rim","Lime wedge"},         2, "Stir gently"};
  drinks[9] = {"Whiskey Sour",  {{"Bourbon",60,2},{"Lemon Juice",30,1},{"Simple Syrup",15,0.5}},    3, {"Cherry garnish"},                1, "Shake & strain"};
}

// ========================
// Persistence
// ========================
static void prefsKey(char* buf, int i) {
  snprintf(buf, 5, "d%d", i);
}

void saveRecipes() {
  Preferences prefs;
  prefs.begin("sc", false);
  prefs.putInt("n", numDrinks);
  char key[5];
  for (int i = 0; i < numDrinks; i++) {
    JsonDocument doc;
    doc["name"] = drinks[i].name;
    JsonArray w = doc["w"].to<JsonArray>();
    for (int j = 0; j < drinks[i].numWeighed; j++) {
      JsonObject ing = w.add<JsonObject>();
      ing["n"] = drinks[i].weighed[j].name;
      ing["ml"] = drinks[i].weighed[j].ml;
      ing["oz"] = drinks[i].weighed[j].oz;
    }
    JsonArray u = doc["u"].to<JsonArray>();
    for (int j = 0; j < drinks[i].numUnweighed; j++) {
      u.add(drinks[i].unweighed[j]);
    }
    doc["f"] = drinks[i].finish;
    String json;
    serializeJson(doc, json);
    prefsKey(key, i);
    prefs.putString(key, json);
  }
  for (int i = numDrinks; i < MAX_DRINKS; i++) {
    prefsKey(key, i);
    prefs.remove(key);
  }
  prefs.end();
}

bool loadRecipes() {
  Preferences prefs;
  prefs.begin("sc", true);
  int n = prefs.getInt("n", 0);
  if (n <= 0) { prefs.end(); return false; }
  numDrinks = min(n, MAX_DRINKS);
  char key[5];
  for (int i = 0; i < numDrinks; i++) {
    prefsKey(key, i);
    String json = prefs.getString(key, "");
    if (json.isEmpty()) continue;
    JsonDocument doc;
    if (deserializeJson(doc, json)) continue;  // skip on parse error
    drinks[i].name = doc["name"].as<String>();
    JsonArray w = doc["w"];
    drinks[i].numWeighed = min((int)w.size(), 6);
    for (int j = 0; j < drinks[i].numWeighed; j++) {
      drinks[i].weighed[j].name = w[j]["n"].as<String>();
      drinks[i].weighed[j].ml = w[j]["ml"];
      drinks[i].weighed[j].oz = w[j]["oz"];
    }
    JsonArray u = doc["u"];
    drinks[i].numUnweighed = min((int)u.size(), 4);
    for (int j = 0; j < drinks[i].numUnweighed; j++) {
      drinks[i].unweighed[j] = u[j].as<String>();
    }
    drinks[i].finish = doc["f"].as<String>();
  }
  prefs.end();
  return true;
}

void saveSettings() {
  Preferences prefs;
  prefs.begin("sc", false);
  prefs.putBool("muted", muted);
  prefs.putInt("scrLvl", screenLevel);
  prefs.putInt("ledLvl", ledLevel);
  prefs.putString("apPass", apPassword);
  prefs.end();
}

void loadSettings() {
  Preferences prefs;
  prefs.begin("sc", true);
  muted = prefs.getBool("muted", false);
  screenLevel = constrain((int)prefs.getInt("scrLvl", 0), 0, 2);
  ledLevel = constrain((int)prefs.getInt("ledLvl", 0), 0, 2);
  String savedPass = prefs.getString("apPass", "cheers123");
  strncpy(apPassword, savedPass.c_str(), sizeof(apPassword) - 1);
  apPassword[sizeof(apPassword) - 1] = '\0';
  prefs.end();
}

void sortDrinks() {
  for (int i = 0; i < numDrinks - 1; i++) {
    for (int j = 0; j < numDrinks - i - 1; j++) {
      if (drinks[j].name > drinks[j + 1].name) {
        std::swap(drinks[j], drinks[j + 1]);
      }
    }
  }
}

void clampMenuSelection() {
  if (numDrinks <= 0) { menuSelection = 0; return; }
  if (menuSelection >= numDrinks) menuSelection = numDrinks - 1;
  if (menuSelection < 0) menuSelection = 0;
}

// ========================
// Web API
// ========================
void parseDrinkFromJson(JsonDocument& doc, Drink& d) {
  d.name = doc["name"].as<String>();
  JsonArray w = doc["weighed"];
  d.numWeighed = min((int)w.size(), 6);
  for (int j = 0; j < d.numWeighed; j++) {
    d.weighed[j].name = w[j]["name"].as<String>();
    d.weighed[j].ml = w[j]["ml"];
    d.weighed[j].oz = w[j]["oz"];
  }
  // Clear unused ingredient slots
  for (int j = d.numWeighed; j < 6; j++) {
    d.weighed[j] = Ingredient();
  }
  JsonArray u = doc["unweighed"];
  d.numUnweighed = min((int)u.size(), 4);
  for (int j = 0; j < d.numUnweighed; j++) {
    d.unweighed[j] = u[j].as<String>();
  }
  for (int j = d.numUnweighed; j < 4; j++) {
    d.unweighed[j] = String();
  }
  d.finish = doc["finish"].as<String>();
}

void handleIndex() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleGetRecipes() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < numDrinks; i++) {
    JsonObject d = arr.add<JsonObject>();
    d["name"] = drinks[i].name;
    JsonArray w = d["weighed"].to<JsonArray>();
    for (int j = 0; j < drinks[i].numWeighed; j++) {
      JsonObject ing = w.add<JsonObject>();
      ing["name"] = drinks[i].weighed[j].name;
      ing["ml"] = drinks[i].weighed[j].ml;
      ing["oz"] = drinks[i].weighed[j].oz;
    }
    JsonArray u = d["unweighed"].to<JsonArray>();
    for (int j = 0; j < drinks[i].numUnweighed; j++) {
      u.add(drinks[i].unweighed[j]);
    }
    d["finish"] = drinks[i].finish;
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddRecipe() {
  if (numDrinks >= MAX_DRINKS) {
    server.send(400, "application/json", "{\"error\":\"Max recipes reached\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  parseDrinkFromJson(doc, drinks[numDrinks]);
  numDrinks++;
  sortDrinks();
  clampMenuSelection();
  saveRecipes();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleUpdateRecipe() {
  int id = server.arg("id").toInt();
  if (id < 0 || id >= numDrinks) {
    server.send(400, "application/json", "{\"error\":\"Invalid id\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  parseDrinkFromJson(doc, drinks[id]);
  sortDrinks();
  clampMenuSelection();
  saveRecipes();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteRecipe() {
  // Parse id from JSON body since WebServer may not parse DELETE query params
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc.containsKey("id")) {
    // Fallback to query param
    if (!server.hasArg("id")) {
      server.send(400, "application/json", "{\"error\":\"Missing id\"}");
      return;
    }
  }
  int id = doc.containsKey("id") ? doc["id"].as<int>() : server.arg("id").toInt();
  if (id < 0 || id >= numDrinks) {
    server.send(400, "application/json", "{\"error\":\"Invalid id\"}");
    return;
  }
  for (int i = id; i < numDrinks - 1; i++) {
    drinks[i] = drinks[i + 1];
  }
  numDrinks--;
  drinks[numDrinks] = Drink();
  clampMenuSelection();
  saveRecipes();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleResetRecipes() {
  loadDefaults();
  clampMenuSelection();
  saveRecipes();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleGetSettings() {
  JsonDocument doc;
  doc["muted"] = muted;
  doc["screenLevel"] = screenLevel;
  doc["ledLevel"] = ledLevel;
  doc["ssid"] = AP_SSID;
  doc["pass"] = apPassword;
  doc["ip"] = WiFi.softAPIP().toString();
  doc["hostname"] = String(MDNS_HOST) + ".local";
  doc["battVoltage"] = (int)(battVoltage * 100.0f) / 100.0f;
  doc["battPercent"] = battPercent;
  doc["battCharging"] = battCharging;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleUpdateSettings() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  if (doc.containsKey("muted")) muted = doc["muted"];
  if (doc.containsKey("screenLevel")) {
    screenLevel = constrain(doc["screenLevel"].as<int>(), 0, 2);
    display.dim(screenLevel == 0);
  }
  if (doc.containsKey("ledLevel")) {
    ledLevel = constrain(doc["ledLevel"].as<int>(), 0, 2);
    ring.setBrightness(LED_LEVELS[ledLevel]);
    ring.show();
  }
  bool passChanged = false;
  if (doc.containsKey("pass")) {
    String newPass = doc["pass"].as<String>();
    if (newPass.length() >= 8 && newPass.length() < sizeof(apPassword)) {
      strncpy(apPassword, newPass.c_str(), sizeof(apPassword) - 1);
      apPassword[sizeof(apPassword) - 1] = '\0';
      passChanged = true;
    } else {
      server.send(400, "application/json", "{\"error\":\"Password must be 8+ characters\"}");
      return;
    }
  }
  saveSettings();
  if (passChanged) {
    server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
    delay(500);
    ESP.restart();  // Need restart to apply new AP password
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeviceReset() {
  Preferences prefs;
  prefs.begin("sc", false);
  prefs.clear();
  prefs.end();
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

void setupWebServer() {
  WiFi.softAP(AP_SSID, apPassword);

  // mDNS: allows browsing to smartcoaster.local
  MDNS.begin(MDNS_HOST);
  MDNS.addService("http", "tcp", 80);

  // DNS redirect for captive portal (backup for devices that don't support mDNS)
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/recipes", HTTP_GET, handleGetRecipes);
  server.on("/api/recipes", HTTP_POST, handleAddRecipe);
  server.on("/api/recipes", HTTP_PUT, handleUpdateRecipe);
  server.on("/api/recipes", HTTP_DELETE, handleDeleteRecipe);
  server.on("/api/recipes/reset", HTTP_POST, handleResetRecipes);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_PUT, handleUpdateSettings);
  server.on("/api/device/reset", HTTP_POST, handleDeviceReset);

  // Captive portal: respond to OS connectivity checks
  server.on("/generate_204", HTTP_GET, handleIndex);       // Android
  server.on("/hotspot-detect.html", HTTP_GET, handleIndex); // iOS
  server.on("/connecttest.txt", HTTP_GET, handleIndex);     // Windows
  server.onNotFound([]() {
    if (server.uri().startsWith("/api")) {
      server.send(404, "application/json", "{\"error\":\"Not found\"}");
    } else {
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
    }
  });

  server.begin();
}

// ========================
// Sound
// ========================
void playTone(int freq, int duration) {
  if (muted) { delay(duration); return; }
  ledcAttach(BUZZER_PIN, freq, 8);
  ledcWrite(BUZZER_PIN, 128);
  delay(duration);
  ledcWrite(BUZZER_PIN, 0);
  ledcDetach(BUZZER_PIN);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void beep(int duration) {
  playTone(2700, duration);
}

void chimeAndBlink() {
  ring.fill(ring.Color(0, 150, 0));
  ring.show();
  playTone(1047, 80);
  delay(50);
  playTone(1319, 80);
  delay(200);
  ring.clear();
  ring.show();
}

// ========================
// Boot Animation
// ========================
void bootAnimation() {
  playTone(523, 100); delay(30);
  playTone(659, 100); delay(30);
  playTone(784, 100); delay(30);
  playTone(1047, 250);

  unsigned long start = millis();
  while (millis() - start < 1500) {
    unsigned long elapsed = millis() - start;
    float progress = (float)elapsed / 1500.0f;
    if (progress > 1.0f) progress = 1.0f;

    int baseLevel = 63 - (int)(progress * 63);
    display.clearDisplay();
    for (int x = 0; x < 128; x++) {
      float wave = sinf(x * 0.08f + elapsed * 0.005f) * 3.0f
                 + sinf(x * 0.15f + elapsed * 0.008f) * 2.0f;
      int surfaceY = constrain(baseLevel + (int)wave, 0, 63);
      if (surfaceY < 63) display.drawLine(x, surfaceY, x, 63, SSD1306_WHITE);
    }
    display.setTextSize(2);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(10, 16); display.print("Smart");
    display.setCursor(4, 36);  display.print("Coaster");
    display.display();

    int litLeds = (int)(progress * LED_COUNT);
    ring.clear();
    for (int i = 0; i < litLeds; i++) {
      ring.setPixelColor(i, ring.ColorHSV((uint16_t)(i * 65536L / LED_COUNT), 255, 150));
    }
    ring.show();
  }

  for (int i = 0; i < 3; i++) {
    ring.fill(ring.Color(0, 150, 0)); ring.show(); delay(200);
    ring.clear(); ring.show(); delay(200);
  }
}

// ========================
// Display Helpers
// ========================
void printCurrentWeight(float grams) {
  display.print(grams / 29.5735f, 1);
  display.print("oz");
}

void printTargetWeight(float ml, float oz) {
  // Show clean integers when possible (1oz not 1.0oz)
  if (oz == (int)oz) display.print((int)oz);
  else display.print(oz, 1);
  display.print("oz");
}

void drawBatteryIcon(int x, int y) {
  // Battery outline: 12x7 pixels
  display.drawRect(x, y, 10, 7, SSD1306_WHITE);
  display.fillRect(x + 10, y + 2, 2, 3, SSD1306_WHITE);  // nub

  if (battCharging) {
    // Lightning bolt inside
    display.drawLine(x + 5, y + 1, x + 3, y + 3, SSD1306_WHITE);
    display.drawLine(x + 3, y + 3, x + 6, y + 3, SSD1306_WHITE);
    display.drawLine(x + 6, y + 3, x + 4, y + 5, SSD1306_WHITE);
  } else {
    // Fill bar based on percentage
    int fillW = (int)(8.0f * battPercent / 100.0f);
    if (fillW > 0) display.fillRect(x + 1, y + 1, fillW, 5, SSD1306_WHITE);
  }
}

void drawHeader(const String& title) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);

  // Battery indicator top-right
  drawBatteryIcon(104, 1);
  char pctBuf[5];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", battPercent);
  int pw = strlen(pctBuf) * 6;
  display.setCursor(104 - pw - 1, 0);
  display.print(pctBuf);

  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  if (battChargeError) {
    display.setCursor(0, 13);
    display.setTextSize(1);
    display.print(F("! Charge error"));
  }
}

void drawScrollText(int x, int y, int maxW, const char* text, int len, int textSize) {
  int charW = (textSize == 2) ? 12 : 6;
  int textW = len * charW;
  display.setTextSize(textSize);
  if (textW <= maxW) {
    display.setCursor(x, y);
    display.print(text);
    return;
  }
  int scrollRange = textW - maxW;
  int pauseMs = 800;
  int period = 2000 + scrollRange * 15;
  int totalCycle = pauseMs + period;
  unsigned long now = millis();
  int elapsed = now % totalCycle;
  int offset = 0;
  if (elapsed > pauseMs) {
    float phase = (float)(elapsed - pauseMs) / period;
    offset = (int)(scrollRange * (0.5f - 0.5f * cosf(phase * 2.0f * PI)));
  }
  display.setCursor(x - offset, y);
  display.print(text);
}

// Convenience overload for String
void drawScrollText(int x, int y, int maxW, const String& text, int textSize) {
  drawScrollText(x, y, maxW, text.c_str(), text.length(), textSize);
}

// ========================
// LED Ring
// ========================
IngColor getIngredientColor(const String& name) {
  if (name.indexOf("Bourbon") >= 0 || name.indexOf("Whiskey") >= 0) return {180, 100, 0};
  if (name.indexOf("Tequila") >= 0)    return {200, 180, 0};
  if (name.indexOf("Gin") >= 0)        return {100, 150, 200};
  if (name.indexOf("Vodka") >= 0)      return {150, 150, 200};
  if (name.indexOf("Rum") >= 0)        return {180, 80, 0};
  if (name.indexOf("Campari") >= 0)    return {200, 0, 30};
  if (name.indexOf("Lime") >= 0)       return {0, 200, 50};
  if (name.indexOf("Lemon") >= 0)      return {200, 200, 0};
  if (name.indexOf("Grapefruit") >= 0) return {200, 80, 80};
  if (name.indexOf("Simple") >= 0)     return {200, 180, 100};
  if (name.indexOf("Triple") >= 0)     return {200, 120, 0};
  if (name.indexOf("Vermouth") >= 0)   return {150, 0, 0};
  if (name.indexOf("Soda") >= 0 || name.indexOf("Tonic") >= 0 || name.indexOf("Ginger") >= 0)
    return {150, 200, 200};
  return {0, 100, 200};
}

void updateRingProgress(float weight, float target) {
  IngColor col = {0, 100, 200};
  if (appState == STATE_POURING && currentIngredient < drinks[menuSelection].numWeighed) {
    col = getIngredientColor(drinks[menuSelection].weighed[currentIngredient].name);
  }

  if (target <= 0 || weight <= 0) {
    for (int i = 0; i < LED_COUNT; i++) {
      if (ledBright[i] > 20) ledBright[i] -= 20;
      else ledBright[i] = 0;
    }
  } else {
    float progress = weight / target;
    if (progress > 1.0f) progress = 1.0f;
    int lit = (int)(progress * LED_COUNT);

    for (int i = 0; i < LED_COUNT; i++) {
      uint8_t targetB;
      if (i < lit) {
        targetB = 150;
      } else if (i == lit) {
        targetB = (uint8_t)(150.0f * (progress * LED_COUNT - lit));
      } else {
        targetB = 0;
      }
      // Smooth toward target
      if (ledBright[i] < targetB) {
        int val = ledBright[i] + 25;
        ledBright[i] = (val > targetB) ? targetB : (uint8_t)val;
      } else if (ledBright[i] > targetB) {
        ledBright[i] = (ledBright[i] > targetB + 25) ? ledBright[i] - 25 : targetB;
      }
    }
  }

  for (int i = 0; i < LED_COUNT; i++) {
    float b = ledBright[i] / 150.0f;
    ring.setPixelColor(i, ring.Color(
      (uint8_t)(col.r * b),
      (uint8_t)(col.g * b),
      (uint8_t)(col.b * b)
    ));
  }
  ring.show();
}

// ========================
// Sleep
// ========================
void goToSleep() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(34, 28);
  display.print("Sleeping...");
  display.display();
  delay(800);

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  ring.clear();
  ring.show();
  scale.power_down();

  rtc_gpio_pullup_en((gpio_num_t)BTN_SELECT);
  rtc_gpio_pullup_en((gpio_num_t)BTN_LEFT);
  rtc_gpio_pullup_en((gpio_num_t)BTN_RIGHT);

  uint64_t mask = (1ULL << BTN_SELECT) | (1ULL << BTN_LEFT) | (1ULL << BTN_RIGHT);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// ========================
// Setup
// ========================
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  analogSetAttenuation(ADC_11db);  // ADC range 0-3.3V
  pinMode(BATT_PIN, INPUT);

  loadSettings();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.dim(screenLevel == 0);
  display.display();

  // Now init peripherals after display is stable
  ring.begin();
  ring.setBrightness(LED_LEVELS[ledLevel]);
  ring.clear();
  ring.show();

  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();

  // Force initial battery read
  readBattery();

  if (!loadRecipes()) {
    loadDefaults();
    saveRecipes();
  }
  clampMenuSelection();

  bootAnimation();
  setupWebServer();  // Start WiFi after boot to reduce battery inrush

  ring.clear();
  ring.show();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  appState = STATE_MENU;
  lastActivity = millis();
}

// ========================
// Input
// ========================
void checkUSB() {
  // Voltage-based USB detection
  // Runs inside readBattery() via battVoltage updates
}

void readScale() {
  if (!scale.is_ready()) return;
  float raw = scale.get_units(1);
  float prev = smoothed_weight;
  smoothed_weight = smoothed_weight * 0.5f + raw * 0.5f;
  smoothed_weight = roundf(smoothed_weight);
  if (smoothed_weight < 0) smoothed_weight = 0;
  if (fabsf(smoothed_weight - prev) >= 2.0f) lastActivity = millis();
}

void readBattery() {
  unsigned long now = millis();
  if (now - lastBattRead < BATT_READ_INTERVAL) return;
  lastBattRead = now;

  // Read ADC (voltage divider halves battery voltage)
  uint32_t mv = analogReadMilliVolts(BATT_PIN);
  float voltage = (mv * 2.0f) / 1000.0f;  // multiply by 2 for divider

  // Smooth the reading (skip EMA on first read)
  if (battVoltage < 0) battVoltage = voltage;
  else battVoltage = battVoltage * 0.9f + voltage * 0.1f;

  // Detect USB plug/unplug via voltage jump
  // Charge IC raises terminal voltage on plug-in, drops on unplug
  static float prevVoltage = -1;
  if (prevVoltage > 0) {
    float jump = voltage - prevVoltage;
    if (jump > 0.05f && !usbConnected) {
      // Voltage jumped up — USB plugged in
      usbConnected = true;
      usbPlugInTime = now;
      voltageAtPlugIn = voltage;
      battChargeError = false;
    } else if (jump < -0.05f && usbConnected) {
      // Voltage dropped — USB unplugged
      usbConnected = false;
    }
  }
  prevVoltage = voltage;

  if (usbConnected && battPercent < 100) {
    battCharging = true;
    if (!battChargeError && (now - usbPlugInTime > CHARGE_CHECK_DELAY)) {
      if (battVoltage <= voltageAtPlugIn + 0.01f) {
        battChargeError = true;
      }
    }
  } else {
    battCharging = false;
    battChargeError = false;
  }

  // Map voltage to percentage (LiPo curve approximation)
  // 4.2V=100%, 3.7V=50%, 3.3V=10%, 3.0V=0%
  if (battVoltage >= 4.2f) {
    battPercent = 100;
  } else if (battVoltage >= 3.7f) {
    battPercent = 50 + (int)((battVoltage - 3.7f) / 0.5f * 50.0f);
  } else if (battVoltage >= 3.3f) {
    battPercent = 10 + (int)((battVoltage - 3.3f) / 0.4f * 40.0f);
  } else if (battVoltage >= 3.0f) {
    battPercent = (int)((battVoltage - 3.0f) / 0.3f * 10.0f);
  } else {
    battPercent = 0;
  }
  battPercent = constrain(battPercent, 0, 100);
}

bool btnPressed(int pin) {
  if (pin == BTN_SELECT) return false;
  unsigned long now = millis();
  if (now - lastBtnTime > 150 && digitalRead(pin) == LOW) {
    lastBtnTime = now;
    lastActivity = now;
    beep(30);
    return true;
  }
  return false;
}

void updateSelectHold() {
  selectClicked = false;
  unsigned long now = millis();
  if (digitalRead(BTN_SELECT) == LOW) {
    if (!selectHeld) {
      selectHeld = true;
      selectHoldStart = now;
      selectConsumed = false;
      lastActivity = now;
    } else if (!selectConsumed && now - selectHoldStart > 2000) {
      selectConsumed = true;
      settingsSelection = 0;
      settingsEditing = false;
      stateBeforeSettings = appState;
      appState = STATE_SETTINGS;
      beep(50);
    }
  } else {
    if (selectHeld && !selectConsumed && now - selectHoldStart < 2000) {
      if (now - lastBtnTime > 150) {
        selectClicked = true;
        lastBtnTime = now;
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

// ========================
// App States
// ========================
void loopMenu() {
  if (numDrinks == 0) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.print(F("No recipes!"));
    display.setCursor(10, 35);
    display.print(F("Add via web app"));
    display.display();
    return;
  }

  if (btnPressed(BTN_RIGHT)) {
    menuSelection = (menuSelection + 1) % numDrinks;
    menuSlideX = 128;
  }
  if (btnPressed(BTN_LEFT)) {
    menuSelection = (menuSelection - 1 + numDrinks) % numDrinks;
    menuSlideX = -128;
  }
  if (selectPressed()) {
    currentIngredient = 0;
    target_reached = false;
    appState = STATE_ADD_GLASS;
    return;
  }

  // Animate slide toward center
  if (menuSlideX > 0) { menuSlideX -= 16; if (menuSlideX < 0) menuSlideX = 0; }
  if (menuSlideX < 0) { menuSlideX += 16; if (menuSlideX > 0) menuSlideX = 0; }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawHeader(F("Select Drink:"));

  const String& name = drinks[menuSelection].name;
  int textW = name.length() * 6;
  display.setCursor((128 - textW) / 2 + menuSlideX, 28);
  display.print(name);

  display.setTextSize(2);
  display.setCursor(2, 28);   display.print('<');
  display.setCursor(116, 28); display.print('>');

  display.setTextSize(1);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d/%d", menuSelection + 1, numDrinks);
  int pw = strlen(buf) * 6;
  display.setCursor((128 - pw) / 2, 55);
  display.print(buf);

  display.display();
}

void cancelDrink() {
  ring.clear();
  ring.show();
  memset(ledBright, 0, sizeof(ledBright));
  appState = STATE_MENU;
}

void loopAddGlass() {
  readScale();
  Drink& drink = drinks[menuSelection];

  if (btnPressed(BTN_LEFT)) { cancelDrink(); return; }
  if (selectPressed()) {
    scale.tare();
    smoothed_weight = 0;
    cumulativeWeight = 0;
    currentIngredient = 0;
    target_reached = false;
    memset(ledBright, 0, sizeof(ledBright));
    ring.clear();
    ring.show();

    display.clearDisplay();
    drawHeader(drink.name);
    display.setCursor(0, 16);
    display.print(F("First up:"));
    display.setTextSize(2);
    display.setCursor(0, 30);
    display.print(drink.weighed[0].name);
    display.display();
    delay(1500);

    settling = false;
    lastPourWeight = 0;
    appState = STATE_POURING;
    return;
  }

  // Pulse ring amber
  uint8_t pulse = (uint8_t)(abs((int)(millis() % 1000) - 500) * 150 / 500);
  ring.fill(ring.Color(pulse, pulse / 2, 0));
  ring.show();

  display.clearDisplay();
  drawHeader(drink.name);
  display.setTextSize(2);
  display.setCursor(4, 20); display.print(F("Add your"));
  display.setCursor(4, 40); display.print(F("glass"));
  display.display();
}

void advanceIngredient() {
  Drink& drink = drinks[menuSelection];
  currentIngredient++;
  if (currentIngredient >= drink.numWeighed) {
    currentUnweighed = 0;
    appState = (drink.numUnweighed > 0) ? STATE_UNWEIGHED : STATE_FINISH;
    return;
  }

  cumulativeWeight = smoothed_weight;
  target_reached = false;
  settling = false;
  lastPourWeight = 0;
  memset(ledBright, 0, sizeof(ledBright));

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(43, 10);
  display.print(F("Next:"));
  const String& ingName = drink.weighed[currentIngredient].name;
  int nameW = ingName.length() * 6;
  display.setCursor((128 - nameW) / 2, 30);
  display.print(ingName);
  display.display();
  delay(1500);
}

void loopOverpour() {
  readScale();
  Drink& drink = drinks[menuSelection];
  Ingredient& ing = drink.weighed[currentIngredient];
  float ingWeight = smoothed_weight - cumulativeWeight;

  // Flash red ring
  if ((millis() / 300) % 2 == 0) ring.fill(ring.Color(150, 0, 0));
  else ring.clear();
  ring.show();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Too much "));
  display.print(ing.name);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 16);
  printCurrentWeight(ingWeight);
  display.print('/');
  printTargetWeight(ing.ml, ing.oz);

  display.setTextSize(1);
  display.setCursor(0, 38); display.print(F("May need to adjust"));
  display.setCursor(0, 48); display.print(F("other ingredients"));
  display.setCursor(0, 58); display.print(F("[<] Start over  [>] OK"));
  display.display();

  if (btnPressed(BTN_LEFT)) {
    ring.clear(); ring.show();
    appState = STATE_MENU;
  }
  if (btnPressed(BTN_RIGHT)) {
    target_reached = true;
    chimeAndBlink();
    advanceIngredient();
  }
}

void loopPouring() {
  if (btnPressed(BTN_LEFT)) { cancelDrink(); return; }
  readScale();
  Drink& drink = drinks[menuSelection];
  Ingredient& ing = drink.weighed[currentIngredient];

  float ingWeight = smoothed_weight - cumulativeWeight;
  if (ingWeight < 0) ingWeight = 0;
  float ingTarget = ing.ml;

  if (!target_reached) {
    updateRingProgress(ingWeight, ingTarget);
  }

  float progress = (ingTarget > 0) ? ingWeight / ingTarget : 0;
  if (progress > 1.0f) progress = 1.0f;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header with progress counter
  display.setTextSize(1);
  char hdr[32];
  snprintf(hdr, sizeof(hdr), "%s %d/%d", drink.name.c_str(), currentIngredient + 1, drink.numWeighed);
  display.setCursor(0, 0);
  display.print(hdr);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Ingredient name (scrolling if needed) — build once, reuse c_str
  char addBuf[40];
  snprintf(addBuf, sizeof(addBuf), "Add %s", ing.name.c_str());
  drawScrollText(0, 14, 128, addBuf, strlen(addBuf), 1);

  display.setTextSize(2);
  display.setCursor(0, 26);
  printCurrentWeight(ingWeight);
  display.print('/');
  printTargetWeight(ing.ml, ing.oz);

  display.drawRect(0, 52, 128, 12, SSD1306_WHITE);
  int barW = (int)(124.0f * progress);
  if (barW > 0) display.fillRect(2, 54, barW, 8, SSD1306_WHITE);

  display.display();

  // Settle detection
  if (!target_reached) {
    if (fabsf(ingWeight - lastPourWeight) > SETTLE_THRESHOLD) {
      lastPourWeight = ingWeight;
      settleStart = millis();
      settling = false;
    } else if (!settling && ingWeight > 2.0f && millis() - settleStart > SETTLE_TIME) {
      settling = true;
    }

    if (settling) {
      float diff = ingWeight - ingTarget;
      if (diff > WAY_OVER) {
        appState = STATE_OVERPOUR;
        return;
      }
      if (diff >= -CLOSE_ENOUGH) {
        target_reached = true;
        chimeAndBlink();
        advanceIngredient();
        return;
      }
      settling = false;
    }
  }
}

void loopUnweighed() {
  if (btnPressed(BTN_LEFT)) { cancelDrink(); return; }
  Drink& drink = drinks[menuSelection];

  uint8_t pulse = (uint8_t)(abs((int)(millis() % 1000) - 500) * 150 / 500);
  ring.fill(ring.Color(pulse / 2, 0, pulse));
  ring.show();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawHeader(drink.name + " - Add:");
  drawScrollText(0, 20, 128, drink.unweighed[currentUnweighed], 2);
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("[OK] when done"));
  display.display();

  if (selectPressed()) {
    currentUnweighed++;
    if (currentUnweighed >= drink.numUnweighed) {
      appState = STATE_FINISH;
    }
  }
}

void loopFinish() {
  if (btnPressed(BTN_LEFT)) { cancelDrink(); return; }
  Drink& drink = drinks[menuSelection];

  uint8_t pulse = (uint8_t)(abs((int)(millis() % 1200) - 600) * 150 / 600);
  ring.fill(ring.Color(0, pulse, 0));
  ring.show();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawHeader(drink.name);
  drawScrollText(0, 20, 128, drink.finish, 2);
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("[OK] to finish"));
  display.display();

  if (selectPressed()) {
    ring.fill(ring.Color(0, 150, 0));
    ring.show();
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print(F("Cheers!"));
    display.display();
    playTone(784, 100);  delay(30);
    playTone(988, 100);  delay(30);
    playTone(1175, 100); delay(30);
    playTone(1568, 300); delay(100);
    playTone(1175, 80);  delay(30);
    playTone(1568, 400); delay(1000);
    ring.clear();
    ring.show();
    appState = STATE_MENU;
  }
}

void loopSettings() {
  if (settingsEditing) {
    if (btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) {
      int dir = (digitalRead(BTN_RIGHT) == LOW) ? 1 : -1;
      switch (settingsSelection) {
        case 0: muted = !muted; break;
        case 1:
          screenLevel = constrain(screenLevel + dir, 0, 2);
          display.dim(screenLevel == 0);
          break;
        case 2:
          ledLevel = constrain(ledLevel + dir, 0, 2);
          ring.setBrightness(LED_LEVELS[ledLevel]);
          ring.fill(ring.Color(100, 100, 100));
          ring.show();
          break;
      }
      saveSettings();
    }
    if (selectPressed()) {
      settingsEditing = false;
      ring.clear();
      ring.show();
    }
  } else {
    if (btnPressed(BTN_RIGHT)) settingsSelection = (settingsSelection + 1) % NUM_SETTINGS;
    if (btnPressed(BTN_LEFT))  settingsSelection = (settingsSelection - 1 + NUM_SETTINGS) % NUM_SETTINGS;
    if (selectPressed()) {
      if (settingsSelection == 5) { goToSleep(); return; }
      if (settingsSelection == 6) { appState = stateBeforeSettings; return; }
      // WiFi/Pass (3,4) are info-only, don't enter edit mode
      if (settingsSelection <= 2) settingsEditing = true;
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawHeader(F("Settings"));

  // Scrollable list — show 5 visible rows, scroll if needed
  int startIdx = 0;
  const int VISIBLE_ROWS = 5;
  if (settingsSelection > VISIBLE_ROWS - 1) {
    startIdx = settingsSelection - (VISIBLE_ROWS - 1);
  }

  static const char* labels[] = {"Sound", "Screen", "LED", "WiFi", "Pass", "Sleep", "Back"};
  for (int v = 0; v < VISIBLE_ROWS && (startIdx + v) < NUM_SETTINGS; v++) {
    int i = startIdx + v;
    display.setCursor(10, 14 + v * 10);
    display.print((i == settingsSelection) ? (settingsEditing ? F("* ") : F("> ")) : F("  "));
    display.print(labels[i]);
    display.print(F(": "));
    switch (i) {
      case 0: display.print(muted ? F("OFF") : F("ON")); break;
      case 1: display.print(BRIGHT_LABELS[screenLevel]); break;
      case 2: display.print(BRIGHT_LABELS[ledLevel]); break;
      case 3: display.print(AP_SSID); break;
      case 4: display.print(apPassword); break;
      case 5: break;
      case 6: break;
    }
  }

  if (settingsEditing) {
    display.setCursor(0, 56);
    display.print(F("</>: adjust  [OK]: done"));
  }

  display.display();
}

// ========================
// Main Loop
// ========================
void loop() {
  updateSelectHold();
  checkUSB();
  readBattery();
  dnsServer.processNextRequest();
  server.handleClient();

  if (millis() - lastActivity > SLEEP_TIMEOUT) {
    goToSleep();
  }

  switch (appState) {
    case STATE_MENU:     loopMenu();      break;
    case STATE_ADD_GLASS: loopAddGlass();  break;
    case STATE_POURING:  loopPouring();   break;
    case STATE_UNWEIGHED: loopUnweighed(); break;
    case STATE_FINISH:   loopFinish();    break;
    case STATE_OVERPOUR: loopOverpour();  break;
    case STATE_SETTINGS: loopSettings();  break;
  }
  delay(20);
}
