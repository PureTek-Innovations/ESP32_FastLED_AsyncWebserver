/*
   Modifed 2019 by Jeremy Spencer
   
   Ported from ESP32 FastLED WebServer: https://github.com/jasoncoon/esp32-fastled-webserver
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


/*
  MAX9814 MEMS Microphone
  VCC   5V
  GND   GND
  OUT   35
  gain  33
  AR    34
*/

/*
  Special thanks to Edgar Bonet for help fixing the micros() overflow https://arduino.stackexchange.com/a/12588
*/

// Portions of this file are adapted from the work of Stefan Petrick:
// https://plus.google.com/u/0/115124694226931502095

// Portions of this file are adapted from RGB Shades Audio Demo Code by Garrett Mace:
// https://github.com/macetech/RGBShadesAudio

#include "arduinoFFT.h" // Standard Arduino FFT library
#include <driver/i2s.h>

const i2s_port_t I2S_PORT = I2S_NUM_0;

arduinoFFT FFT = arduinoFFT();

// forward dwclarations
uint16_t XY( uint8_t, uint8_t);

// external variables
extern uint8_t cooling;
extern uint8_t  sparking;
//extern  CRGBPalette16 palettes[];

//// Pin definitions
//#define AudioPin 35
//#define gainPin 33
//#define ARpin   34

// Smooth/average settings
#define SPECTRUMSMOOTH 0.3 //0.08 0.15 bigger value = less smoothing
#define PEAKDECAY 0.01
#define NOISEFLOOR 65 //65

#define spectrumValueAttack 1.2
#define spectrumValueDecay 0.9

// AGC settings
#define AGCSMOOTH 0.004
#define GAINUPPERLIMIT 15.0 //15
#define GAINLOWERLIMIT 0.1

//FFT Variables
#define SAMPLES 1024
#define SAMPLING_FREQUENCY 40000
#define amplitude 200

//static const int spectrumAmplitude[7] = {300,100,100,100,100,100,100};
unsigned int sampling_period_us;
//unsigned long microseconds;
byte peak[] = {0, 0, 0, 0, 0, 0, 0, 0};
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime, oldTime;
int value[8];

// temp
int MinValue[8] = {500, 500, 500, 500, 500, 500, 500, 500};
int MaxValue[8];

// Global variables
CRGBArray < NUM_LEDS / 2 > audioLeds; // half the number of actual leds
unsigned int spectrumValue[8];  // holds raw adc values
float spectrumDecay[8] = {0};   // holds time-averaged values
float spectrumPeaks[8] = {0};   // holds peak values
float audioAvg = 270.0;
float gainAGC = 7.5;
int analogsum = 0;

uint8_t spectrumByte[8];        // holds 8-bit adjusted adc values

uint8_t spectrumAvg;


unsigned long currentMillis; // store current loop's millis value

void buildAudioPalette();
//void loadBeatDetectData();
//void printBeatDetect();
//bool bassDetect();
//bool midDetect();
//bool trebleDetect();


void setupAudio() {
esp_err_t err;

  // The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
      .sample_rate = 16000,                         // 16KHz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // although the SEL config should be left, it seems to transmit on right
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 4,                           // number of buffers
      .dma_buf_len = 8                              // 8 samples per buffer (minimum)
  };

  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 14,   // BCKL SCK
      .ws_io_num = 15,    // LRCL WS
      .data_out_num = -1, // not used (only for speakers)
      .data_in_num = 32   // DOUT SD
  };

  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }
  Serial.println("I2S driver installed.");
}

#include "readAudio.h"
void readAudio() {
  audioFFT();
   // store sum of values for AGC
  analogsum = 0;
  // filter noise
  for (int i = 0; i < 8; i++) {
    if (value[i] < NOISEFLOOR) {
      value[i] = 0;
    } else {
      value[i] = value[i] - NOISEFLOOR;
    }

    spectrumValue[i] = value[i];
    //    spectrumValue[i] = constrain(value[i],0,1024);
    // prepare average for AGC
    analogsum += spectrumValue[i];

    // apply current gain value
    spectrumValue[i] *= gainAGC;

    // process time-averaged values
    spectrumDecay[i] = (1.0 - SPECTRUMSMOOTH) * spectrumDecay[i] + SPECTRUMSMOOTH * spectrumValue[i];

    // process peak values
    if (spectrumPeaks[i] < spectrumDecay[i]) spectrumPeaks[i] = spectrumDecay[i];
    spectrumPeaks[i] = spectrumPeaks[i] * (1.0 - PEAKDECAY);

    spectrumByte[i] = spectrumValue[i] / 4;
    //temp
    if (spectrumValue[i] < MinValue[i])MinValue[i] = spectrumValue[i];
    if (spectrumValue[i] > MaxValue[i])MaxValue[i] = spectrumValue[i];

  }
  // Calculate audio levels for automatic gain
  audioAvg = (1.0 - AGCSMOOTH) * audioAvg + AGCSMOOTH * (analogsum / 8.0);
  spectrumAvg = (analogsum / 8.0) / 4;
  // Calculate gain adjustment factor
  gainAGC = 270.0 / audioAvg;
  if (gainAGC > GAINUPPERLIMIT) gainAGC = GAINUPPERLIMIT;
  if (gainAGC < GAINLOWERLIMIT) gainAGC = GAINLOWERLIMIT;

  // load the data into beatdection.
//  loadBeatDetectData();
//  bassDetect();
//  midDetect();
//  trebleDetect();

//  EVERY_N_SECONDS(10) {
//     printBeatDetect(); 
////    for (int n = 0; n < 8; n++) {
//////      Serial.println("Min Value: " + (String)n + " : " + (String)MinValue[n]);
////      Serial.println("Max Value: " + (String)n + " : " + (String)MaxValue[n]);
////    }
////    Serial.println("gainAGC: " + (String)gainAGC);
////    Serial.println(micros());
////    Serial.println();
//  }
}

