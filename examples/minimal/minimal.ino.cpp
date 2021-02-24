# 1 "C:\\Users\\Steve\\AppData\\Local\\Temp\\tmpxxqd7ahc"
#include <Arduino.h>
# 1 "C:/Users/Steve/Projects/Arduino/libraries/stevesch-PageHandler/examples/minimal/minimal.ino"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <stevesch-WiFiConnector.h>

#include <stevesch-PageHandler.h>

using stevesch::PageHandler::VarReflector;


long lastAutoSend = -1;
constexpr long kAutoSendPeriod = 500;


long lastLog = -1;
constexpr long kLogPeriod = 3000;
int logsPerformed = 0;
void printWiFiStaus();

String processHostName(const String& name);
String processDateTime(const String& name);

VarReflector<int> vrIntValue1("INTVALUE1", 17);
VarReflector<float> vrValue1("FLOATVALUE1", 0.65f);

VarReflector<String> vrColorValue1("COLORVALUE1", "#ffff00");

VarReflector<int> vrMemFreeHeap("MEMFREEHEAP", 0);
VarReflector<int> vrMemMaxAllocHeap("MEMMAXALLOCHEAP", 0);
VarReflector<int> vrMemMinFreeHeap("MEMMINFREEHEAP", 0);
VarReflector<int> vrMemHeapSize("MEMHEAPSIZE", 0);

AsyncWebServer server(80);
void setup();
void loop();
#line 34 "C:/Users/Steve/Projects/Arduino/libraries/stevesch-PageHandler/examples/minimal/minimal.ino"
void setup()
{
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Setup initializing...");

  stevesch::PageHandler::registerProcessor("HOSTNAME", processHostName);
  stevesch::PageHandler::registerProcessor("DATETIME", processDateTime);

  stevesch::WiFiConnector::setup(&server, "PageHandler-Test");
  stevesch::PageHandler::setup(server);
  server.begin();

  Serial.println("Setup complete.");
}

void loop()
{
  long tNow = millis();
  long delta;


  delta = (tNow - lastAutoSend);
  if (delta > kAutoSendPeriod)
  {
    lastAutoSend = tNow;


    vrMemFreeHeap = ESP.getFreeHeap();
    vrMemMaxAllocHeap = ESP.getMaxAllocHeap();
    vrMemMinFreeHeap = ESP.getMinFreeHeap();
    vrMemHeapSize = ESP.getHeapSize();


    stevesch::PageHandler::processAndSendUpdatedServerValues();
  }


  delta = (tNow - lastLog);
  if (delta > kLogPeriod)
  {
    lastLog = tNow;
    ++logsPerformed;

    printWiFiStaus();
  }

  stevesch::WiFiConnector::loop();
  stevesch::PageHandler::loop();
}


void printWiFiStaus()
{
  String strIp = WiFi.localIP().toString();
  Serial.printf("%10s %12s RSSI: %d  ch: %d  Tx: %d\n",
    WiFi.SSID().c_str(),
    strIp.c_str(),
    WiFi.RSSI(), WiFi.channel(), (int)WiFi.getTxPower()
  );
  if (stevesch::WiFiConnector::isUpdating()) {
    Serial.println("OTA update is being performed");
  }
}

String processHostName(const String& name)
{
  const char* hostname = WiFi.getHostname();
  if (hostname) {
    return String(hostname);
  }
  return WiFi.localIP().toString();
}

String processDateTime(const String& name)
{
  String s;
  struct tm timeinfo;
  if(getLocalTime(&timeinfo))
  {
    s = asctime(&timeinfo);
  }
  return s;
}