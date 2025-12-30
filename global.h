#pragma once

#include "Logger.h"
#include "ToastWidget.h"
#include "WebSocketClient.h"
#include <QSettings>
#include <QElapsedTimer>

extern QSettings settings;
extern WebSocketClient* webSocketClient;
extern QElapsedTimer* elapsedTimer;