//void animateBeats(){
//  leds.fadeToBlackBy(200);
//  if (bassDetect()){
//    leds(0, NUM_LEDS_PER_STRIP - 1) = CRGB::Red;
//  }
//  if (midDetect()){
//    leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) =CRGB::Blue;
//  }
//  if (trebleDetect()){
//    leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = CRGB::Green;
//  }
//
////  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
//}

// Attempt at beat detection
byte beatTriggered = 0;
#define beatLevel 20.0
#define beatDeadzone 30.0
#define beatDelay 50
float lastBeatVal = 0;
byte beatDetect() {
  currentMillis = millis();
  static float beatAvg = 0;
  static unsigned long lastBeatMillis;
  float specCombo = 0;
  if (spectrumDecay[0] > spectrumDecay[1]) {
    specCombo = spectrumDecay[0];
  } else {
    specCombo = spectrumDecay[1];
  }
  //  float specCombo = (spectrumDecay[0] + spectrumDecay[1]) / 2.0;
  beatAvg = (1.0 - AGCSMOOTH) * beatAvg + AGCSMOOTH * specCombo;
  if (lastBeatVal < beatAvg) lastBeatVal = beatAvg;
  if ((specCombo - beatAvg) > beatLevel && beatTriggered == 0 && currentMillis - lastBeatMillis > beatDelay) {
    beatTriggered = 1;
    lastBeatVal = specCombo;
    lastBeatMillis = currentMillis;
    return 1;
  } else if ((lastBeatVal - specCombo) > beatDeadzone) {
    beatTriggered = 0;
    return 0;
  } else {
    return 0;
  }
}

void fade_down(uint8_t value) {
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i].fadeToBlackBy(value);
  }
}

