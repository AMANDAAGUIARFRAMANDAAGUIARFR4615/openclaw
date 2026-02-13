#pragma once

#include "Logger.h"
#include "ToastWidget.h"
#include "WebSocketClient.h"
#include <QSettings>
#include <QElapsedTimer>
#include <QNetworkAccessManager>

extern QSettings* settings;
extern WebSocketClient* webSocketClient;
extern QElapsedTimer* elapsedTimer;
extern QNetworkAccessManager* networkAccessManager;

namespace Config {
#ifdef QT_DEBUG
    const QString SERVER_IP = "192.168.0.111";
    // const QString SERVER_IP = "8.210.25.235";
#else
    // const QString SERVER_IP = "43.167.226.242";
    const QString SERVER_IP = "47.107.154.171";
#endif

    const int SERVER_PORT = 9000;

    const QString SITE_URL = "https://remotepro.cn";
    const QString VERSION = "2.0.8";
}
