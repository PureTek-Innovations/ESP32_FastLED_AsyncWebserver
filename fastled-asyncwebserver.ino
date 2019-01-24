/*
   ESP32 FastLED WebServer: https://github.com/jasoncoon/esp32-fastled-webserver
   Copyright (C) 2017 Jason Coon

   Built upon the amazing FastLED work of Daniel Garcia and Mark Kriegsman:
   https://github.com/FastLED/FastLED

   ESP32 support provided by the hard work of Sam Guyer:
   https://github.com/samguyer/FastLED

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// wifi ssid and password should be added to a file in the sketch named secrets.h
// the secrets.h file should be added to the .gitignore file and never committed or
// pushed to public source control (GitHub).
// const char* ssid = "........";
// const char* password = "........";
//  #define TOKEN "token_............." // Set your Beebotte channel token


#include <FastLED.h>

#define IncludeRotaryEncoder    true
#define IncludeAudio            true  // only works with ESP32
#define IncludeMPU9250          true  // configured for ESP32
#define IncludeBeebotte         true  // does not work in AP mode, currently configured for ESP32

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#endif

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <EEPROM.h>

#if defined(IncludeRotaryEncoder) && (IncludeRotaryEncoder)
#include <Rotary.h>
#endif

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001008)
#warning "Requires FastLED 3.1.8 or later; check github for latest code."
#endif

AsyncWebServer webServer(80);


const int led = 2;

uint8_t autoplay = 0;
uint8_t autoplayDuration = 10;
unsigned long autoPlayTimeout = 0;

uint8_t currentPatternIndex = 0; // Index number of which pattern is current

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

uint8_t power = 1;

uint8_t brightnessMap[] = {2,4, 7, 12, 18, 27, 38, 51, 67, 86, 108, 134, 163, 197, 235, 255 };
uint8_t brightnessCount = ARRAY_SIZE(brightnessMap)-1;
uint8_t brightnessIndex = 5;
uint8_t brightness = brightnessMap[brightnessIndex];

uint8_t speed = 5;


// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
uint8_t cooling = 50;

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
uint8_t sparking = 120;

CRGB solidColor = CRGB::Blue;

uint8_t cyclePalettes = 0;
uint8_t paletteDuration = 10;
uint8_t currentPaletteIndex = 0;
unsigned long paletteTimeout = 0;
uint8_t patternTimer = 20;
uint8_t paletteTimer = 20;



#if defined(ESP8266)
#define DATA_PIN    D6
#define CLK_PIN   D6
#elif defined(ESP32)
#define DATA_PIN    25
#define CLK_PIN   5
#endif


#define LED_TYPE    WS2813
#define COLOR_ORDER BGR
#define NUM_STRIPS 1
#define NUM_LEDS_PER_STRIP 288
#define NUM_LEDS NUM_LEDS_PER_STRIP * NUM_STRIPS
CRGBArray<NUM_LEDS> leds;

#define MILLI_AMPS         10000 // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)
#define FRAMES_PER_SECOND  120

// -- The core to run FastLED.show()
#define FASTLED_SHOW_CORE 0

#include "patterns.h"
#include "field.h"
#include "fields.h"
#include "secrets.h"
#include "wifi.h"
#include "web.h"

#if defined(IncludeRotaryEncoder) && (IncludeRotaryEncoder)
#include "rotaryEncoder.h"
#endif

#if defined(IncludeAudio) && (IncludeAudio)
#include "audio.h"
#endif

#if defined(IncludeMPU9250) && (IncludeMPU9250)
#include "MPU9250.h"
#endif

#if defined(IncludeBeebotte) && (IncludeBeebotte)
#include "beebotte.h"
#endif

#if defined(ESP8266)
void listDir() {
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
  }
  Serial.printf("\n");
}

#elif defined(ESP32)



// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

/** show() for ESP32
    Call this function instead of FastLED.show(). It signals core 0 to issue a show,
    then waits for a notification that it is done.
*/
void FastLEDshowESP32()
{
  if (userTaskHandle == 0) {
    // -- Store the handle of the current task, so that the show task can
    //    notify it when it's done
    userTaskHandle = xTaskGetCurrentTaskHandle();

    // -- Trigger the show task
    xTaskNotifyGive(FastLEDshowTaskHandle);

    // -- Wait to be notified that it's done
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
    ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    userTaskHandle = 0;
  }
}

/** show Task
    This function runs on core 0 and just waits for requests to call FastLED.show()
*/
void FastLEDshowTask(void *pvParameters)
{
  // -- Run forever...
  for (;;) {
    // -- Wait for the trigger
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // -- Do the show (synchronously)
    FastLED.show();

    // -- Notify the calling task
    xTaskNotifyGive(userTaskHandle);
  }
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}