void spectrumPaletteWaves()
{
  //  fade_down(1);
  readAudio();
  int centerLed =  NUM_LEDS_PER_STRIP / 2;
  CRGB color6 = ColorFromPalette(palettes[currentPaletteIndex], spectrumByte[6], spectrumByte[6]);
  CRGB color5 = ColorFromPalette(palettes[currentPaletteIndex], spectrumByte[5] / 8, spectrumByte[5] / 8);
  CRGB color1 = ColorFromPalette(palettes[currentPaletteIndex], spectrumByte[1] / 2, spectrumByte[1] / 2);

  CRGB color = nblend(color6, color5, 256 / 8);
  color = nblend(color, color1, 256 / 2);

  leds[centerLed] = color;
  leds[centerLed].fadeToBlackBy(spectrumByte[3] / 12);

  leds[centerLed - 1] = color;
  leds[centerLed - 1].fadeToBlackBy(spectrumByte[3] / 12);

  //move to the left
  for (int i = NUM_LEDS_PER_STRIP - 1; i > centerLed; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < centerLed; i++) {
    leds[i] = leds[i + 1];
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

void spectrumPaletteWaves2()
{
  //  fade_down(1);
  int centerLed =  NUM_LEDS_PER_STRIP / 2;
  readAudio();
  CRGBPalette16 palette = palettes[currentPaletteIndex];

  CRGB color6 = ColorFromPalette(palette, 255 - spectrumByte[6], spectrumByte[6]);
  CRGB color5 = ColorFromPalette(palette, 255 - spectrumByte[5] / 8, spectrumByte[5] / 8);
  CRGB color1 = ColorFromPalette(palette, 255 - spectrumByte[1] / 2, spectrumByte[1] / 2);

  CRGB color = nblend(color6, color5, 256 / 8);
  color = nblend(color, color1, 256 / 2);

  leds[centerLed] = color;
  leds[centerLed].fadeToBlackBy(spectrumByte[3] / 12);

  leds[centerLed - 1] = color;
  leds[centerLed - 1].fadeToBlackBy(spectrumByte[3] / 12);

  //move to the left
  for (int i = NUM_LEDS_PER_STRIP - 1; i > centerLed; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < centerLed; i++) {
    leds[i] = leds[i + 1];
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

void spectrumWaves()
{
  readAudio();
  fade_down(2);
  int centerLed =  NUM_LEDS_PER_STRIP / 2;
  CRGB color = CRGB(spectrumByte[6], spectrumByte[5] / 8, spectrumByte[1] / 2);

  leds[centerLed] = color;
  leds[centerLed].fadeToBlackBy(spectrumByte[3] / 12);

  leds[centerLed - 1] = color;
  leds[centerLed - 1].fadeToBlackBy(spectrumByte[3] / 12);

  //move to the left
  for (int i = NUM_LEDS_PER_STRIP - 1; i > centerLed; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < centerLed; i++) {
    leds[i] = leds[i + 1];
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

void spectrumWaves2()
{
  readAudio();
  fade_down(2);
  int centerLed =  NUM_LEDS_PER_STRIP / 2;
  CRGB color = CRGB(spectrumByte[5] / 8, spectrumByte[6], spectrumByte[1] / 2);

  leds[centerLed] = color;
  leds[centerLed].fadeToBlackBy(spectrumByte[3] / 12);

  leds[centerLed - 1] = color;
  leds[centerLed - 1].fadeToBlackBy(spectrumByte[3] / 12);

  //move to the left
  for (int i = NUM_LEDS_PER_STRIP - 1; i > centerLed; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < centerLed; i++) {
    leds[i] = leds[i + 1];
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

void spectrumWaves3()
{
  readAudio();
  fade_down(2);
  int centerLed =  NUM_LEDS_PER_STRIP / 2;
  CRGB color = CRGB(spectrumByte[1] / 2, spectrumByte[5] / 8, spectrumByte[6]);

  leds[centerLed] = color;
  leds[centerLed].fadeToBlackBy(spectrumByte[3] / 12);

  leds[centerLed - 1] = color;
  leds[centerLed - 1].fadeToBlackBy(spectrumByte[3] / 12);

  //move to the left
  for (int i = NUM_LEDS_PER_STRIP - 1; i > centerLed; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < centerLed; i++) {
    leds[i] = leds[i + 1];
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

void analyzerColumnsOriginal()
{
//  readAudio();
  //  fill_solid(leds, NUM_LEDS, CRGB::Black);
  fade_down(100);

  const int columnSize = NUM_LEDS_PER_STRIP / 7;

  for (uint8_t i = 0; i < 7; i++) {
    int columnStart = i * columnSize;
    int columnEnd = columnStart + columnSize;

    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

    int columnHeight = map8(spectrumByte[i], 1, columnSize);

    for (int j = columnStart; j < columnStart + columnHeight; j++) {
      if (j >= NUM_LEDS || j >= columnEnd)
        continue;

      leds[j] = CHSV(i * 40, 255, 255);
    }
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}
void analyzerColumns()
{
//  readAudio();
  //    fill_solid(leds, NUM_LEDS, CRGB::Black);
  fade_down(175);

  const int columnSize = NUM_LEDS / 8;
  static int columnSpectrumValue[4];
  const int binNumber[] = {1, 3, 4, 5};

  for (uint8_t i = 0; i < 4; i++) {
    int columnStart = i * columnSize;
    int columnEnd = columnStart + columnSize;

    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

    //    int columnHeight = constrain(map(spectrumValue[i + 2],0,1024, 0, columnSize),0,columnSize);
    //    int columnHeight = constrain(map(spectrumDecay[i + 2],0,1024, 0, columnSize),0,columnSize);

    int columnHeight = constrain(map(spectrumValue[binNumber[i]], 0, 1024, 0, columnSize + 1), 0, columnSize + 1);
    if (columnHeight < columnSpectrumValue[i]) {
      if (columnHeight < columnSpectrumValue[i] * 0.85) {
        columnHeight = columnSpectrumValue[i] * 0.85;
      } else {
        columnHeight = columnSpectrumValue[i];
      }
      columnSpectrumValue[i] = columnHeight;
    } else {
      columnSpectrumValue[i] = columnHeight;
    }

    for (int j = 0; j < columnHeight; j++) {
      leds[((i * 2 + 1) * NUM_LEDS_PER_STRIP / 2) - j] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255), 255); //CHSV(i * 40, 255, 255);
      leds[((i * 2 + 1)*NUM_LEDS_PER_STRIP / 2) + j - 1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255), 255); //CHSV(i * 40, 255, 255);
    }
  }


  //  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  //  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  //  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

void analyzerColumns1()
{
//  readAudio();
  fade_down(175);
  const int columnSize = NUM_LEDS / 16;
  static int columnSpectrumValue[8];
  //  const int binNumber[] = {0, 1, 2, 3,3 , 4, 5, 6};
  const int binNumber[] = {1, 1, 3, 3, 4 , 4, 5, 5};

  for (uint8_t i = 0; i < 8; i++) {

    int columnHeight = constrain(map(spectrumValue[binNumber[i]], 0, 1024, 0, columnSize + 1 ), 0, columnSize + 1);
    if (columnHeight < columnSpectrumValue[i]) {
      if (columnHeight < columnSpectrumValue[i] * 0.85) {
        columnHeight = columnSpectrumValue[i] * 0.85;// down slowly
      } else {
        columnHeight = columnSpectrumValue[i];
      }
      columnSpectrumValue[i] = columnHeight;
    } else {
      columnSpectrumValue[i] = columnHeight; // up quickly
    }
    // need to ensure it fits on the leds!!! both go out of range, i goes up to 7
    if (i % 2) { // odd 1,3,5,7
      for (int j = 0; j < columnHeight; j++) {
        leds[((i  ) * NUM_LEDS_PER_STRIP / 2) - j] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255), 255); //CHSV(i * 40, 255, 255);
        leds[((i  )*NUM_LEDS_PER_STRIP / 2) + j - 1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255), 255); //CHSV(i * 40, 255, 255);
      }
    } else { // even 0,2,4,6
      for (int j = 0; j < columnHeight; j++) {
        leds[(i * NUM_LEDS_PER_STRIP / 2) + j] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255), 255); //CHSV(i * 40, 255, 255);
        leds[((i  + 2)*NUM_LEDS_PER_STRIP / 2) - j - 1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255), 255); //CHSV(i * 40, 255, 255);
      }
    }
  }
}

void analyzerColumns2()
{
  fade_down(175);
  const int columnSize = NUM_LEDS / 16;
  static int columnSpectrumValue[8];
  //  const int binNumber[] = {0, 1, 2, 3,3 , 4, 5, 6};
  const int binNumber[] = {1, 1, 3, 3, 4 , 4, 5, 5};

  for (uint8_t i = 0; i < 8; i++) {

    int columnHeight = constrain(map(spectrumValue[binNumber[i]], 0, 1024, 0, columnSize + 1), 0, columnSize + 1);
    if (columnHeight < columnSpectrumValue[i]) {
      if (columnHeight < columnSpectrumValue[i] * 0.85) {
        columnHeight = columnSpectrumValue[i] * 0.85;// down slowly
      } else {
        columnHeight = columnSpectrumValue[i];
      }
      columnSpectrumValue[i] = columnHeight;
    } else {
      columnSpectrumValue[i] = columnHeight; // up quickly
    }
    if (i % 2) { // odd 1,3,5,7
      for (int j = 0; j < columnHeight; j++) {
        leds[((i  ) * NUM_LEDS_PER_STRIP / 2) - (NUM_LEDS_PER_STRIP / 4) + j-1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) + gHue, 255);
        leds[((i  )*NUM_LEDS_PER_STRIP / 2) + (NUM_LEDS_PER_STRIP / 4) - j - 2] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) + gHue, 255);
      }
    } else { // even 0,2,4,6
      for (int j = 0; j < columnHeight; j++) {
        leds[(i * NUM_LEDS_PER_STRIP / 2) + (NUM_LEDS_PER_STRIP / 4) - j-1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) + gHue, 255);
        leds[((i  + 2)*NUM_LEDS_PER_STRIP / 2) - (NUM_LEDS_PER_STRIP / 4) + j - 2] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) + gHue, 255);
      }
    }
  }
}

