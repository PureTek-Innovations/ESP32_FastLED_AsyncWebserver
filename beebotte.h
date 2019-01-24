#include <PubSubClient.h>
#include <ArduinoJson.h>

#define BBT "mqtt.beebotte.com"     // Domain name of Beebotte MQTT service


#define CHANNEL "Hearts"          // Replace with your device name
#define LED_RESOURCE "ga"

WiFiClient espClient;
PubSubClient client(espClient);

void nextPattern(bool);
void nextPalette(bool);

// to track delay since last reconnection
long lastReconnectAttempt = 0;

const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

char id[17];

const char * generateID()
{
  randomSeed(analogRead(0));
  int i = 0;
  for (i = 0; i < sizeof(id) - 1; i++) {
    id[i] = chars[random(sizeof(chars))];
  }
  id[sizeof(id) - 1] = '\0';

  return id;
}

int find_text(String needle, String haystack) {
  int foundpos = -1;
  if (needle.length() <= haystack.length()) {
    for (int i = 0; i <= haystack.length() - needle.length(); i++) {
      if (haystack.substring(i, needle.length() + i) == needle) {
        foundpos = i;
      }
    }
  }
  return foundpos;
}

void find_commands(String data) {
  /* power on off
     turn on off
     pattern up down autocyle on off
     pallete up down autocyle on off
     speed up down maximum minimum
     brightness up down maximum minimum
  */
  if ( find_text("power", data) > -1) {
    if  (find_text("on", data) > -1) {
      Serial.println("Command power on");
      power = 1;
    } else if (find_text("off", data) > -1) {
      Serial.println("Command power off");
      power = 0;
    }
  } else if ( find_text("pattern", data) > -1) {
    if  (find_text("next", data) > -1) {
      Serial.println("Command next pattern");
      nextPattern(true);
      writeFieldsToEEPROM(fields, fieldCount);
    } else if (find_text("previous", data) > -1) {
      Serial.println("Command previous pattern");
      nextPattern(false);
      writeFieldsToEEPROM(fields, fieldCount);
    } else if (find_text("auto cycle", data) > -1) {
      if  (find_text("on", data) > -1) {
        Serial.println("Command auto cycle patterns on");
        autoplay = 1;
        autoPlayTimeout = millis() + (autoplayDuration * 1000);
        writeFieldsToEEPROM(fields, fieldCount);
      } else if (find_text("off", data) > -1) {
        Serial.println("Command auto cylce patterns off");
        autoplay = 0;
        writeFieldsToEEPROM(fields, fieldCount);
      }
    }
  } else if ( find_text("pallet", data) > -1 || find_text("palette", data) > -1) {
    if  (find_text("next", data) > -1) {
      Serial.println("Command next palette");
      nextPalette(true);
      writeFieldsToEEPROM(fields, fieldCount);
    } else if (find_text("previous", data) > -1) {
      Serial.println("Command previous palette");
      nextPalette(false);
      writeFieldsToEEPROM(fields, fieldCount);
    } else if (find_text("auto cycle", data) > -1) {
      if  (find_text("on", data) > -1) {
        Serial.println("Command auto cycle palettes on");
        cyclePalettes = 1;
        paletteTimeout = millis() + (paletteDuration * 1000);
        writeFieldsToEEPROM(fields, fieldCount);
      } else if (find_text("off", data) > -1) {
        Serial.println("Command auto cylce palettes off");
        cyclePalettes = 0;
        writeFieldsToEEPROM(fields, fieldCount);
      }
    }
  } else if ( find_text("speed", data) > -1) {
    if  (find_text("up", data) > -1 || find_text("increase", data) > -1) {
      Serial.println("Command speed up");
      adjustSpeed (true);
    } else if (find_text("down", data) > -1 || find_text("decrease", data) > -1) {
      Serial.println("Command speed down");
      adjustSpeed (false);
    } else if (find_text("max", data) > -1) {
      Serial.println("Command max speed");
      speed = 10;
      adjustSpeed (true);
    } else if (find_text("min", data) > -1) {
      Serial.println("Command min speed");
      speed = 1;
      adjustSpeed (false);
    }
  } else if ( find_text("slow", data) > -1) {
    if  (find_text("down", data) > -1) {
      Serial.println("Command slow down");
      adjustSpeed (false);
    }
  } else if ( find_text("brightness", data) > -1) {
    if  (find_text("up", data) > -1 || find_text("increase", data) > -1) {
      Serial.println("Command brightness up");
      adjustBrightness(true);
    } else if (find_text("down", data) > -1 || find_text("decrease", data) > -1) {
      Serial.println("Command brightness down");
      adjustBrightness(false);
    } else if (find_text("max", data) > -1) {
      Serial.println("Command max brightness");
      brightnessIndex = brightnessCount;
      adjustBrightness(true);
    } else if (find_text("min", data) > -1) {
      Serial.println("Command min brightness");
      brightnessIndex = 1;
      adjustBrightness(false);
    }
  } else if ( find_text("brighter", data) > -1) {
    Serial.println("Command brighter");
    adjustBrightness(true);
  } else if ( find_text("dimmer", data) > -1) {
    Serial.println("Command dimmer");
    adjustBrightness(false);
  } else if ( find_text("white", data) > -1) {
    Serial.println("Command white");
    power = 1;
    solidColor = CRGB::White;
    currentPatternIndex = patternCount - 1; // set the current pattern to Solid Color
    autoplay = 0; // turn off pattern auto cycle
    writeFieldsToEEPROM(fields, fieldCount);
  }  else if ( find_text("red", data) > -1) {
    Serial.println("Command red");
    power = 1;
    solidColor = CRGB::Red;
    currentPatternIndex = patternCount - 1; // set the current pattern to Solid Color
    autoplay = 0; // turn off pattern auto cycle
    writeFieldsToEEPROM(fields, fieldCount);
  }  else if ( find_text("green", data) > -1) {
    Serial.println("Command green");
    power = 1;
    solidColor = CRGB::Green;
    currentPatternIndex = patternCount - 1; // set the current pattern to Solid Color
    autoplay = 0; // turn off pattern auto cycle
    writeFieldsToEEPROM(fields, fieldCount);
  }  else if ( find_text("blue", data) > -1) {
    Serial.println("Command blue");
    power = 1;
    solidColor = CRGB::Blue;
    currentPatternIndex = patternCount - 1; // set the current pattern to Solid Color
    autoplay = 0; // turn off pattern auto cycle
    writeFieldsToEEPROM(fields, fieldCount);
  } else {
    Serial.println("Command not found");
  }
}

