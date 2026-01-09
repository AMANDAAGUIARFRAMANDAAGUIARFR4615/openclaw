#include "global.h"

QSettings* settings = new QSettings("deepseek", "RemotePro");
WebSocketClient *webSocketClient = new WebSocketClient();
QElapsedTimer* elapsedTimer = new QElapsedTimer();
QNetworkAccessManager* networkAccessManager = new QNetworkAccessManager();