void analyzerColumns3()
{
//  readAudio();
//  buildAudioPalette();
  fade_down(175);
  const int columnSize = NUM_LEDS / 16;
  static int columnSpectrumValue[8];
  //  const int binNumber[] = {0, 1, 2, 3,3 , 4, 5, 6};
  const int binNumber[] = {1, 1, 3, 3, 4 , 4, 5, 5};
//  for (uint8_t i = 0; i < 4; i++) {
//    
//  }
//  CRGBPalette16    audioPalette = CRGBPalette16(
//                                    CHSV( constrain(map(spectrumValue[1], 0, 1024, 0, 255), 0, 255), 255, 255),
//                                    CHSV( constrain(map(spectrumValue[3], 0, 1024, 0, 255)+32, 0, 255), 255, 255),
//                                    CHSV( constrain(map(spectrumValue[4], 0, 1024, 0, 255)+64, 0, 255), 255, 255),
//                                    CHSV( constrain(map(spectrumValue[5], 0, 1024, 0, 255)+128, 0, 255), 255, 255));

  for (uint8_t i = 0; i < 8; i++) {

    int columnHeight = constrain(map(spectrumValue[binNumber[i]], 0, 1024, 0, columnSize + 1), 0, columnSize + 1);
    if (columnHeight < columnSpectrumValue[i]) {
      if (columnHeight < columnSpectrumValue[i] * 0.85) {
        columnHeight = columnSpectrumValue[i] * 0.85;// down slowly
      } else {
        columnHeight = columnSpectrumValue[i];
      }
      columnSpectrumValue[i] = columnHeight;
    } else {
      columnSpectrumValue[i] = columnHeight; // up quickly
    }
    if (i % 2) { // odd 1,3,5,7
      for (int j = 0; j < columnHeight; j++) {
        leds[((i  ) * NUM_LEDS_PER_STRIP / 2) - (NUM_LEDS_PER_STRIP / 4) + j-1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255)+ gHue, 255);
        leds[((i  )*NUM_LEDS_PER_STRIP / 2) + (NUM_LEDS_PER_STRIP / 4) - j - 2] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255)+ gHue, 255);
      }
    } else { // even 0,2,4,6
      for (int j = 0; j < columnHeight; j++) {
        leds[(i * NUM_LEDS_PER_STRIP / 2) + (NUM_LEDS_PER_STRIP / 4) - j-1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255)+ gHue, 255);
        leds[((i  + 2)*NUM_LEDS_PER_STRIP / 2) - (NUM_LEDS_PER_STRIP / 4) + j - 2] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255)+ gHue, 255); 
      }
    }
  }
}

