#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "config.h"

extern WebServer server;
extern WebSocketsServer webSocket;

bool initWiFi();
void setupWebServer();
void handleRoot();
void handleStream();

#endif