#endif




void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led, 1);

  //  delay(3000); // 3 second delay for recovery
  Serial.begin(115200);

  SPIFFS.begin();
#if defined(ESP8266)
  listDir();
  EEPROM.begin(512);
#elif defined(ESP32)
  listDir(SPIFFS, "/", 1);
#endif

  loadFieldsFromEEPROM(fields, fieldCount);

  setupWiFi();
#if defined(IncludeRotaryEncoder) && (IncludeRotaryEncoder)
  setupRotaryEncoder();
#endif

#if defined(IncludeAudio) && (IncludeAudio)
  setupAudio();
#endif

#if defined(IncludeMPU9250) && (IncludeMPU9250)
  setupMPU9250();
#endif

#if defined(IncludeBeebotte) && (IncludeBeebotte)
  setupBeebotte();
#endif


  // three-wire LEDs (WS2811, WS2812, NeoPixel)
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // four-wire LEDs (APA102, DotStar)
  //  FastLED.addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER, DATA_RATE_MHZ(1)>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // Parallel output: 13, 12, 27, 33, 15, 32, 14, SCL
  //  FastLED.addLeds<LED_TYPE, 13, COLOR_ORDER>(leds, 0, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE, 12, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE, 27, COLOR_ORDER>(leds, 2 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE, 33, COLOR_ORDER>(leds, 3 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE, 15, COLOR_ORDER>(leds, 4 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE, 32, COLOR_ORDER>(leds, 5 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE, 14, COLOR_ORDER>(leds, 6 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE, SCL, COLOR_ORDER>(leds, 7 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);

  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);

  // set master brightness control
  FastLED.setBrightness(brightness);
#if defined(ESP32)
  int core = xPortGetCoreID();
  Serial.print("Main code running on core ");
  Serial.println(core);

  // -- Create the FastLED show task
  xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);
#endif
  autoPlayTimeout = millis() + (autoplayDuration * 1000);
  paletteTimeout = millis() + (paletteDuration * 1000);
}

void loop()
{
  handleWeb();
#if defined(IncludeRotaryEncoder) && (IncludeRotaryEncoder)
  RotaryEncoder();
#endif

#if defined(IncludeBeebotte) && (IncludeBeebotte)
  loopBeebotte();
#endif

  if (power == 0) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
#if defined(ESP8266)
    FastLED.show();
#elif defined(ESP32)
    FastLEDshowESP32();
#endif
    delay(4); // allows the WiFi extra time
  }
  else {

#if defined(IncludeAudio) && (IncludeAudio)
    readAudio();
    buildAudioPalette();
#endif

#if defined(IncludeMPU9250) && (IncludeMPU9250)
    EVERY_N_MILLISECONDS(50) {
      get_MPU9250_data();
      build_compassPalette();
    }
#endif

    // Call the current pattern function once, updating the 'leds' array
    EVERY_N_MILLIS_I(patterntimer, patternTimer) {
      patterntimer.setPeriod(patternTimer);
      gHue += 1 + (speed / 3);
      // Call the current pattern function once, updating the 'leds' array
      patterns[currentPatternIndex].pattern();
      // send the 'leds' array out to the actual LED strip
#if defined(ESP8266)
      FastLED.show();
#elif defined(ESP32)
      FastLEDshowESP32();
#endif
    }

    // blend the current palette to the next
    EVERY_N_MILLIS_I(palettetimer, paletteTimer) {
      palettetimer.setPeriod( 64 - paletteDuration / 6);
      nblendPaletteTowardPalette(currentPalette, targetPalette, 8);
    }

    if (autoplay == 1 && (millis() > autoPlayTimeout)) {
      nextPattern(true);
      autoPlayTimeout = millis() + (autoplayDuration * 1000);
    }

    if (cyclePalettes == 1 && (millis() > paletteTimeout)) {
      nextPalette(true);
      paletteTimeout = millis() + (paletteDuration * 1000);
    }
    delay(4); // allows the WiFi extra time
  }
}

void nextPattern(bool up)
{
  // add one to the current pattern number, and wrap around at the end
  // skips the solid colour 'pattern'
  if (up) {
    currentPatternIndex = (currentPatternIndex + 1) % (patternCount - 1);
  } else {
    currentPatternIndex = (currentPatternIndex - 1) % (patternCount - 1);
  }
}

void nextPalette(bool up)
{
  if (up) {
    currentPaletteIndex = (currentPaletteIndex + 1) % paletteCount;
  } else {
    currentPaletteIndex = (currentPaletteIndex - 1) % paletteCount;
  }
  targetPalette = palettes[currentPaletteIndex];
}
