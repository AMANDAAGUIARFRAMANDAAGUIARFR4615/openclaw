#pragma once

#define LOG_PREFIX() QStringLiteral("%1:%2@%3 | ") \
    .arg(__FILE__) \
    .arg(__LINE__) \
    .arg(__FUNCTION__)

#define qDebugEx()    qDebug().noquote() << LOG_PREFIX()
#define qInfoEx()     qInfo().noquote() << LOG_PREFIX()
#define qWarningEx()  qWarning().noquote() << LOG_PREFIX()
#define qCriticalEx() qCritical().noquote() << LOG_PREFIX()
