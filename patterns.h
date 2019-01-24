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

#include "palettes.h";
#include "twinkleFOX.h"
#include "plasma.h";

#if defined(IncludeMPU9250) && (IncludeMPU9250)
void yawPitchRoll();
#endif

#if defined(IncludeAudio) && (IncludeAudio)
void  analyzerColumns();
void  analyzerColumns1();
void  analyzerColumns2();
void  analyzerColumns3();
void  audioSparkleBass1();
void  audioSparkleBass2();
void  audioSparkleBass3();
void  audioSparkleMid1();
void  audioSparkleMid2();
void  audioSparkleMid3();
void  audioSparkleTreb1();
void  audioSparkleTreb2();
void  audioSparkleTreb3();
#endif

void rainbow()
{
  // FastLED's built-in rainbow generator
  patternTimer = 30 - ( speed * 3);
  fill_palette( leds, NUM_LEDS,
                gHue, 8, /* higher = narrower stripes */
                currentPalette, 255, LINEARBLEND);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void confetti()
{
  patternTimer = 80 - ( speed * 7);
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 5 + speed);
  uint16_t pos = random16(NUM_LEDS);
  leds[pos] += ColorFromPalette(currentPalette, gHue + random8(64), 255, LINEARBLEND);
}

void  cylon()
{
  patternTimer = 60 - ( speed * 5);
  fadeToBlackBy( leds, NUM_LEDS, 10 + speed );
  static uint16_t x = NUM_LEDS / 4;
  static uint16_t y = NUM_LEDS * 3 / 4;
  leds[x] = ColorFromPalette(currentPalette, gHue, 255, LINEARBLEND);
  leds[y] = ColorFromPalette(currentPalette, gHue + 128, 255, LINEARBLEND);
  x = (x + 1) % (NUM_LEDS - 1);
  y = (y + 1) % (NUM_LEDS - 1);
}

void sinelon() {
  patternTimer = 60 - ( speed * 5);
  fadeToBlackBy( leds, NUM_LEDS,  10 );
  int pos = beatsin16( 2 + speed / 2, 0, NUM_LEDS - 1 );
  leds[pos] += ColorFromPalette(currentPalette, gHue, 255, LINEARBLEND);
  if (pos + 1 < NUM_LEDS - 1)leds[pos + 1] += ColorFromPalette(currentPalette, gHue, 255, LINEARBLEND); // or it skips some leds in the middle of the strip
}

void bpm()
{
  patternTimer = 60 - ( speed * 5);
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t beat = beatsin8( speed, 64, 255);
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(currentPalette, gHue + (i * 2), beat - gHue + (i * 10), LINEARBLEND);

  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  patternTimer = 60 - ( speed * 5);
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16( i + speed, 0, NUM_LEDS - 1 )] |= ColorFromPalette(currentPalette, dothue, 255, LINEARBLEND);
    dothue += 32;
  }
}

void showSolidColor()
{
  fill_solid(leds, NUM_LEDS, solidColor);
}

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void heatMap(CRGBPalette16 palette, bool up)
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(256));

  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  byte colorindex;

  // Step 1.  Cool down every cell a little
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((cooling * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( uint16_t k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < sparking ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( uint16_t j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    colorindex = scale8(heat[j], 190);

    CRGB color = ColorFromPalette(palette, colorindex);

    if (up) {
      leds[j] = color;
    }
    else {
      leds[(NUM_LEDS - 1) - j] = color;
    }
  }
}

void fire()
{
  patternTimer = 80 - ( speed * 7);
  heatMap(HeatColors_p, true);
}

void water()
{
  patternTimer = 80 - ( speed * 7);
  heatMap(IceColors_p, false);
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.

void pride()
{
  patternTimer = 10;
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60) * speed / 5; //beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113 + (speed * 2), 1, 3000); //beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
  }
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette)
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8, LINEARBLEND);

    uint16_t pixelnumber = i;
    pixelnumber = (numleds - 1) - pixelnumber;

    nblend( ledarray[pixelnumber], newcolor, 128);
  }
}

void colorWaves()
{

  colorwaves(leds, NUM_LEDS, currentPalette);
}

typedef void (*Pattern)();
typedef Pattern PatternList[];
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

PatternAndNameList patterns = {
#if defined(IncludeMPU9250) && (IncludeMPU9250)
  {yawPitchRoll,            "Yaw Pitch and Roll"},
#endif
#if defined(IncludeAudio) && (IncludeAudio)
  { analyzerColumns,        "Analyzer Columns" },
  { analyzerColumns1,        "Analyzer Columns 2" },
  { analyzerColumns2,        "Analyzer Columns 3" },
  { analyzerColumns3,        "Analyzer Columns 4" },
  {audioSparkleBass1,            "Audio Sparkle Bass 1"},
  {audioSparkleBass2,            "Audio Sparkle Bass 2"},
  {audioSparkleBass3,            "Audio Sparkle Bass 3"},
  {audioSparkleMid1,            "Audio Sparkle Mid 1"},
  {audioSparkleMid2,            "Audio Sparkle Mid 2"},
  {audioSparkleMid3,            "Audio Sparkle Mid 3"},
  {audioSparkleTreb1,            "Audio Sparkle Treb 1"},
  {audioSparkleTreb2,            "Audio Sparkle Treb 2"},
  {audioSparkleTreb3,            "Audio Sparkle Treb 3"},
#endif
  { pride,                  "Pride" },
  { colorWaves,             "Color Waves" },
  { drawPlasma,             "Plasma"},

  // TwinkleFOX patterns
  { drawTwinkles, "Twinkles" },

  // Fire & Water
  { fire, "Fire" },
  { water, "Water" },

  // DemoReel100 patterns
  { rainbow, "rainbow" },
  { rainbowWithGlitter, "rainbowWithGlitter" },
  { confetti, "confetti" },
  { sinelon, "sinelon" },
  { juggle, "juggle" },
  { bpm, "bpm" },
  {cylon, "Cylon"},

  { showSolidColor,         "Solid Color" },
};

const uint8_t patternCount = ARRAY_SIZE(patterns);
