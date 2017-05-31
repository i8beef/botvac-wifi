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
#include <ESP8266HTTPClient.h>
#include <TimedAction.h>
#include <rBase64.h>


#define SSID_FILE "etc/ssid"
#define PASSWORD_FILE "etc/pass"
#define SERIAL_NUMBER "etc/serial"

#define AP_SSID "neato"
String readString;
String incomingSerial = "Empty";
String batteryInfo;
int lastBattRun = 0;
char serialnum[256];


WiFiClient client;
int maxBuffer = 8192;
int bufferSize = 0;
uint8_t currentClient = 0;
uint8_t serialBuffer[8193];
ESP8266WebServer server = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266WebServer updateServer(82);
ESP8266HTTPUpdateServer httpUpdater;

void getPage() {
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
   
      // Only check battery once at the beginning, once every 60 seconds, or once when it got an empty response.
      if (batteryInfo == "" || batteryInfo == "-FAIL-" || lastBattRun > 11) {
        getBattery();
        lastBattRun = 0;
      } else {
        lastBattRun++;
      }
      if (batteryInfo != "" && batteryInfo != "-FAIL-" && incomingSerial.indexOf("Empty") == -1 && incomingSerial != "") {
        HTTPClient http;  //Declare an object of class HTTPClient
        http.begin("http://www.neatoscheduler.com/api/actionPull.php?serial="+incomingSerial+"&battery="+batteryInfo);  //Specify request destination
        int httpCode = http.GET(); //Send the request
     
        if (httpCode > 0) { //Check the returning code
     
          String payload = http.getString();   //Get the request response payload
          if (payload != "None") {
            // If it's something other than none, shoot it to the vac.
            Serial.println(payload);
            //delay(2000);
            // Do it twice because we might be asleep
            //Serial.println(payload);
          }
        }
        http.end();   //Close connection
      }
    }
}

void getSerial() {
    Serial.setTimeout(250);
    Serial.println("GetVersion");
    incomingSerial = Serial.readString();
    int serialString = incomingSerial.indexOf("Serial Number");
    if (serialString > -1){
      int capUntil = serialString+50;
      incomingSerial = rbase64.encode(incomingSerial.substring(serialString,capUntil));
      incomingSerial.replace('+', '-');
      incomingSerial.replace('/', '_');
      incomingSerial.replace('=', ',');
    }
}

void getBattery() {
    Serial.setTimeout(500);
    Serial.println("GetCharger");
    String batteryInfoTemp = Serial.readString();
    String checkArray[3] = {"FuelPercent", "ChargingActive", "ChargingEnabled"};
    for (int i = 0; i < 3; i++){
      int serialString = batteryInfoTemp.indexOf(checkArray[i]);
      if (serialString > -1){
        int checkLength = checkArray[i].length()+4;
        int capUntil = serialString+checkLength;
        if (i == 0) {
          batteryInfo = batteryInfoTemp.substring(serialString,capUntil);
        } else {
          batteryInfo = batteryInfo + "," + batteryInfoTemp.substring(serialString,capUntil);
        }
      }
    }
    batteryInfo = rbase64.encode(batteryInfo);
    batteryInfo.replace('+', '-');
    batteryInfo.replace('/', '_');
    batteryInfo.replace('=', ',');
}
//create a couple timers that will fire repeatedly every x ms
TimedAction checkServer = TimedAction(5000,getPage);