void analyzerColumns4(){
  static uint8_t colourSpectrumValue[8];
  CRGB spectrumColour[8];
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t colourValue = constrain(map(spectrumDecay[i], 0, 1024, 0, 255), 0, 255);
    if (colourValue < colourSpectrumValue[i]) {
      if (colourValue < colourSpectrumValue[i] * 0.85) {
        colourValue = colourSpectrumValue[i] * 0.85;// down slowly
      } else {
        colourValue = colourSpectrumValue[i];
      }
      colourSpectrumValue[i] = colourValue;
    } else {
      colourSpectrumValue[i] = colourValue; // up quickly
    }
    //    colourSpectrumValue[i] += i*32; // ensure a range of different colours at 0 readings
    //    spectrumColour[i]= CHSV( colourSpectrumValue[i] + (i*8), 255, 255);
    spectrumColour[i] = CHSV( colourSpectrumValue[i], 255, 255);
  }
  CRGBPalette16 tempAudioPalette;
  fade_down(175);
  const int columnSize = NUM_LEDS / 16;
  static int columnSpectrumValue[8];
  //  const int binNumber[] = {0, 1, 2, 3,3 , 4, 5, 6};
  const int binNumber[] = {1, 1, 3, 3, 4 , 4, 5, 5};

  for (uint8_t i = 0; i < 8; i++) {

    int columnHeight = constrain(map(spectrumValue[binNumber[i]], 0, 1024, 0, columnSize + 1), 0, columnSize + 1);
    if (columnHeight < columnSpectrumValue[i]) {
      if (columnHeight < columnSpectrumValue[i] * 0.85) {
        columnHeight = columnSpectrumValue[i] * 0.85;// down slowly
      } else {
        columnHeight = columnSpectrumValue[i];
      }
      columnSpectrumValue[i] = columnHeight;
    } else {
      columnSpectrumValue[i] = columnHeight; // up quickly
    }
    if (i<2){
      tempAudioPalette = CRGBPalette16(spectrumColour[0], spectrumColour[1]);
    } else if (i<4){
      tempAudioPalette = CRGBPalette16(spectrumColour[2], spectrumColour[3]);
    } else if (i<6){
      tempAudioPalette = CRGBPalette16(spectrumColour[4], spectrumColour[5]);
    } else {
      tempAudioPalette = CRGBPalette16(spectrumColour[6], spectrumColour[7]);
    }
    if (i % 2) { // odd 1,3,5,7
      for (int j = 0; j < columnHeight; j++) {
        leds[((i  ) * NUM_LEDS_PER_STRIP / 2) - (NUM_LEDS_PER_STRIP / 4) + j-1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) , 255);
        leds[((i  )*NUM_LEDS_PER_STRIP / 2) + (NUM_LEDS_PER_STRIP / 4) - j - 2] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) , 255);
      }
    } else { // even 0,2,4,6
      for (int j = 0; j < columnHeight; j++) {
        leds[(i * NUM_LEDS_PER_STRIP / 2) + (NUM_LEDS_PER_STRIP / 4) - j-1] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) , 255);
        leds[((i  + 2)*NUM_LEDS_PER_STRIP / 2) - (NUM_LEDS_PER_STRIP / 4) + j - 2] = ColorFromPalette(palettes[currentPaletteIndex], map(j, 0, columnHeight, 0, 255) , 255);
      }
    }
  }
}
void audioRainbow(){
  static uint8_t colourSpectrumValue[8];
  CRGB spectrumColour[8];
  CRGBPalette16 tempPalette;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t colourValue = constrain(map(spectrumDecay[i], 0, 1024, 0, 255), 0, 255);
    if (colourValue < colourSpectrumValue[i]) {
      if (colourValue < colourSpectrumValue[i] * 0.85) {
        colourValue = colourSpectrumValue[i] * 0.85;// down slowly
      } else {
        colourValue = colourSpectrumValue[i];
      }
      colourSpectrumValue[i] = colourValue;
    } else {
      colourSpectrumValue[i] = colourValue; // up quickly
    }
    if (colourSpectrumValue[i]>0){
    spectrumColour[i]= CHSV( colourSpectrumValue[i], 255, colourSpectrumValue[i]);
    } else {
      spectrumColour[i]= CRGB::Black;
    }
  }

tempPalette = CRGBPalette16(spectrumColour[0],spectrumColour[0],spectrumColour[1],spectrumColour[1],
                                  spectrumColour[2],spectrumColour[2],spectrumColour[3],spectrumColour[3],
                                 spectrumColour[4],spectrumColour[4],spectrumColour[5],spectrumColour[5],
                                 spectrumColour[6],spectrumColour[6],spectrumColour[7],spectrumColour[7]);
    
   for (uint8_t i = 0; i < NUM_STRIPS; i++) {
        for (int j = 0; j < NUM_LEDS_PER_STRIP; j++) {
        leds[j+(i*NUM_LEDS_PER_STRIP)] = ColorFromPalette(tempPalette, map(j, 0, NUM_LEDS_PER_STRIP/2, 0, 255), 255);
    }
  }
}


