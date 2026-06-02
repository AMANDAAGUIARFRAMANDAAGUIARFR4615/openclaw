#pragma once

#include "Logger.h"
#include "ToastWidget.h"
#include "WebSocketClient.h"
#include "TunnelClient.h"
#include <QSettings>
#include <QElapsedTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <algorithm>

/** 等同 qBound(min, val, max)，但 min > max 时不触发 Qt 断言。 */
template<typename T>
constexpr T safeBound(const T &min, const T &val, const T &max)
{
    return std::min(max, std::max(min, val));
}

/** 不依赖 RTTI，将 QEvent 安全转为 QMouseEvent；非鼠标事件返回 nullptr。 */
inline QMouseEvent *asMouseEvent(QEvent *event)
{
    if (!event)
        return nullptr;

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        return static_cast<QMouseEvent *>(event);
    default:
        return nullptr;
    }
}

extern QSettings* settings;
extern WebSocketClient* webSocketClient;
extern QElapsedTimer* elapsedTimer;
extern QNetworkAccessManager* networkAccessManager;
extern TunnelClient* tunnelClient;

/** 为 0 时隐藏投屏窗口菜单中的「脚本工具」二级菜单（含层级树、配置编辑器、图片工具）。 */
#ifndef REMOTEPRO_FEATURE_SCRIPT_TOOLS_MENU
#define REMOTEPRO_FEATURE_SCRIPT_TOOLS_MENU 0
#endif

namespace Config {
    static constexpr qint64 WAN_FILE_TRANSFER_SIZE_LIMIT = 5LL * 1024 * 1024;

    inline bool isWanMode() {
        return settings && !settings->value("isLanMode", true).toBool();
    }

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
    const QString VERSION = "2.8.9";
}