// will be called every time a message is received
void onMessage(char* topic, byte* payload, unsigned int length) {

  // decode the JSON payload
  StaticJsonBuffer<128> jsonInBuffer;

  JsonObject& root = jsonInBuffer.parseObject(payload);

  // Test if parsing succeeded
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  // led resource is a boolean read it accordingly
  String data = root["data"];
  data.toLowerCase();

  // Print the received value to serial monitor for debugging
  Serial.print("Received message of length ");
  Serial.print(length);
  Serial.println();
  Serial.print("data ");
  Serial.print(data);
  Serial.println();
  find_commands(data);

  // Set the led pin to high or low
  //  digitalWrite(LEDPIN, data ? HIGH : LOW);
}

// reconnects to Beebotte MQTT server
boolean reconnect() {
  if (client.connect(generateID(), TOKEN, "")) {
    char topic[64];
    sprintf(topic, "%s/%s", CHANNEL, LED_RESOURCE);
    client.subscribe(topic);

    Serial.println("Connected to Beebotte MQTT");
  }
  return client.connected();
}

void setupBeebotte() {
  randomSeed(micros());
  client.setServer(BBT, 1883);
  client.setCallback(onMessage);
  lastReconnectAttempt = 0;
}



void loopBeebotte()
{
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }
}