void botDissconect() {
  // always disable testmode on disconnect
  Serial.println("TestMode off");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
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
  server.send(200, "text/html", "<!DOCTYPE html><meta charset='utf-8' /><style>p{white-space:pre;word-wrap:break-word;font-family:monospace;}</style><title>Neato Console</title><script language='javascript' type='text/javascript'>var b='ws://'+location.hostname+':81/',c,d,e;function g(){d=new WebSocket(b);d.onopen=function(){h('[connected]')};d.onclose=function(){h('[disconnected]')};d.onmessage=function(a){h('<span style=\"color: blue;\">[response] '+a.data+'</span>')};d.onerror=function(a){h('<span style=\"color: red;\">[error] </span> '+a.data)}}\nfunction k(a){if(13==a.keyCode){a=e.value;if('/disconnect'==a)d.close();else if('/clear'==a)for(;c.firstChild;)c.removeChild(c.firstChild);else''!=a&&(h('[sent] '+a),d.send(a));e.value='';e.focus()}}function h(a){var f=document.createElement('p');f.innerHTML=a;c.appendChild(f);window.scrollTo(0,document.body.scrollHeight)}\nwindow.addEventListener('load',function(){c=document.getElementById('c');e=document.getElementById('i');g();document.getElementById('i').addEventListener('keyup',k,!1);e.focus()},!1);</script><h2>Neato Console</h2><div id='c'></div><input type='text' id='i' style=\"width:100%;font-family:monospace;\">\n");
}

void setupEvent() {
  server.send(200, "text/html", "<!DOCTYPE html><html> <body> <form action=\"\" method=\"post\"> Access Point SSID:<br><input type=\"text\" name=\"ssid\" value=\"XXX\"> <br>WPA2 Password:<br><input type=\"text\" name=\"password\" value=\"XXX\"> <br><br><input type=\"submit\" value=\"Submit\"> </form> <p>Enter the details for your access point. After you submit, the controller will reboot to apply the settings.</p></body></html>\n");
}

void saveEvent() {
  String user_ssid = server.arg("ssid");
  String user_password = server.arg("password");
  
  if(user_ssid != "" && user_password != "") {
    File ssid_file = SPIFFS.open(SSID_FILE, "w");
    ssid_file.print(user_ssid);
    ssid_file.close();
    File passwd_file = SPIFFS.open(PASSWORD_FILE, "w");
    passwd_file.print(user_password);
    passwd_file.close();
    ESP.reset();
  }
  //save these to the filesystem
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
    File ssid_file = SPIFFS.open(SSID_FILE, "r");
    char ssid[256];
    ssid_file.readString().toCharArray(ssid, 256);
    ssid_file.close();
    File passwd_file = SPIFFS.open(PASSWORD_FILE, "r");
    char passwd[256];
    passwd_file.readString().toCharArray(passwd, 256);
    passwd_file.close();

  // start wifi
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passwd);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
  }
  else {
    WiFi.disconnect();
    if(! WiFi.softAP(AP_SSID))
      ESP.reset();
  }

  // start websocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  //OTA update hooks
  ArduinoOTA.onStart([]() {
    SPIFFS.end();
    webSocket.sendTXT(currentClient, "ESP-12x: OTA Update Starting\n");
  });

  ArduinoOTA.onEnd([]() {
    SPIFFS.begin();
    webSocket.sendTXT(currentClient, "ESP-12x: OTA Update Complete\n");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    webSocket.sendTXT(currentClient, "ESP-12x: OTA Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("ESP-12x: OTA Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("ESP-12x: OTA Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("ESP-12x: OTA Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("ESP-12x: OTA Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("ESP-12x: OTA End Failed");
  });

  ArduinoOTA.begin();
  httpUpdater.setup(&updateServer);
  updateServer.begin();
  // start webserver
  server.on("/console", serverEvent);
  server.on("/", HTTP_POST, saveEvent);
  server.on("/", HTTP_GET, setupEvent);
  server.onNotFound(serverEvent);
  server.begin();

  // start MDNS
  // this means that the botvac can be reached at http://neato.local or ws://neato.local:81
  if (!MDNS.begin("neato")) {
    while (1) {
      // wait for watchdog timer
      delay(500);
    }
  }
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);
  MDNS.addService("http", "tcp", 82);

  webSocket.sendTXT(currentClient, "ESP-12x: Ready\n");
}

void loop() {
  checkServer.check();
  webSocket.loop();
  
  checkServer.check();
  server.handleClient();
  
  checkServer.check();
  ArduinoOTA.handle();
  
  checkServer.check();
  updateServer.handleClient();
  
  checkServer.check();
  // Get our serial if we can
  if (incomingSerial.indexOf("Empty") != -1 || incomingSerial == ""){
    getSerial();
  }
  checkServer.check();
  serialEvent();
  }
