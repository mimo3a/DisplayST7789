#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

const char* WIFI_SSID = "REDACTED_WIFI_SSID";
const char* WIFI_PASS = "REDACTED_WIFI_PASSWORD";
const char* SERVER_URL = "https://api.garden.mimozalab.com/api/measurements";
TFT_eSPI tft = TFT_eSPI();

void syncClock()
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeInfo;
  for (int attempt = 0; attempt < 20; ++attempt) {
    if (getLocalTime(&timeInfo, 500)) {
      return;
    }
    delay(250);
  }
}

String getIsoTimestampUtc()
{
  time_t now;
  time(&now);

  if (now < 100000) {
    return "1970-01-01T00:00:00Z";
  }

  struct tm timeInfo;
  gmtime_r(&now, &timeInfo);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeInfo);
  return String(buffer);
}

void screenHeader(const String& text)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println(text);
}

void screenLine(const String& text, int y)
{
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, y);
  tft.println(text);
}

// ---------- HTTP POST ----------
int sendMeasurement(float temperature, float humidity)
{
  const String timestamp = getIsoTimestampUtc();
  String json =
    "{"
    "\"sensorId\":1,"
    "\"temperature\":" + String(temperature, 1) + ","
    "\"humidity\":" + String(humidity, 1) + ","
    "\"timestamp\":\"" + timestamp + "\""
    "}";

  WiFiClientSecure client;
client.setInsecure();
client.setTimeout(5000);

HTTPClient http;
http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

http.begin(client, SERVER_URL);
http.addHeader("Content-Type","application/json");

int code = http.POST(json);

  http.end();
  return code;
}


// ---------- GET CHECK ----------
int checkServerGet()
{
  WiFiClientSecure client;
client.setInsecure();
client.setTimeout(5000);

HTTPClient http;
http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

http.begin(client, SERVER_URL);

int code = http.GET();
  http.end();

  return code;
}


#ifdef TFT_BL
constexpr int8_t kTftBacklightPin = TFT_BL;
#else
constexpr int8_t kTftBacklightPin = -1;
#endif

void setup() {
  Serial.begin(115200);

  // --- UART from STM32 ---
#define RXD2 16
#define TXD2 17
Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  if (kTftBacklightPin >= 0) {
    pinMode(kTftBacklightPin, OUTPUT);
#ifdef TFT_BACKLIGHT_ON
    digitalWrite(kTftBacklightPin, TFT_BACKLIGHT_ON);
#else
    digitalWrite(kTftBacklightPin, HIGH);
#endif
  }

  tft.init();
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("HELLO WORLD!");

  WiFi.begin(WIFI_SSID,WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  syncClock();
  tft.setCursor(10, 40);
  tft.println("WiFi connected");
}

void loop() {

  // --- Проверка WiFi ---
  if (WiFi.status() != WL_CONNECTED) {
    screenHeader("ERROR");
    screenLine("WiFi Lost", 60);
    delay(2000);
    return;
  }

  // --- Receive data from STM32 ---
  if (Serial2.available()) {

    String line = Serial2.readStringUntil('\n');
    line.trim();

    screenHeader("DATA FROM STM");
    screenLine(line, 40);

    // Ожидаем формат: T:23.45;M:1234
    int tIndex = line.indexOf("T:");
    int mIndex = line.indexOf("M:");

    if (tIndex >= 0 && mIndex > tIndex) {

      float temperature = line.substring(tIndex + 2, line.indexOf(";", tIndex)).toFloat();
      float humidity = line.substring(mIndex + 2).toFloat();

      screenLine("Temp: " + String(temperature), 70);
      screenLine("Hum: " + String(humidity), 100);

      int postCode = sendMeasurement(temperature, humidity);

      screenLine("HTTP: " + String(postCode), 140);
    }

    delay(3000);
  }
}
 

//   // --- GET ---
//   int getCode = checkServerGet();

//   screenHeader("SERVER CHECK");
//   screenLine("GET code:",40);
//   screenLine(String(getCode),70);

//   delay(1500);

//   // --- POST ---
//   int postCode = sendMeasurement(temperature, humidity);

//   screenHeader("POST RESULT");

//   screenLine("Temp: "+String(temperature),40);
//   screenLine("Hum : "+String(humidity),70);

//   screenLine("HTTP:",120);
//   screenLine(String(postCode),150);

//   delay(5000);
// }