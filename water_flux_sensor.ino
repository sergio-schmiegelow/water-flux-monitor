#define NTP_SERVER "pool.ntp.br"
#define FALSE 0
#define TRUE 1
#define PULSE_PIN D4 
#define PULSE_RATE_INTERVAL 1000 //ms

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Time.h>

#include "wifi_credentials.h"
//Espected format for wifi_credentials.h
//#define STASSID "<your AP ssid>"
//#define STAPSK  "<your AP password>"

//flow meter variables ----------------------------------------------------------

volatile unsigned long pulseCount = 0;
unsigned long pulseCountCopy;
unsigned long lastPulseCount;
unsigned long lastPulseCountMillis;
float pulseRate;

//NTP variables -----------------------------------------------------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER , 0);
int lastNTPUpdate = 0;

//WiFi variables ----------------------------------------------------------------
const char* ssid = STASSID;
const char* password = STAPSK;

//Web server variables ----------------------------------------------------------
ESP8266WebServer server(80);
String output_json;

//General variables -------------------------------------------------------------
const int LED = 2;
unsigned long bootTimeStamp;

//Web server functions ----------------------------------------------------------
void handleRoot() {
    server.send(200, "text/plain", String(pulseCountCopy));
}
//-------------------------------------------------------------------------------
void handleSensorDataJson() {
    server.send(200, "text/plain", createJsonOutput());
}
//-------------------------------------------------------------------------------
void handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

//Hardware interruption callback function-----------------------------------------
ICACHE_RAM_ATTR void pulseCounter() {
    // Increment the pulse counter
    pulseCount++;
}
//--------------------------------------------------------------------------------
void updatePulseRate() {
    unsigned long currMillis;
    unsigned long deltaT;
    unsigned long deltaPulses;

    currMillis = millis();
    if (currMillis <= lastPulseCountMillis){
        //counter restarted
        lastPulseCountMillis = currMillis;
        lastPulseCount = pulseCountCopy;
        return;
    }
    deltaT = currMillis - lastPulseCountMillis;
    if (deltaT >= PULSE_RATE_INTERVAL) {
        deltaPulses = pulseCountCopy - lastPulseCount;
        pulseRate = ((float)deltaPulses) / ((float)deltaT) * 1000.0;
        lastPulseCount = pulseCountCopy;
        lastPulseCountMillis = currMillis;
    }
}
//--------------------------------------------------------------------------------
String createJsonOutput(){
    DynamicJsonDocument doc(1024);
    unsigned long reportTimeStamp;
    char readableReportTime[32];
    char readableBootTime[32];
    
    reportTimeStamp = timeClient.getEpochTime();
    sprintf(readableReportTime, "%02d/%02d/%02d - %02d:%02d:%02d UTC", day(reportTimeStamp), month(reportTimeStamp), year(reportTimeStamp), hour(reportTimeStamp), minute(reportTimeStamp), second(reportTimeStamp));
    sprintf(readableBootTime, "%02d/%02d/%02d - %02d:%02d:%02d UTC", day(bootTimeStamp), month(bootTimeStamp), year(bootTimeStamp), hour(bootTimeStamp), minute(bootTimeStamp), second(bootTimeStamp));
    doc["boot_time_epoch"]   = bootTimeStamp;
    doc["boot_time"]         = readableBootTime;
    doc["report_time_epoch"] = reportTimeStamp;
    doc["report_time"]       = readableReportTime;
    doc["pulses"]            = pulseCountCopy;
    doc["pulse_rate"]        = pulseRate;
    output_json = "";
    serializeJson(doc, output_json);
    return output_json;
}
//--------------------------------------------------------------------------------
void setup(void) {
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp8266")) {
      Serial.println("MDNS responder started");
    }

    server.on("/", handleRoot);
    server.on("/sensor_data.json", handleSensorDataJson);

    server.onNotFound(handleNotFound);

    timeClient.begin();
    Serial.println("NTP client started");
    timeClient.update();
  
    bootTimeStamp = timeClient.getEpochTime();
    lastPulseCount = 0;
    lastPulseCountMillis = millis();
    pulseRate = 0;
  
    server.begin();
    Serial.println("HTTP server started");
  
    pinMode(PULSE_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseCounter, RISING);
}
//--------------------------------------------------------------------------------

void loop(void) {
    pulseCountCopy = pulseCount;
    updatePulseRate();
    digitalWrite(LED, pulseCountCopy % 2);
    server.handleClient();
    MDNS.update();
}
