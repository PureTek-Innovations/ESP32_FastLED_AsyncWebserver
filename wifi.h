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

#define HOSTNAME "ESP32" ///< Hostname. The setup function adds the Chip ID at the end.
String hostname(HOSTNAME);
const bool apMode = false;

/* Soft AP network parameters */
char WiFiAPPSK[] = "12345678";
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);
const byte DNS_PORT = 53;
DNSServer dnsServer;


void setupWiFi() {

  //  webServer.on("/wifi", HTTP_POST, []() {
  //    String ssid = ws.arg("ssid");
  //    String password = ws.arg("password");
  // String mode = webServer.arg("mode");
  //
  //    char ssidChars[50];
  //    ssid.toCharArray(ssidChars, 50);
  //
  //    char passwordChars[50];
  //    password.toCharArray(passwordChars, 50);
  //
  //    debugPrintln("Connecting to new SSID:");
  //    debugPrintln(ssid);

  // dnsServer.stop();
  // WiFi.softAPdisconnect(true);

  // apMode = false;
  // WiFi.mode(WIFI_STA);
  //    WiFi.begin(ssidChars, passwordChars);
  // futureTimeout = millis() + connectionTimeout;

  //    webServer.sendHeader("Location", "/wifi.htm");
  //    webServer.send(303);
  //  });
  //
  //  webServer.on("/wifi", HTTP_GET, []() {
  //    String json = getWiFiJson();
  //    webServer.send(200, "application/json", json);
  //  });

#if defined(ESP8266)
if(apMode){
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 4], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String softAP_ssid = "XmasTree" + macID;
  char ssid[softAP_ssid.length()];
  softAP_ssid.toCharArray(ssid, softAP_ssid.length() + 1);

  String softAP_password = macID;
  softAP_password += macID;

  char password[softAP_password.length()];
  softAP_password.toCharArray(password, softAP_password.length() + 1);
}
#elif defined(ESP32)
  uint64_t chipid = ESP.getEfuseMac();
  Serial.printf("ESP32 Chip ID = %04X", (uint16_t)(chipid >> 32)); //print High 2 bytes
  Serial.printf("%08X\n", (uint32_t)chipid); //print Low 4bytes.

  uint16_t long1 = (unsigned long)((chipid & 0xFFFF0000) >> 16 );
  uint16_t long2 = (unsigned long)((chipid & 0x0000FFFF));
  String hex = String(long1, HEX) + String(long2, HEX); // six octets
  hostname += hex;

  char hostnameChar[hostname.length() + 1];
  memset(hostnameChar, 0, hostname.length() + 1);

  for (uint8_t i = 0; i < hostname.length(); i++)hostnameChar[i] = hostname.charAt(i);
  for (uint8_t i = 0; i < hex.length(); i++)WiFiAPPSK[i] = hex.charAt(i);
#endif
  // Set Hostname.





  if (apMode) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
#if defined(ESP8266)
    WiFi.hostname(ssid);
#elif defined(ESP32)
    WiFi.setHostname(hostnameChar);
#endif
    Serial.println("Starting AP Mode");
#if defined(ESP8266)
    WiFi.softAP(ssid, password);
    delay(500); // Without delay I've seen the IP address blank
    Serial.printf("Connect to Wi-Fi access point: %s\n", ssid);
    Serial.printf("Password: %s\n", password);
#elif defined(ESP32)
    WiFi.softAP(hostnameChar, WiFiAPPSK);
    delay(500); // Without delay I've seen the IP address blank
    Serial.printf("Connect to Wi-Fi access point: %s\n", hostnameChar);
    Serial.printf("Password: %s\n", WiFiAPPSK);
#endif
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println();
    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  } else     {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
#if defined(ESP8266)
    WiFi.hostname("PureTek Controller");
#elif defined(ESP32)
    WiFi.setHostname("PureTek Controller");
#endif
    Serial.printf("Connecting to %s\n", ssid);
    //    if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);

    //    }
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    /* Setup the DNS server redirecting all the domains to the IP */
    //    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    //    dnsServer.start(DNS_PORT, "*", WiFi.localIP());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  if (!MDNS.begin("esp32")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "id", "esp32");
    MDNS.setInstanceName("ESP32 Controller");
    Serial.println("MDNS responder set up");
  }
}
