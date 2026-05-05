#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "SdFat.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA260.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

#ifdef BUILTIN_SDCARD
static const int SD_CS_PIN = BUILTIN_SDCARD;
#else
static const int SD_CS_PIN = 10;
#endif

#define FILE_BASE_NAME "INA260"
static const unsigned long LOG_INTERVAL_MS = 100;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_INA260 ina260;
SdFat sd;
File32 file;
uint32_t logTime = 0; //ms timestamp for log record

#define error(msg) sd.errorHalt(F(msg))

String formatTimestamp(unsigned long ms)
{
  unsigned long totalSeconds = ms / 1000;
  unsigned int hours = (totalSeconds / 3600);
  unsigned int minutes = (totalSeconds % 3600) / 60;
  unsigned int seconds = totalSeconds % 60;

  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, seconds);
  return String(buf);
}

bool initDisplay()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    return false;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("INA260 Logger");
  display.println("Initializing...");
  display.display();
  delay(500);
  return true;
}

void writeHeader()
{
  file.print(F("timestamp,voltage_v,current_a,power_w"));
  file.println();
}

void openLogFile()
{
  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "00.csv";

  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }

  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }

  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open");
  }

  Serial.print(F("Logging to: "));
  Serial.println(fileName);

  writeHeader();

  logTime = millis() / (LOG_INTERVAL_MS) + 1;
  logTime *= LOG_INTERVAL_MS;
}

bool initSD()
{
  if (!sd.begin(SD_CS_PIN, SD_SCK_MHZ(50))) {
    return false;
  }

  openLogFile();
  return true;
}

void updateDisplay(float voltage, float current, float power)
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("V: ");
  display.print(voltage, 2);
  display.print("V");

  display.setCursor(0, 16);
  display.print("I: ");
  display.print(current, 3);
  display.print("A");

  display.display();
}

void logMeasurement(const String &timestamp, float voltage, float current, float power)
{
  file.print(timestamp);
  file.write(',');
  file.print(voltage, 3);
  file.write(',');
  file.print(current, 3);
  file.write(',');
  file.println(power, 3);

  if (!file.sync() || file.getWriteError()) {
    error("write error");
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    ;
  }

  Serial.println("Starting INA260 + SSD1306 logger");

  Wire.begin();

  if (!initDisplay()) {
    Serial.println("SSD1306 not found");
    while (true) {
      delay(1000);
    }
  }

  if (!ina260.begin()) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("INA260 not found");
    display.display();
    Serial.println("Could not find INA260 sensor");
    while (1) ;
  }

  if (!initSD()) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SD init failed");
    display.display();
    Serial.println("SD card initialization failed");
    while (1) ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Startup complete");
  display.println("Logging to SD card");
  display.display();
  delay(1000);
}

void loop()
{
  logTime += LOG_INTERVAL_MS;

  int32_t diff;
  do {
    diff = (int32_t)(millis() - logTime);
  } while (diff < 0);

  if (diff > 20) {
    error("Missed data record");
  }

  float voltage = ina260.readBusVoltage() / 1000.0; // mV to V
  float current = ina260.readCurrent() / 1000.0; // mA to A
  float power = voltage * current; // W

  String timestamp = formatTimestamp(logTime);
  logMeasurement(timestamp, voltage, current, power);
  updateDisplay(voltage, current, power);

  Serial.print(timestamp);
  Serial.print(", V=");
  Serial.print(voltage, 3);
  Serial.print("V, I=");
  Serial.print(current, 3);
  Serial.print("A, P=");
  Serial.print(power, 3);
  Serial.println("W");

  if (Serial.available()) {
    file.close();
    Serial.println(F("Done"));
    while (1) {}
  }
}
