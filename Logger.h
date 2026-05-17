#pragma once

#define LOG_PREFIX() QStringLiteral("%1:%2@%3 | ") \
    .arg(__FILE__) \
    .arg(__LINE__) \
    .arg(__FUNCTION__)

#if defined(QT_NO_DEBUG_OUTPUT)
#define qDebugEx()    while (false) qDebug().noquote()
#else
#define qDebugEx()    qDebug().noquote() << LOG_PREFIX()
#endif

#if defined(QT_NO_INFO_OUTPUT)
#define qInfoEx()     while (false) qInfo().noquote()
#else
#define qInfoEx()     qInfo().noquote() << LOG_PREFIX()
#endif

#if defined(QT_NO_WARNING_OUTPUT)
#define qWarningEx()  while (false) qWarning().noquote()
#else
#define qWarningEx()  qWarning().noquote() << LOG_PREFIX()
#endif

#define qCriticalEx() qCritical().noquote() << LOG_PREFIX()
