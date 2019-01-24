/*
   Copyright (C) 2019 Jeremy Spencer
   
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

#if defined(ESP8266)
#define rotaryPin1 D1
#define rotaryPin2 D2
#define buttonPin D3
#elif defined(ESP32)
#define rotaryPin1 1
#define rotaryPin2 2
#define buttonPin 3
#endif



Rotary r = Rotary(rotaryPin1, rotaryPin2);
int buttonState = HIGH;             // the current reading from the input pin
int lastButtonState = HIGH;   // the previous reading from the input pin
boolean hasRotated = LOW;

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
unsigned long longPressDelay = 500;
unsigned long extraLongPressDelay = 5000;
unsigned long lastPressed;
unsigned long lastRelease;

void nextPattern(bool);

void adjustBrightness(bool up) {
  if (up)
    brightnessIndex++;
  else
    brightnessIndex--;

  // don't wrap around at the ends
  if (brightnessIndex < 1)
    brightnessIndex = 1;
  if (brightnessIndex > brightnessCount)
    brightnessIndex = brightnessCount;
  brightness = brightnessMap[brightnessIndex];
  FastLED.setBrightness(brightness);
  writeFieldsToEEPROM(fields, fieldCount);
}

void adjustSpeed (bool up) {
  if (up)
    speed++;
  else
    speed--;

  // don't wrap around at the ends
  if (speed < 1)
    speed = 1;
  if (speed >= 10)
    speed = 10;
  writeFieldsToEEPROM(fields, fieldCount);
}



void setupRotaryEncoder() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(rotaryPin1, INPUT_PULLUP);
  pinMode(rotaryPin2, INPUT_PULLUP);
}

void RotaryEncoder() {
  // read the state of the switch into a local variable:
  int reading = digitalRead(buttonPin);
  unsigned char result = r.process();

  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        //        ledState = !ledState;

        if (((millis() - lastPressed) > extraLongPressDelay) && hasRotated == LOW)  {
          Serial.println("Released From Extra Long Press");
          //          EEPROM.write(EEpromInitialConfig,true);
          //          EEPROM.commit();
          fill_solid(leds, NUM_LEDS, CRGB::Blue);
          FastLED.delay(1000);
        } else  if (((millis() - lastPressed) > longPressDelay) && hasRotated == LOW)  {
          Serial.println("Released From Long Press");
          if (!autoplay) {
            autoplay = 1;
            autoPlayTimeout = millis() + (autoplayDuration * 1000);
            writeFieldsToEEPROM(fields, fieldCount);
          } else {
            autoplay = 0;
            writeFieldsToEEPROM(fields, fieldCount);
          }

        } else if (hasRotated == LOW) {
          Serial.println("Released From short Press");
          nextPattern(true);
          writeFieldsToEEPROM(fields, fieldCount);
          //          sendInt(currentPatternIndex);
        }
      } else {
        Serial.println("Pressed");
        lastPressed = millis();
        hasRotated = LOW;
      }
    }
    if (result) {
      if (buttonState == LOW) {
        Serial.println(result == DIR_CW ? "Pressed & Right" : "Pressed & Left");
        if (result == DIR_CW) {
          adjustSpeed(true);
        } else {
          adjustSpeed(false);
        }
        hasRotated = HIGH;
      } else {
        Serial.println(result == DIR_CW ? "Right" : "Left");
        if (result == DIR_CW) {
          adjustBrightness(true);
        } else {
          adjustBrightness(false);
        }
      }
    }
  }
  lastButtonState = reading;
}
