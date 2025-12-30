#include "global.h"

QSettings settings("deepseek", "RemotePro");
WebSocketClient *webSocketClient = new WebSocketClient();
QElapsedTimer* elapsedTimer = new QElapsedTimer();
