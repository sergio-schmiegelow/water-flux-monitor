#define STASSID "ssw"
#define STAPSK  "9192631770"
#define NTP_UPDATE_PERIOD 60 //seconds
#define NTP_SERVER "pool.ntp.org"
#define FALSE 0
#define TRUE 1

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "wifi_credentials.h"
//Espected format for wifi_credentials.h
//#define STASSID "<your AP ssid>"
//#define STAPSK  "<your AP password>"

//flow meter variables ----------------------------------------------------------
uint8_t PULSE_PIN = D4; 
volatile int pulseCount = 0;
int pulseCountCopy;

//NTP variables -----------------------------------------------------------------
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER , 0);
int lastNTPUpdate = 0;

//WiFi variables ----------------------------------------------------------------
const char* ssid = STASSID;
const char* password = STAPSK;

//Web server variables ----------------------------------------------------------
ESP8266WebServer server(80);

//General variables -------------------------------------------------------------
const int LED = 2;
bool firstLoop = FALSE;

//Web server functions ----------------------------------------------------------
void handleRoot() {
  server.send(200, "text/plain", timeClient.getFormattedTime() + ' ' + pulseCountCopy);
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

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  timeClient.begin();
  Serial.println("NTP client started");
  pinMode(PULSE_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseCounter, RISING);
}
//--------------------------------------------------------------------------------

void loop(void) {
  int ntpDeltaT;
  pulseCountCopy = pulseCount;
  digitalWrite(LED, pulseCountCopy % 2);
  
  server.handleClient();
  MDNS.update();
  ntpDeltaT = millis() - lastNTPUpdate;
  if (lastNTPUpdate == 0 || ntpDeltaT < 0 || (ntpDeltaT / 1000) >= NTP_UPDATE_PERIOD)
  {
    timeClient.update();
    lastNTPUpdate = millis();
    Serial.println("NTP updated");
  }
}
