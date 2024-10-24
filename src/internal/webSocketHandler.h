#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <algorithm>
#include <map>
#include <set>

typedef std::function<void(const String &, const String &)> KeyValueOp;

class WebSocketHandler
{
public:
  WebSocketHandler();
  ~WebSocketHandler();

  void allocateWebSocket(AsyncWebServer *server);
  // void sendSensorUpdates();
  void sendVarBlock(const char* block);
  void clearWebSocket();

  KeyValueOp handleIncomingValue = [](const String&, const String&) {};

private:
  struct ClientData
  {
    std::set<String> subscribedVariables;
  };

  std::map<uint32_t, ClientData> clients;
  AsyncWebSocket *mWebSocket;

  void handleWebSocketMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len);
  void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len);
};