// #include <FreeRTOS.h>
#include <SPIFFS.h>
#include "reloader.h"
// #include <pgmspace.h>   // for PROGMEM
#include <base64.hpp>

#include "pageHandler.h"

// using namespace std::placeholders;
using std::placeholders::_1;
using std::placeholders::_2;

namespace
{
  SemaphoreHandle_t sReceiveQueueKey = xSemaphoreCreateMutex();
  SemaphoreHandle_t sSendQueueKey = xSemaphoreCreateMutex();

  // compare lower-case extensions
  bool shouldServeDirect(const String& ext) {
    if (!ext.length()) {
      return false;
    }
    return (
      (ext == "js") ||
      (ext == "css") ||
      (ext == "png") ||
      (ext == "jpg") ||
      (ext == "jpeg") ||
      (ext == "gif") ||
      (ext == "ico")
    );
  }

  inline bool isHtmlExtension(const String& ext) {
    return (
      (ext == "html") ||
      (ext == "htm")
    );
  }

  // returns lowercase characters after the last dot (excluding dot)
  // (dot only counts if after the last slash)
  String getExtension(const String& url) {
    int n = url.length() - 1;
    // find last dot
    while (n >= 0) {
      char ch = url.charAt(n);
      if (ch == '.') {
        String ext = url.substring(n + 1);
        ext.toLowerCase();
        return ext;
      }
      if (ch == '/') {
        // found separator before dot-- no extension
        break;
      }
      --n;
    }
    return "";
  }
}

namespace stevesch
{
  PageHandler::PageHandler() : mEvents("/events"), mRestartTime(0), mEnableAsync(false)
  {
    // when serving an HTML file (index.html), replace REFL_LIST with
    // an array of registered variable names:
    registerProcessor("REFL_LIST", std::bind(&PageHandler::processReflectionList, this, _1));

    // hook for client connection starting:
    // mEvents.onConnect(std::bind(&PageHandler::handleConnectClient, this, _1));
  }

