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
    inline const QString SERVER_IP() {
#ifdef QT_DEBUG
        const QString ip = "192.168.0.111";
#else
        QString ip = qApp->property("SERVER_IP").toString();
#endif
        return ip;
    }

    const int SERVER_PORT = 9000;

    const QString SITE_URL = "https://remotepro.cn/";
    const QString VERSION = "2.7.2";
}
