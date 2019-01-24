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

void setupWeb() {
  webServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm").setCacheControl("max-age=600");

  webServer.on("/all", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = getFieldsJson(fields, fieldCount);
    request->send(200, "text/json", json);
  });

// webServer.onRequestBody([](AsyncWebServerRequest *request){
  webServer.onNotFound([](AsyncWebServerRequest * request) { //Hacky - but works ;)
    String newDataName;
    String newDataValue;
    String newValue;
    bool gotDataName = false;
    bool gotDataValue = false;
    bool gotSolidColor = false;
    String r;
    String g;
    String b;

    if (request->hasParam("name", true)) {
      AsyncWebParameter* p = request->getParam("name", true);
      newDataName = p->value().c_str();
      gotDataName = true;
    }
    if (newDataName == "solidColor") {

      if (request->hasParam("r", true)) {
        AsyncWebParameter* p = request->getParam("r", true);
        r = p->value().c_str();
      }
      if (request->hasParam("g", true)) {
        AsyncWebParameter* p = request->getParam("g", true);
        g = p->value().c_str();
      }
      if (request->hasParam("b", true)) {
        AsyncWebParameter* p = request->getParam("b", true);
        b = p->value().c_str();
      }

      newDataValue = r + "," + g + "," + b;
      gotDataValue = true;
    }   else if  (request->hasParam("value", true)) {
      AsyncWebParameter* p = request->getParam("value", true);
      newDataValue = p->value().c_str();
      gotDataValue = true;
    }
    if (gotDataName && gotDataValue) {
      newValue = setFieldValue(newDataName, newDataValue, fields, fieldCount);
      request->send(200, "text/json", newValue);
      gotDataName = false;
      gotDataValue = false;
    } else {
      request->send(404);
    }
  });

  Serial.println ( "Starting HTTP server" );
  webServer.begin();
  Serial.println ( "HTTP server started" );

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

//  ArduinoOTA
//  .onStart([]() {
//    String type;
//    if (ArduinoOTA.getCommand() == U_FLASH)
//      type = "sketch";
//    else // U_SPIFFS
//      type = "filesystem";
//
//    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
//    Serial.println("Start updating " + type);
//  })
//  .onEnd([]() {
//    Serial.println("\nEnd");
//  })
//  .onProgress([](unsigned int progress, unsigned int total) {
//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
//  })
//  .onError([](ota_error_t error) {
//    Serial.printf("Error[%u]: ", error);
//    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
//    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
//    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
//    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
//    else if (error == OTA_END_ERROR) Serial.println("End Failed");
//  });
//
//  ArduinoOTA.begin();

  Serial.println("Ready");
  if (!apMode) {
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void handleWeb() {
  static bool webServerStarted = false;
  if (!apMode) {
    // check for connection
    if ( WiFi.status() == WL_CONNECTED ) {
      if (!webServerStarted) {
        // turn off hte board's LED when connected to wifi
        digitalWrite(led, 1);
        Serial.println();
        webServerStarted = true;
        setupWeb();
      }
//      ArduinoOTA.handle();
    } else {
      // blink the board's LED while connecting to wifi
      static uint8_t ledState = 0;
      EVERY_N_MILLIS(125) {
        ledState = ledState == 0 ? 1 : 0;
        digitalWrite(led, ledState);
        Serial.print (".");
      }
    }
  } else {
    if (!webServerStarted) {
      setupWeb();
      webServerStarted = true;
    } else {
      dnsServer.processNextRequest();
//      ArduinoOTA.handle();
    }
  }
}
