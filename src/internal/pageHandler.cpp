#include "pageHandler.h"
#include "reloader.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <map>
// #include <pgmspace.h>   // for PROGMEM

using stevesch::PageHandler::procFn_t;
using stevesch::PageHandler::recvFn_t;
namespace {
  struct ProcRegistryEntry
  {
    procFn_t  procFn;
    String    lastSentValue;
  };
  std::map<String, ProcRegistryEntry> processorRegistry;

  struct RecvRegistryEntry
  {
    recvFn_t  recvFn;
  };
  std::map<String, RecvRegistryEntry> receiverRegistry;

  AsyncEventSource events("/events");

  // Replace placeholders with values,
  // e.g. replace %FOO% with return value of function registered for "FOO"
  String processor(const String& var);

  // very meta-- processor that tells the HTML what processors exist:
  String processReflectionList(const String& name);

  // Route /api/set?name=<var>&value=<value> to the proper function for variable assignment
  void receive(const String& name, const String& value);

  // Send code-based page for reloader (for when SPIFFS is not available)
  void handleReloader(AsyncWebServerRequest *request)
  {
    request->send(200, "text/html", stevesch::PageHandler::kReloader);
  }

  // Send the main index.html page
  void sendIndex(AsyncWebServerRequest *request)
  {
    const char* path = "/index.html";
    if (SPIFFS.exists(path)) {
      request->send(SPIFFS, path, "text/html", false, processor);
    } else {
      handleReloader(request);
    }
  }

  void handleCss(AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/style.css", "text/css");
  }

  void handleJs(AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/pageHandler.js", "text/javascript");
  }

  void handleIndex(AsyncWebServerRequest *request)
  {
    sendIndex(request);
  }

  void handleSet(AsyncWebServerRequest *request)
  {
    Serial.println("handleSet...");
    AsyncWebParameter* nameParam = request->getParam(String("name"));
    if (nameParam) {
      AsyncWebParameter* valueParam = request->getParam(String("value"));
      if (valueParam) {
        const String& valueStr = valueParam->value();
        const String& nameStr = nameParam->value();
        Serial.printf("handleSet: %s=%s\n", nameStr.c_str(), valueStr.c_str());
        request->send(200, "text/html", valueStr.c_str());
        receive(nameStr, valueStr);
        return;
      }
      request->send(406, "text/html", "value required");
      return;
    }
    request->send(406, "text/html", "name required");
  }

  // Handle Web Server Events
  void handleEvents(AsyncEventSourceClient *client)
  {
    if (client->lastId())
    {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message, id as current millis
    // and set reconnect delay to 10s
    client->send("Server attached", NULL, millis(), 10000);
  }

  // void handlePageNotFound(AsyncWebServerRequest* request);
  void handlePageNotFound(AsyncWebServerRequest* request) {
    String str = "Callback: handlePageNotFound";
    Serial.println(str);
    request->redirect("/");
  }

  String processor(const String& var)
  {
    const auto it = processorRegistry.find(var);
    if (it != processorRegistry.end()) {
      const ProcRegistryEntry& entry = it->second;
      return (entry.procFn)(var);
    }
    return String();
  }

  void sendNamedValue(const char* name, const char* value)
  {
    events.send(value, name, millis());
  }

  void receive(const String& name, const String& value)
  {
    const auto it = receiverRegistry.find(name);
    if (it != receiverRegistry.end()) {
      const RecvRegistryEntry& entry = it->second;
      Serial.printf("recvFn: %s=%s\n", name.c_str(), value.c_str());
      (entry.recvFn)(name, value);
    }
    else {
      Serial.printf("recvFn: %s=%s NOT REGISTERED\n", name.c_str(), value.c_str());
    }

    // reflect back to client(s)-- attempt to use processor[s] first, in case
    // the server has altered the received value (applied limits, transformations, etc.)
    if (!stevesch::PageHandler::processAndSendRegisteredValue(name)) {
      // it wasn't registered, so reflect received value (unmodified) back to all clients
      Serial.printf("Unregistered reflect: %s=%s\n", name.c_str(), value.c_str());
      sendNamedValue(name.c_str(), value.c_str());
    }
  }



  String processReflectionList(const String& name)
  {
    String s = "";
    const char* commaSep = ",";
    const char* nextSep = nullptr;
    for(auto&& it : processorRegistry)
    {
      // ProcRegistryEntry& entry = it.second;
      const String& name = it.first;
      if (nextSep) {
        s += nextSep;
      }
      s += "'";
      s += name;
      s += "'";
      nextSep = commaSep;
    }
    return s;
  }
} // namespace

namespace stevesch {
namespace PageHandler {

  void setup(AsyncWebServer& server)
  {
    // Initialize SPIFFS
    if(!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }

    // when serving an HTML file (index.html), replace REFL_LIST with
    // an array of registered variable names:
    registerProcessor("REFL_LIST", processReflectionList);

    // api functions for communication from client
    server.on("/api/set", HTTP_GET, handleSet);
    server.on("/api/reloader", handleReloader); // for testing reloader page

    // serve index page, css, js, etc.
    server.on("/", HTTP_GET, handleIndex);
    server.on("/style.css", HTTP_GET, handleCss);
    server.on("/pageHandler.js", HTTP_GET, handleJs);
    server.onNotFound(handlePageNotFound);

    // handle client connection for server-side events
    events.onConnect(handleEvents);
    server.addHandler(&events);
  }

  void loop()
  {
  }

  bool processAndSendRegisteredValue(const String& name)
  {
    const auto it = processorRegistry.find(name);
    if (it == processorRegistry.end()) {
      // not registered
      return false;
    }

    ProcRegistryEntry& entry = it->second;
    String value = (entry.procFn)(name);
    if (value != entry.lastSentValue)
    {
      entry.lastSentValue = value;
      Serial.printf("sendNamedValue %s=%s\n", name.c_str(), value.c_str());
      sendNamedValue(name.c_str(), value.c_str());
    }
    return true;
  }

  void processAndSendUpdatedServerValues()
  {
    for(auto&& it : processorRegistry)
    {
      ProcRegistryEntry& entry = it.second;
      const String& name = it.first;
      String value = (entry.procFn)(name);
      if (value != entry.lastSentValue)
      {
        entry.lastSentValue = value;
        sendNamedValue(name.c_str(), value.c_str());
      }
    }
  }

  void registerProcessor(const String& var, procFn_t fn)
  {
    ProcRegistryEntry entry;
    entry.procFn = fn;
    processorRegistry.insert({ var, entry });
  }

  void registerReceiver(const String& name, recvFn_t fn)
  {
    RecvRegistryEntry entry;
    entry.recvFn = fn;
    receiverRegistry.insert({ name, entry });
  }

} // namespace PageHandler
} // namespace stevesch
