#include "pageHandler.h"
#include "reloader.h"
#include <SPIFFS.h>
// #include <pgmspace.h>   // for PROGMEM

// using namespace std::placeholders;
using std::placeholders::_1;
using std::placeholders::_2;

namespace stevesch
{

  PageHandler::PageHandler() : mEvents("/events"), mRestartTime(0)
  {
    // when serving an HTML file (index.html), replace REFL_LIST with
    // an array of registered variable names:
    registerProcessor("REFL_LIST", std::bind(&PageHandler::processReflectionList, this, _1));

    // handle client connection for server-side events
    mEvents.onConnect(std::bind(&PageHandler::handleEvents, this, _1));
  }

  void PageHandler::setup()
  {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true))
    {
      // Serial.println("PageHandler: An Error has occurred while mounting SPIFFS.");
    }
  }

  void PageHandler::connect(AsyncWebServer &server)
  {
    // api functions for communication from client
    server.on("/api/set", HTTP_GET, std::bind(&PageHandler::handleSet, this, _1));
    server.on("/api/reloader", std::bind(&PageHandler::handleReloader, this, _1)); // for testing reloader page
    server.on("/api/restart", std::bind(&PageHandler::handleRestart, this, _1));   // for testing reloader page

    // serve index page, css, js, etc.
    server.on("/", HTTP_GET, std::bind(&PageHandler::handleIndex, this, _1));
    server.on("/style.css", HTTP_GET, std::bind(&PageHandler::handleCss, this, _1));
    server.on("/pageHandler.js", HTTP_GET, std::bind(&PageHandler::handleJs, this, _1));
    server.onNotFound(std::bind(&PageHandler::handlePageNotFound, this, _1));

    server.addHandler(&mEvents);
  }

  void PageHandler::disconnect(AsyncWebServer &server)
  {
  }

  void PageHandler::loop()
  {
    if (mRestartTime)
    {
      if (millis() >= mRestartTime)
      {
        ESP.restart();
      }
    }
  }

  bool PageHandler::processAndSendRegisteredValue(const String &name)
  {
    const auto it = mProcessorRegistry.find(name);
    if (it == mProcessorRegistry.end())
    {
      // not registered
      return false;
    }

    ProcRegistryEntry &entry = it->second;
    String value = (entry.procFn)(name);
    if (value != entry.lastSentValue)
    {
      entry.lastSentValue = value;
      // Serial.printf("sendNamedValue %s=%s\n", name.c_str(), value.c_str());
      sendNamedValue(name.c_str(), value.c_str());
    }
    return true;
  }

  void PageHandler::processAndSendUpdatedServerValues()
  {
    for (auto &&it : mProcessorRegistry)
    {
      ProcRegistryEntry &entry = it.second;
      const String &name = it.first;
      String value = (entry.procFn)(name);
      if (value != entry.lastSentValue)
      {
        entry.lastSentValue = value;
        sendNamedValue(name.c_str(), value.c_str());
      }
    }
  }

  void PageHandler::registerProcessor(const String &var, procFn_t fn)
  {
    ProcRegistryEntry entry;
    entry.procFn = fn;
    mProcessorRegistry.insert({var, entry});
  }

  void PageHandler::registerReceiver(const String &name, recvFn_t fn)
  {
    RecvRegistryEntry entry;
    entry.recvFn = fn;
    mReceiverRegistry.insert({name, entry});
  }

  /////////////////////////////////////////////////////////////////////////////

  // Send code-based page for reloader (for when SPIFFS is not available)
  void PageHandler::handleReloader(AsyncWebServerRequest *request)
  {
    String str(stevesch::kPageHandlerReloader);
    str.replace("%TITLE%", "Updating...");
    str.replace("%MESSAGE%", "Update in progress");
    request->send(200, "text/html", str);
  }

  void PageHandler::handleRestart(AsyncWebServerRequest *request)
  {
    String str(stevesch::kPageHandlerReloader);
    str.replace("%TITLE%", "Restarting...");
    str.replace("%MESSAGE%", "Restarting");
    request->send(200, "text/html", str);
    mRestartTime = millis() + 1000;
  }

  void PageHandler::handleCss(AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/style.css", "text/css");
  }

  void PageHandler::handleJs(AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/pageHandler.js", "text/javascript");
  }

  void PageHandler::handleIndex(AsyncWebServerRequest *request)
  {
    const char *path = "/index.html";
    if (SPIFFS.exists(path))
    {
      request->send(SPIFFS, path, "text/html", false, std::bind(&PageHandler::processField, this, _1));
    }
    else
    {
      handleReloader(request);
    }
  }

  void PageHandler::handleSet(AsyncWebServerRequest *request)
  {
    // Serial.println("handleSet...");
    AsyncWebParameter *nameParam = request->getParam(String("name"));
    if (nameParam)
    {
      AsyncWebParameter *valueParam = request->getParam(String("value"));
      if (valueParam)
      {
        const String &valueStr = valueParam->value();
        const String &nameStr = nameParam->value();
        // Serial.printf("handleSet: %s=%s\n", nameStr.c_str(), valueStr.c_str());
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
  void PageHandler::handleEvents(AsyncEventSourceClient *client)
  {
    // if (client->lastId())
    // {
    //   Serial.printf("Client reconnected.  Last message ID: %u\n", client->lastId());
    // }
    // send event with message, id as current millis
    // and set reconnect delay to 10s
    client->send("Server attached", NULL, millis(), 10000);
  }

  // void handlePageNotFound(AsyncWebServerRequest* request);
  void PageHandler::handlePageNotFound(AsyncWebServerRequest *request)
  {
    // String str = "Callback: handlePageNotFound";
    // Serial.println(str);
    request->redirect("/");
  }

  String PageHandler::processField(const String &var)
  {
    const auto it = mProcessorRegistry.find(var);
    if (it != mProcessorRegistry.end())
    {
      const ProcRegistryEntry &entry = it->second;
      return (entry.procFn)(var);
    }
    return String();
  }

  void PageHandler::sendNamedValue(const char *name, const char *value)
  {
    mEvents.send(value, name, millis());
  }

  void PageHandler::receive(const String &name, const String &value)
  {
    const auto it = mReceiverRegistry.find(name);
    if (it != mReceiverRegistry.end())
    {
      const RecvRegistryEntry &entry = it->second;
      // Serial.printf("recvFn: %s=%s\n", name.c_str(), value.c_str());
      (entry.recvFn)(name, value);
    }
    // else
    // {
    //   Serial.printf("recvFn: %s=%s NOT REGISTERED\n", name.c_str(), value.c_str());
    // }

    // reflect back to client(s)-- attempt to use processor[s] first, in case
    // the server has altered the received value (applied limits, transformations, etc.)
    if (!stevesch::PageHandler::processAndSendRegisteredValue(name))
    {
      // it wasn't registered, so reflect received value (unmodified) back to all clients
      // Serial.printf("Unregistered reflect: %s=%s\n", name.c_str(), value.c_str());
      sendNamedValue(name.c_str(), value.c_str());
    }
  }

  String PageHandler::processReflectionList(const String &name)
  {
    // resulting string format:
    // value1","value2","value3","value4
    // note lack of leading/trailing double-quotes, because document should have
    // window.reflections = ["%REFL_LIST%"];
    String s = "";
    const char *wordSep = "\",\"";
    const char *nextSep = nullptr;
    for (auto &&it : mProcessorRegistry)
    {
      // ProcRegistryEntry& entry = it.second;
      const String &name = it.first;
      if (nextSep)
      {
        s += nextSep;
      }
      s += name;
      nextSep = wordSep;
    }
    return s;
  }
} // namespace stevesch