  void PageHandler::setup()
  {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true))
    {
      // Serial.println("PageHandler: An Error has occurred while mounting SPIFFS.");
    }
  }

  bool filterHtml(AsyncWebServerRequest *request) {
    String ext = getExtension(request->url());
    return isHtmlExtension(ext);
  }

  bool filterServeDirect(AsyncWebServerRequest *request) {
    String ext = getExtension(request->url());
    return shouldServeDirect(ext);
  }

  void PageHandler::connect(AsyncWebServer &server, const char* defaultPage)
  {
    mDefaultPage = defaultPage ? defaultPage : "";

    // api functions for communication from client
    server.on("/api/set", HTTP_GET, std::bind(&PageHandler::handleSet, this, _1));
    server.on("/api/getall", HTTP_GET, std::bind(&PageHandler::handleGetAll, this, _1));
    server.on("/api/reloader", std::bind(&PageHandler::handleReloader, this, _1)); // for testing reloader page
    server.on("/api/restart", std::bind(&PageHandler::handleRestart, this, _1));   // for testing reloader page

    // serve index page, css, js, etc.

    // js and css files get served without modification:
    server.serveStatic("/", SPIFFS, "/").setFilter(filterServeDirect);

    // HTML files go through template processor:
    server.serveStatic("/", SPIFFS, "/")
      .setTemplateProcessor(std::bind(&PageHandler::processField, this, _1))
      .setFilter(filterHtml);

    // root get redirected to index file:
    server.on("/", HTTP_GET, std::bind(&PageHandler::handleIndex, this, _1));

    // server.on("/style.css", HTTP_GET, std::bind(&PageHandler::handleCss, this, _1));
    // server.on("/pageHandler.js", HTTP_GET, std::bind(&PageHandler::handleJs, this, _1));
    server.onNotFound(std::bind(&PageHandler::handlePageNotFound, this, _1));

    server.addHandler(&mEvents);
  }

  void PageHandler::disconnect(AsyncWebServer &server)
  {
  }

  void PageHandler::processReceivedQueue()
  {
    std::deque<ReceivePair> queue;
    xSemaphoreTake(sReceiveQueueKey, portMAX_DELAY);
    if (mReceivedQueue.size()) {
      std::swap(queue, mReceivedQueue);
    }
    xSemaphoreGive(sReceiveQueueKey);

    while (queue.size()) {
      const ReceivePair& data = queue.front();
      receive(data.name, data.value);
      queue.pop_front();
    }
  }

  void PageHandler::loop()
  {
    processReceivedQueue();
    flushSendQueue();

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
      // Serial.printf("queueNamedValue %s=%s\n", name.c_str(), value.c_str());
      queueNamedValue(name.c_str(), value.c_str());
    }
    return true;
  }

  int PageHandler::snoopUpdatedServerValues() const
  {
    int numDirtyEntries = 0;
    for (auto &&it : mProcessorRegistry)
    {
      const ProcRegistryEntry &entry = it.second;
      const String &name = it.first;
      String value = (entry.procFn)(name);
      if (value != entry.lastSentValue)
      {
        ++numDirtyEntries;
      }
    }
    return numDirtyEntries;
  }

  void PageHandler::bundleAllServerValues(String& msgBlockOut)
  {
    const int kYieldPer = 16;
    int numBeforeYield = kYieldPer;
    for (auto &&it : mProcessorRegistry)
    {
      ProcRegistryEntry &entry = it.second;
      const String &name = it.first;
      String value = (entry.procFn)(name);

      const size_t origQueueLength = msgBlockOut.length();
      if (origQueueLength > 0)
      {
        msgBlockOut += ';';
      }
      appendNamedValue(msgBlockOut, name.c_str(), value.c_str());

      if (--numBeforeYield <= 0) {
        yield();
        numBeforeYield = kYieldPer;
      }
    }
  }

  void PageHandler::processAndSendUpdatedServerValues()
  {
    const int kYieldPer = 16;
    int numBeforeYield = kYieldPer;
    for (auto &&it : mProcessorRegistry)
    {
      ProcRegistryEntry &entry = it.second;
      const String &name = it.first;
      String value = (entry.procFn)(name);
      if (value != entry.lastSentValue)
      {
        entry.lastSentValue = value;
        queueNamedValue(name.c_str(), value.c_str());
        if (--numBeforeYield <= 0) {
          yield();
          numBeforeYield = kYieldPer;
        }
      }
    }
  }

  int PageHandler::processAndSendUpdatedServerValues(int maxToSend, int startHint)
  {
    auto&& itBegin = mProcessorRegistry.begin();
    auto&& itEnd = mProcessorRegistry.end();
    
    int startIndex = 0;
    const int totalNum = mProcessorRegistry.size();
    int numToSend = std::min(totalNum, maxToSend);
    auto it = itBegin;
    if (startHint < totalNum) {
      for (int n = startHint; n > 0; --n) {
        ++it;
      }
      startIndex = startHint;
    }
    
    int nextIndex = startIndex;

    while (numToSend > 0) {
      ProcRegistryEntry &entry = it->second;
      const String &name = it->first;
      String value = (entry.procFn)(name);
      if (value != entry.lastSentValue)
      {
        entry.lastSentValue = value;
        queueNamedValue(name.c_str(), value.c_str());
        --numToSend;
      }

      ++nextIndex;
      ++it;
      if (it == itEnd) {
        it = itBegin;
        nextIndex = 0;
      }

      if (nextIndex == startIndex) {
        // wrapped back to initial index-- number that needed to be sent was <=  maxToSend
        nextIndex = 0;  // start at beginning next time
        break;
      }
    }

    return nextIndex;
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
    long now = millis();
    if ((mRestartTime != 0) && (now >= mRestartTime)) {
      // force restart immediately-- loop may not be running
      ESP.restart();
    }
    mRestartTime = now + 1000;
  }

  void PageHandler::handleIndex(AsyncWebServerRequest *request)
  {
    const char *path = "/index.html";
    if (mDefaultPage.length()) {
      path = mDefaultPage.c_str();
    }

    if (SPIFFS.exists(path))
    {
      request->send(SPIFFS, path, "text/html", false, std::bind(&PageHandler::processField, this, _1));
    }
    else
    {
      // if the index file does not exist, it's possible an update is inprogress--
      // assume that's the case and display the reloader/update-in-progress page:
      handleReloader(request);
    }
  }

  // void PageHandler::handleCss(AsyncWebServerRequest *request)
  // {
  //   request->send(SPIFFS, "/style.css", "text/css");
  // }

  // void PageHandler::handleJs(AsyncWebServerRequest *request)
  // {
  //   request->send(SPIFFS, "/pageHandler.js", "text/javascript");
  // }

  // void PageHandler::handleRoot(AsyncWebServerRequest *request)
  // {
  //   if (processFileRequest(request)) {
  //     return;
  //   }
  //   const char *path = "/index.html";
  //   if (processFileRequest(request, path)) {
  //     return;
  //   }

  //   // if (SPIFFS.exists(path))
  //   // {
  //   //   request->send(SPIFFS, path, "text/html", false, std::bind(&PageHandler::processField, this, _1));
  //   // }
  //   handleReloader(request);
  // }


  bool PageHandler::processFileRequest(AsyncWebServerRequest *request, const char* urlOverride)
  {
    bool handled = false;

    String contentType;
    String path;
    if (urlOverride) {
      path = urlOverride;
    }
    else {
      path = request->url();
    }

    if (path.startsWith("/"))
    {
      String lc = path;
      lc.toLowerCase();
      if (lc.endsWith(".htm") || lc.endsWith(".html")) {
        if (SPIFFS.exists(path)) {
          contentType = "text/html";
          request->send(SPIFFS, path, contentType, false, std::bind(&PageHandler::processField, this, _1));
          handled = true;
        }
      } else if (lc.endsWith(".js")) {
        if (SPIFFS.exists(path)) {
          request->send(SPIFFS, path, "text/javascript");
          handled = true;
        }
      } else if (lc.endsWith(".css")) {
        if (SPIFFS.exists(path)) {
          request->send(SPIFFS, path, "text/css");
          handled = true;
        }
      }
    }
    return handled;
  }

  void PageHandler::handleSet(AsyncWebServerRequest *request)
  {
    // Serial.println("handleSet...");
    const AsyncWebParameter *nameParam = request->getParam(String("name"));
    if (nameParam)
    {
      const AsyncWebParameter *valueParam = request->getParam(String("value"));
      if (valueParam)
      {
        const String &valueStr = valueParam->value();
        const String &nameStr = nameParam->value();
        // Serial.printf("handleSet: %s=%s\n", nameStr.c_str(), valueStr.c_str());
        request->send(200, "text/html", valueStr.c_str());
        if (mEnableAsync) {
          receive(nameStr, valueStr);
        }
        else
        {
          xSemaphoreTake(sReceiveQueueKey, portMAX_DELAY);
          mReceivedQueue.emplace_back(ReceivePair { nameStr, valueStr });
          xSemaphoreGive(sReceiveQueueKey);
        }
        return;
      }
      request->send(406, "text/html", "value required");
      return;
    }
    request->send(406, "text/html", "name required");
  }

  void PageHandler::handleGetAll(AsyncWebServerRequest *request)
  {
    String msgBlock;
    bundleAllServerValues(msgBlock);
    request->send(200, "text/html", msgBlock.c_str());

    // String msgBlock;
    // auto&& itBegin = mProcessorRegistry.begin();
    // auto&& itEnd = mProcessorRegistry.end();
    
    // int startIndex = 0;
    // const int totalNum = mProcessorRegistry.size();
    // auto it = itBegin;

    // bool first = true;

    // AsyncWebServerResponse* response = request->beginChunkedResponse(
    //   "text/html",
    //   [&](uint8_t* buffer, size_t maxLen, size_t alreadySent) {
    //     int bytesFilled = 0;
    //     int available = maxLen;
    //     uint8_t* outPtr = buffer;
    //     while (available > 0) {
    //       if (!msgBlock.length()) {
    //         if (it == itEnd) {
    //           break; // done
    //         }
    //         // get next
    //         ProcRegistryEntry &entry = it->second;
    //         const String &name = it->first;
    //         String value = (entry.procFn)(name);

    //         if (!first)
    //         {
    //           msgBlock += ';';
    //         }
    //         appendNamedValue(msgBlock, name.c_str(), value.c_str());
    //         ++it;
    //         first = false;
    //       }

    //       int remain = msgBlock.length();
    //       if (remain) {
    //         const int writtenThisPass = min(remain, available);
    //         int toWrite = writtenThisPass;
    //         const char* inPtr = msgBlock.c_str();
    //         while (toWrite > 0) {
    //           *outPtr = (uint8_t)(*inPtr);
    //           ++bytesFilled;
    //           ++outPtr, ++inPtr;
    //           --toWrite;
    //         }
    //         msgBlock.remove(0, writtenThisPass);
    //         available -= writtenThisPass;
    //       }
    //     }
    //     return bytesFilled;
    //   }
    // );

    // response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    // response->addHeader("Pragma", "no-cache");
    // response->addHeader("Expires", "-1");
    // response->setCode(200);
    // request->send(response);

    // AsyncResponseStream *response = request->beginResponseStream("text/html");
    // response->addHeader("Server", "PageHandler-header-field");

    // String msgBlock;
    // const int kYieldPer = 16;
    // int numBeforeYield = kYieldPer;
    // bool first = true;
    // for (auto &&it : mProcessorRegistry)
    // {
    //   ProcRegistryEntry &entry = it.second;
    //   const String &name = it.first;
    //   String value = (entry.procFn)(name);

    //   msgBlock.clear();
    //   appendNamedValue(msgBlock, name.c_str(), value.c_str());

    //   if (--numBeforeYield <= 0) {
    //     yield();
    //     numBeforeYield = kYieldPer;
    //   }
    //   if (!first)
    //   {
    //     response->print(';');
    //   }
    //   response->print(msgBlock);
    //   first = false;
    // }
    // request->send(response);
  }

  // void PageHandler::handleConnectClient(AsyncEventSourceClient *client)
  // {
  //   Serial.printf("Client connected, lastId: %u\n", client->lastId());
  // }

  // void handlePageNotFound(AsyncWebServerRequest* request);
  void PageHandler::handlePageNotFound(AsyncWebServerRequest *request)
  {
    Serial.printf("### Request not found.  url: %s\n", request->url().c_str());
    // size_t args = request->args();
    // Serial.printf("# args: %d\n", request->args());
    // for (size_t i = 0; i < args; ++i) {
    //   Serial.printf("# [%d]: %s\n", i, request->arg(i).c_str());
    // }
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

  // void PageHandler::sendNamedValue(const char *name, const char *value)
  // {
  //   if (WiFi.isConnected())
  //   {
  //     mEvents.send(value, name, millis());
  //   }
  // }

  static void encodeStringDynamic(String& msgBlockOut, const char* str, size_t len, size_t lenEncoded)
  {
    char *strBase64 = new char[lenEncoded + 1];
    unsigned char* p0 = (unsigned char*)(const_cast<char*>(str));
    encode_base64(p0, len, (unsigned char*)strBase64);
    msgBlockOut += strBase64;
    delete[] strBase64;
  }

  static void encodeStringStack(String& msgBlockOut, const char* str, size_t len, size_t lenEncoded)
  {
    char strBase64[lenEncoded + 1];
    unsigned char* p0 = (unsigned char*)(const_cast<char*>(str));
    encode_base64(p0, len, (unsigned char*)strBase64);
    msgBlockOut += strBase64;
  }

  static void encodeString(String& msgBlockOut, const char* str, size_t len, size_t lenEncoded)
  {
    // dynamic allocation is used (rather than stack space) if the encoded
    // string is at least this long:
    const size_t kStackStringCutoff = 256;

    if (lenEncoded < kStackStringCutoff)
    {
      encodeStringStack(msgBlockOut, str, len, lenEncoded);
    }
    else
    {
      encodeStringDynamic(msgBlockOut, str, len, lenEncoded);
    }
  }

  static void encodeVar(String& msgBlockOut,
    const char* name, size_t lenName, size_t lenNameEncoded,
    const char* value, size_t lenValue, size_t lenValueEncoded)
  {
    encodeString(msgBlockOut, name, lenName, lenNameEncoded);
    msgBlockOut += ',';
    encodeString(msgBlockOut, value, lenValue, lenValueEncoded);
  }

  void PageHandler::appendNamedValue(String& msgBlockOut, const char *name, const char *value)
  {
    size_t lenName = strlen(name);
    size_t lenValue = strlen(value);
    size_t lenNameEncoded = encode_base64_length(lenName);
    size_t lenValueEncoded = encode_base64_length(lenValue);
    encodeVar(msgBlockOut, name, lenName, lenNameEncoded, value, lenValue, lenValueEncoded);
  }

  void PageHandler::queueNamedValue(const char *name, const char *value)
  {
    size_t lenName = strlen(name);
    size_t lenValue = strlen(value);
    size_t lenNameEncoded = encode_base64_length(lenName);
    size_t lenValueEncoded = encode_base64_length(lenValue);

    xSemaphoreTake(sSendQueueKey, portMAX_DELAY);
    const size_t origQueueLength = mSendQueue.length();
    if (origQueueLength > 0)
    {
      constexpr size_t kMaxSendQueueChars = 1024;
      constexpr size_t overhead = 2; // for , and potential ;
      const size_t finalLength = origQueueLength + lenNameEncoded + lenValueEncoded + overhead;
      if (finalLength > kMaxSendQueueChars)
      {
        _flushSendQueue();
      }
      else
      {
        mSendQueue += ';';
      }
    }

    encodeVar(mSendQueue, name, lenName, lenNameEncoded, value, lenValue, lenValueEncoded);
    xSemaphoreGive(sSendQueueKey);
  }

  void PageHandler::_flushSendQueue()
  {
    if (mSendQueue.length() > 0) {
      // if not connected, we don't even try to actually send:
      if (WiFi.isConnected())
      {
        mEvents.send(mSendQueue.c_str(), "_M_", millis());
      }
      mSendQueue.clear();
    }
  }

  void PageHandler::flushSendQueue()
  {
    xSemaphoreTake(sSendQueueKey, portMAX_DELAY);
    _flushSendQueue();
    xSemaphoreGive(sSendQueueKey);
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
      // Serial.printf("Unregistered reflect: [%d]=[%d]\n", name.length(), value.length());
      // Serial.printf("Unregistered reflect: %s=%s\n", name.c_str(), value.c_str());
      queueNamedValue(name.c_str(), value.c_str());
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
