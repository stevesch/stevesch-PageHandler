#include <ArduinoJson.h>

#include "./webSocketHandler.h"

// #include <ArduinoJson.h>

WebSocketHandler::WebSocketHandler() : mWebSocket(0)
{
}

WebSocketHandler::~WebSocketHandler()
{
  clearWebSocket();
}

// Allocate the WebSocket and attach the event handler
void WebSocketHandler::allocateWebSocket(AsyncWebServer *server)
{
  if (mWebSocket)
  {
    clearWebSocket();
  }
  mWebSocket = new AsyncWebSocket("/ws");
  mWebSocket->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
              { this->onEvent(server, client, type, arg, data, len); });
  server->addHandler(mWebSocket);
}

// Send sensor updates to clients based on their subscriptions
// void WebSocketHandler::sendSensorUpdates()
// {
//   StaticJsonDocument<200> jsonDoc;

//   for (auto &clientEntry : clients)
//   {
//     uint32_t clientID = clientEntry.first;
//     const std::set<String> &subscriptions = clientEntry.second.subscribedVariables;

//     jsonDoc.clear();

//     if (subscriptions.find("sensor1") != subscriptions.end())
//     {
//       jsonDoc["sensor1"] = sensorValue1;
//     }
//     if (subscriptions.find("sensor2") != subscriptions.end())
//     {
//       jsonDoc["sensor2"] = sensorValue2;
//     }
//     if (subscriptions.find("param1") != subscriptions.end())
//     {
//       jsonDoc["param1"] = param1;
//     }
//     if (subscriptions.find("param2") != subscriptions.end())
//     {
//       jsonDoc["param2"] = param2;
//     }

//     if (!jsonDoc.isNull())
//     {
//       String message;
//       serializeJson(jsonDoc, message);
//       mWebSocket->text(clientID, message);
//     }
//   }
// }

void WebSocketHandler::sendVarBlock(const char* block)
{
  for (auto &clientEntry : clients)
  {
    uint32_t clientID = clientEntry.first;
    mWebSocket->text(clientID, block);
  }
}

// Handle resetting the WebSocket and clearing client data after Wi-Fi reconnects
void WebSocketHandler::clearWebSocket()
{
  delete mWebSocket;
  mWebSocket = 0;
  clients.clear();
}


// Handle WebSocket messages from clients (subscriptions and parameter changes)
void WebSocketHandler::handleWebSocketMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len)
{
  StaticJsonDocument<200> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, data, len);

  if (error)
  {
    Serial.println("Failed to parse WebSocket message");
    return;
  }

  uint32_t clientID = client->id();

  if (jsonDoc.containsKey("subscribe"))
  {
    JsonArray subscribeArray = jsonDoc["subscribe"].as<JsonArray>();
    clients[clientID].subscribedVariables.clear();

    for (JsonVariant value : subscribeArray)
    {
      clients[clientID].subscribedVariables.insert(value.as<String>());
    }

    Serial.printf("Client %u subscribed to: ", clientID);
    for (const auto &var : clients[clientID].subscribedVariables)
    {
      Serial.print(var + " ");
    }
    Serial.println();
  }

  if (jsonDoc.containsKey("set"))
  {
    JsonObject kvObject = jsonDoc["set"].as<JsonObject>();
    for (auto iter=kvObject.begin(); iter != kvObject.end(); ++iter)
    {
      const auto keyRef = iter->key();
      const auto valueRef = iter->value().as<const char*>();
      String keyStr(keyRef.c_str());
      String valueStr = valueRef ? valueRef : "";
      Serial.printf("Updating %s: %s\n", keyStr.c_str(), valueStr.c_str());
      this->handleIncomingValue(keyStr, valueStr);
    }
  }
}

// WebSocket event handler function
void WebSocketHandler::onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                               void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("Client %u connected\n", client->id());
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("Client %u disconnected\n", client->id());
    clients.erase(client->id());
  }
  else if (type == WS_EVT_DATA)
  {
    handleWebSocketMessage(client, data, len);
  }
}
