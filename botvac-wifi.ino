#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>


#define SSID_FILE "etc/ssid"
#define PASSWORD_FILE "etc/pass"
#define HOST_FILE "etc/hostname"

#define AP_IP "192.168.0.1"
#define AP_SSID "neato"
#define AP_PASSWORD "neato"

WiFiClient client;
int maxBuffer = 8192;
int bufferSize = 0;
uint8_t currentClient = 0;
uint8_t serialBuffer[8193];
ESP8266WebServer server = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266WebServer updateServer(82);
ESP8266HTTPUpdateServer httpUpdater;


void botDissconect() {
  // always disable testmode on disconnect
  Serial.println("TestMode off");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch (type) {
    case WStype_DISCONNECTED:
      // always disable testmode on disconnect
      botDissconect();
      break;
    case WStype_CONNECTED:
      webSocket.sendTXT(num, "connected to Neato");
      // allow only one concurrent client connection
      currentClient = num;
      // all clients but the last connected client are disconnected
      for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (i != currentClient) {
          webSocket.disconnect(i);
        }
      }
      // reset serial buffer on new connection to prevent garbage
      serialBuffer[0] = '\0';
      bufferSize = 0;
      break;
    case WStype_TEXT:
      // send incoming data to bot
      Serial.printf("%s\n", payload);
      break;
    case WStype_BIN:
      webSocket.sendTXT(num, "binary transmission is not supported");
      break;
  }
}

void serverEvent() {
  // just a very simple websocket terminal, feel free to use a custom one
  String page = SPIFFS.open("web/index.html", "r").readString();
  server.send(200, "text/html", page);
}

void setupEvent() {
  String page = SPIFFS.open("web/setup.html", "r").readString();
  server.send(200, "text/html", page);
}

void saveEvent() {

}

void serialEvent() {
  while (Serial.available() > 0) {
    char in = Serial.read();
    // there is no proper utf-8 support so replace all non-ascii
    // characters (<127) with underscores; this should have no
    // impact on normal operations and is only relevant for non-english
    // plain-text error messages
    if (in > 127) {
      in = '_';
    }
    serialBuffer[bufferSize] = in;
    bufferSize++;
    // fill up the serial buffer until its max size (8192 bytes, see maxBuffer)
    // or unitl the end of file marker (ctrl-z; \x1A) is reached
    // a worst caste lidar result should be just under 8k, so that maxBuffer
    // limit should not be reached under normal conditions
    if (bufferSize > maxBuffer - 1 || in == '\x1A') {
      serialBuffer[bufferSize] = '\0';
      bool allSend = false;
      uint8_t localBuffer[1464];
      int localNum = 0;
      int bufferNum = 0;
      while (!allSend) {
        localBuffer[localNum] = serialBuffer[bufferNum];
        localNum++;
        // split the websocket packet in smaller (1300 + x) byte packets
        // this is a workaround for some issue that causes data corruption
        // if the payload is split by the wifi library into more than 2 tcp packets
        if (serialBuffer[bufferNum] == '\x1A' || (serialBuffer[bufferNum] == '\n' && localNum > 1300)) {
          localBuffer[localNum] = '\0';
          localNum = 0;
          webSocket.sendTXT(currentClient, localBuffer);
        }
        if (serialBuffer[bufferNum] == '\x1A') {
          allSend = true;
        }
        bufferNum++;
      }
      serialBuffer[0] = '\0';
      bufferSize = 0;
    }
  }
}

void setup() {
  // start serial
  // botvac serial console is 115200 baud, 8 data bits, no parity, one stop bit (8N1)
  Serial.begin(115200);

  SPIFFS.begin();

  if(SPIFFS.exists(SSID_FILE) && SPIFFS.exists(PASSWORD_FILE)) {
    String ssid = SPIFFS.open(SSID_FILE, "r").readString();
    String passwd = SPIFFS.open(PASSWORD_FILE, "r").readString();

  // start wifi
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
  }
  else {
    WiFi.disconnect();
    IPAddress AP_IP = WiFi.softAP(AP_SSID, AP_PASSWORD);
  }


  // start websocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  //OTA update hooks
  ArduinoOTA.onStart([]() {
    SPIFFS.end();
    webSocket.sendTXT(currentClient, "ESP-12F: OTA Update Starting\n");
  });

  ArduinoOTA.onEnd([]() {
    SPIFFS.begin();
    webSocket.sendTXT(currentClient, "ESP-12F: OTA Update Complete\n");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    webSocket.sendTXT(currentClient, "ESP-12F: OTA Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("ESP-12F: OTA Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("ESP-12F: OTA Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("ESP-12F: OTA Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("ESP-12F: OTA Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("ESP-12F: OTA End Failed");
  });

  ArduinoOTA.begin();
  httpUpdater.setup(&updateServer);
  updateServer.begin();
  // start webserver
  server.on("/", serverEvent);
  server.on("/save_settings", saveEvent);
  server.on("/setup", setupEvent);
  server.onNotFound(serverEvent);
  server.begin();

  // start MDNS
  // this means that the botvac can be reached at http://neato.local or ws://neato.local:81
  if (!MDNS.begin(HOST)) {
    while (1) {
      // wait for watchdog timer
      delay(500);
    }
  }
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);
  MDNS.addService("http", "tcp", 82);

  webSocket.sendTXT(currentClient, "ESP-12F: Ready\n");
}

void loop() {
  webSocket.loop();
  server.handleClient();
  ArduinoOTA.handle();
  updateServer.handleClient();
  serialEvent();
}