void buildAudioPalette(){
  static uint8_t colourSpectrumValue[8];
  CRGB spectrumColour[8]; 
  #define upMultiplier 1.05
  #define downMultipier 0.95
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t colourValue = constrain(map(spectrumDecay[i], 0, 1024, 0, 255), 0, 255);
    colourSpectrumValue[i] = (colourSpectrumValue[i]+colourValue)/2;
//    if (colourValue < colourSpectrumValue[i]) {
//      if (colourValue < colourSpectrumValue[i] * downMultipier) {
//        colourSpectrumValue[i] = colourSpectrumValue[i] * downMultipier;// down slowly
//      } else {
//        colourSpectrumValue[i] = colourValue;
//      }
//    } else if (colourValue > colourSpectrumValue[i] * upMultiplier){
//      colourSpectrumValue[i] = colourSpectrumValue[i] * upMultiplier;
//    } else {
//      colourSpectrumValue[i] = colourValue;
//    }
    spectrumColour[i] = CHSV( colourSpectrumValue[i], 255, 255);
  }
    palettes[1] = CRGBPalette16(spectrumColour[0],spectrumColour[0],spectrumColour[1],spectrumColour[1],
                                  spectrumColour[2],spectrumColour[2],spectrumColour[3],spectrumColour[3],
                                 spectrumColour[4],spectrumColour[4],spectrumColour[5],spectrumColour[5],
                                 spectrumColour[6],spectrumColour[6],spectrumColour[7],spectrumColour[7]);
  palettes[2] = CRGBPalette16(spectrumColour[0], spectrumColour[1], spectrumColour[2], spectrumColour[3]);
                              
  palettes[3] = CRGBPalette16(spectrumColour[2], spectrumColour[3], spectrumColour[4], spectrumColour[5]);

  palettes[4] = CRGBPalette16(spectrumColour[4], spectrumColour[5], spectrumColour[6], spectrumColour[7]); 
}


void analyzerPeakColumns()
{
  readAudio();
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  const int columnSize = NUM_LEDS / 7;

  for (uint8_t i = 0; i < 7; i++) {
    int columnStart = i * columnSize;
    int columnEnd = columnStart + columnSize;

    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

    int columnHeight = map(spectrumValue[i], 0, 1023, 0, columnSize);
    int peakHeight = map(spectrumPeaks[i], 0, 1023, 0, columnSize);

    for (int j = columnStart; j < columnStart + columnHeight; j++) {
      if (j < NUM_LEDS && j <= columnEnd) {
        leds[j] = CHSV(i * 40, 255, 128);
      }
    }

    int k = columnStart + peakHeight;
    if (k < NUM_LEDS && k <= columnEnd)
      leds[k] = CHSV(i * 40, 255, 255);
  }
}

void beatWaves()
{
  readAudio();
  fade_down(20);
  int centerLed =  NUM_LEDS_PER_STRIP / 2;
  if (beatDetect()) {
    leds[centerLed] = CRGB::Red;
  }

  //move to the left
  for (int i = NUM_LEDS_PER_STRIP - 1; i > centerLed; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < centerLed; i++) {
    leds[i] = leds[i + 1];
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}


#define VUFadeFactor 4 //5
#define VUScaleFactor 2.0 //2.0
#define VUPaletteFactor 0.5 //1.5
void drawVU() {
  readAudio();
  fade_down(150); //fade_down(150);
  CRGB pixelColor;
  int centerLed =  NUM_LEDS_PER_STRIP / 2;
  const float xScale = 255.0 / (NUM_LEDS_PER_STRIP / 2);
  float specCombo = (spectrumDecay[0] + spectrumDecay[1] + spectrumDecay[2] + spectrumDecay[3]) / 4.0;

  for (int x = 0; x < NUM_LEDS_PER_STRIP / 2; x++) {
    int senseValue = specCombo / VUScaleFactor - xScale * x;
    int pixelBrightness = senseValue * VUFadeFactor;
    if (pixelBrightness > 240) pixelBrightness = 255;
    if (pixelBrightness < 0) pixelBrightness = 0;

    int pixelPaletteIndex = senseValue / VUPaletteFactor - 15;
    if (pixelPaletteIndex > 255) pixelPaletteIndex = 240;
    if (pixelPaletteIndex < 0) pixelPaletteIndex = 0;

    //        pixelColor = ColorFromPalette(palettes[currentPaletteIndex], pixelPaletteIndex, pixelBrightness);
    pixelColor = ColorFromPalette(palettes[currentPaletteIndex], x * 255 / (NUM_LEDS_PER_STRIP / 2), pixelBrightness);

    leds[centerLed + x] += pixelColor;
    leds[centerLed - x - 1] += pixelColor;
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

void drawVU2() {
  readAudio();
  fade_down(150);
  int avg = map8(spectrumAvg, 0, NUM_LEDS_PER_STRIP / 2 - 1);
  //  int avg = map8(analogsum, 0, NUM_LEDS_PER_STRIP/2 - 1);
  for (int i = 0; i < NUM_LEDS_PER_STRIP / 2; i++) {
    if (i <= avg) {
      leds[i] += ColorFromPalette(palettes[currentPaletteIndex], (240 / NUM_LEDS_PER_STRIP) * i);
    }
    //    else {
    //      leds[i] = CRGB::Black;
    //    }
  }
  leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP * 2 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP * 3 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
  leds(NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP * 4 - 1) = leds(0, NUM_LEDS_PER_STRIP - 1);
}

//void audioFire()
//{
//  readAudio();
//
//  uint8_t yawHue = map(yaw, yawMin, yawMax, 0, 255);
//  uint8_t pitchHue = map(pitch, pitchMin, pitchMax, 0, 255) - 64;
//  uint8_t rollHue = map(roll, rollMin, rollMax, 0, 255) + 64;
//
//  CRGB darkcolor  = CHSV(yawHue, 255, 255);
//  CRGB lightcolor = CHSV(pitchHue, 192, 192);
//  CRGB whitecolor = CHSV(rollHue, 128, 255);
//  CRGBPalette16 audioFirePal = CRGBPalette16( CRGB::Black, darkcolor, lightcolor, whitecolor);
//
//  float specComboLow = (spectrumDecay[0] + spectrumDecay[1] + spectrumDecay[2] + spectrumDecay[3]) / 4.0; // map to cooling 10-120
//  int audioSparking = map(specComboLow, 0, 800, 40, 150);
//  float specComboHigh = (spectrumDecay[4] + spectrumDecay[5] + spectrumDecay[6] ) / 3.0; // map to sparking 10-150
//  int audioCooling = map(specComboHigh, 0, 800, 80, 10);
//
//
//  // Array of temperature readings at each simulation cell
//  static byte heat[NUM_STRIPS * 2][NUM_LEDS_PER_STRIP / 2];
//  for (int n = 0; n < NUM_STRIPS * 2 ; n++) {
//    // Step 1.  Cool down every cell a little
//    for ( int i = 0; i < (NUM_LEDS_PER_STRIP / 2 - 1); i++) {
//      heat[n][i] = qsub8( heat[n][i],  random8(0, ((audioCooling * 10) / (NUM_LEDS_PER_STRIP / 4)) + 2));
//    }
//
//    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
//    for ( int k = (NUM_LEDS_PER_STRIP  / 2); k >= 2; k--) {
//      heat[n][k] = (heat[n][k - 1] + heat[n][k - 2] + heat[n][k - 2] ) / 3;
//    }
//
//    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
//    if ( random8() < audioSparking ) {
//      int y = random8(7);
//      heat[n][y] = qadd8( heat[n][y], random8(160, 255) );
//    }
//
//    // Step 4.  Map from heat cells to LED colors
//    for ( int j = 0; j < (NUM_LEDS_PER_STRIP / 2); j++) {
//      // Scale the heat value from 0-255 down to 0-240
//      // for best results with color palettes.
//      byte colorindex = scale8( heat[n][j], 240);
//      CRGB color = ColorFromPalette( audioFirePal, colorindex);
//      if (n % 2) {
//        leds[(n * NUM_LEDS_PER_STRIP / 2) + j] = color;
//      } else {
//        leds[((n + 1)*NUM_LEDS_PER_STRIP / 2) - j - 1] = color;
//      }
//    }
//  }
//}

// Params for width and height
const uint8_t kMatrixWidth = 4;
const uint8_t kMatrixHeight = 74;
int lastValue[kMatrixWidth];


uint16_t y[2];
uint16_t x[2];
uint16_t z[2];
uint16_t scale_x[2];
uint16_t scale_y[2];
uint32_t CentreX = 0;//kMatrixWidth/2;
uint32_t CentreY = 0;//kMatrixHeight/2;
uint32_t noiseData[2][kMatrixWidth][kMatrixHeight];
CRGBArray<NUM_LEDS> buffer;

// as shown on youtube
void noise_audio2() {
  readAudio();
  fade_down(175);
  //  CRGBPalette16 Pal = CRGBPalette16( CRGB::Black, CRGB::Gold, CRGB::OrangeRed,  CRGB::Red);//HeatColors_p;//( pit3 ); // the red one
  //  CRGBPalette16 Pal = CRGBPalette16(CRGB::Red, CRGB::Black, CRGB::OrangeRed, CRGB::Black);//HeatColors_p;//( pit3 ); // the red one
  CRGBPalette16 Pal = CRGBPalette16(CRGB::Red, CRGB::Black, CRGB::Black, CRGB::Black);//( pit3 ); // the red one
  static uint16_t y_0 = spectrumDecay[4]; //(y_0+spectrumDecay[4])/2;
  static uint16_t x_0 = spectrumDecay[1]; //(x_0+spectrumDecay[1])/2;
  y[0] += (y_0 - 10) * 4;
  scale_x[0] = 10000 - (x_0 * 40);
  scale_y[0] = scale_x[0];
  byte layer = 0;
  for (uint8_t i = 0; i < kMatrixWidth; i++) {
    uint32_t ioffset = scale_x[layer] * (i - CentreX);
    for (uint8_t j = 0; j < kMatrixHeight; j++) {
      uint32_t joffset = scale_y[layer] * (j - CentreY);
      uint16_t data = inoise16(x[layer] + ioffset, y[layer] + joffset, z[layer]) * 1.55;
      //      Serial.println(data);
      // results in the range of 11k - 51k
      // limit results:
      if (data < 11000) data = 11000;
      if (data > 51000) data = 51000;
      // normalize data to a range of 0 - 40000
      data = data - 11000;
      // scale down
      data = data / 161;
      noiseData[layer][i][j] = data;
    }
  }


  // map 1st layer
  for (uint8_t y = 0; y < kMatrixHeight; y++) {
    for (uint8_t x = 0; x < kMatrixWidth; x++) {
      leds[XY(x, y)] = ColorFromPalette( Pal, (millis() / 130) + noiseData[0][x][y]);
    }
  }

  //2nd layer
  //  CRGBPalette16 Pal2 = CRGBPalette16( CRGB::Black, CRGB::DarkTurquoise, CRGB::Aqua,  CRGB::Blue);//( pit4 ); // the blue one
  CRGBPalette16 Pal2 = CRGBPalette16( CRGB::Blue, CRGB::Black, CRGB::Black, CRGB::Black); //( pit4 ); // the blue one
  static uint16_t y_1 = spectrumDecay[5]; //(y_1+spectrumDecay[5])/2;
  static uint16_t x_1 = spectrumDecay[3]; //(x_1+spectrumDecay[2])/2;
  y[1] -= (y_1 - 10) * 4;
  //z[1] += 9;
  scale_x[1] = 10000 - (x_1 * 40);
  scale_y[1] = scale_x[1];
  layer = 1;
  for (uint8_t i = 0; i < kMatrixWidth; i++) {
    uint32_t ioffset = scale_x[layer] * (i - CentreX);
    for (uint8_t j = 0; j < kMatrixHeight; j++) {
      uint32_t joffset = scale_y[layer] * (j - CentreY);
      uint16_t data = inoise16(x[layer] + ioffset, y[layer] + joffset, z[layer]) * 1.5;
      // results in the range of 11k - 51k
      // limit results:
      if (data < 11000) data = 11000;
      if (data > 51000) data = 51000;
      // normalize data to a range of 0 - 40000
      data = data - 11000;
      // scale down
      data = data / 161;
      noiseData[layer][i][j] = data;
    }
  }
  for (uint8_t y = 0; y < kMatrixHeight; y++) {
    for (uint8_t x = 0; x < kMatrixWidth; x++) {
      // map 2nd layer
      //      buffer[XY(x, y)] = ColorFromPalette( Pal2, noiseData[1][x][y]);//(value[2] / 4) + noiseData[1][x][y]);
      buffer[XY(x, y)] = ColorFromPalette(Pal2, (spectrumDecay[2] / 4) + noiseData[1][x][y]);
      // add both layers
      leds[XY(x, y)] = buffer[XY(x, y)] + leds[XY(x, y)];
    }
  }
  //  adjust_gamma();
  //  FastLED.show();
}

boolean kMatrixSerpentineLayout = false;

void audioPlasma() {
  readAudio();
  static byte offset  = 0; // counter for radial color wave motion
  static int plasVector = 0; // counter for orbiting plasma center

  // startup tasks
  //  if (effectInit == false) {
  //    effectInit = true;
  //    effectDelay = 10;
  //    selectRandomAudioPalette();
  //    audioActive = true;
  //    fadeActive = 0;
  //  }

  // Calculate current center of plasma pattern (can be offscreen)
  int xOffset = (cos8(plasVector / 256) - 127) / 2;
  int yOffset = (sin8(plasVector / 256) - 127) / 2;

  //int xOffset = 0;
  //int yOffset = 0;


  // Draw one frame of the animation into the LED array
  for (int x = 0; x < kMatrixWidth; x++) {
    for (int y = 0; y < kMatrixHeight; y++) {
      byte color = sin8(sqrt(sq(((float)x - 7.5) * 12 + xOffset) + sq(((float)y - 2) * 12 + yOffset)) + offset);
      leds[XY(x, y)] = ColorFromPalette(palettes[currentPaletteIndex], color, 255);
    }
  }

  offset++; // wraps at 255 for sin8
  plasVector += (spectrumDecay[1] + spectrumDecay[2] + spectrumDecay[3]) / 3; // using an int for slower orbit (wraps at 65536) //spectrumValue
  //  plasVector += (spectrumValue[1] + spectrumValue[2] + spectrumValue[3])/3;
} //audioPlasma()


uint16_t XY( uint8_t x, uint8_t y) {
  uint16_t i;

  if ( kMatrixSerpentineLayout == false) {
    i = (y * kMatrixWidth) + x;
  }

  if ( kMatrixSerpentineLayout == true) {
    if ( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (kMatrixWidth - 1) - x;
      i = (y * kMatrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * kMatrixWidth) + x;
    }
  }
  return i;
}//XY( uint8_t x, uint8_t y)

void audioSparkle() {
  readAudio();
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[1] + spectrumValue[2] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo / 50; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkle()

void audioSparkleBass1() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[1] + spectrumValue[2] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleBass1()

void audioSparkleBass2() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[1] + spectrumValue[2] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo/25; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleBass2()

void audioSparkleBass3() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[1] + spectrumValue[2] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo/50; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleBass3()

void audioSparkleMid1() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[3] + spectrumValue[4] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleMid1()

void audioSparkleMid2() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[3] + spectrumValue[4] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo / 25; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleMid2()

void audioSparkleMid3() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[3] + spectrumValue[4] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo / 50; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleMid3()

void audioSparkleTreb1() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[5] + spectrumValue[6] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleTreb1()

void audioSparkleTreb2() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[5] + spectrumValue[6] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo / 25; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleTreb2()

void audioSparkleTreb3() {
  fadeToBlackBy( leds, NUM_LEDS, 128);
  float specCombo = ( spectrumValue[5] + spectrumValue[6] ) / 2.0;
  //    float specCombo = (spectrumValue[0] + spectrumValue[1] + spectrumValue[2] + spectrumValue[3]) / 4.0;
  for (int i = 0; i < specCombo / 50; i++) {
    leds[random16(NUM_LEDS)] = ColorFromPalette(palettes[currentPaletteIndex], random8(255) , 255, LINEARBLEND);
  }
} // audioSparkleTreb3()
