#pragma once

#include "Tools.h"
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

/** 将投屏原始码流按「每整十分钟」一个文件落盘（与 QMediaPlayer 同源字节，一般为 H.264 裸流 .h264）。 */
class ScreenStreamRecorder {
public:
    explicit ScreenStreamRecorder(QString deviceId) : deviceId_(std::move(deviceId)) {}

    ~ScreenStreamRecorder() { closeFile(); }

    void append(const QByteArray &data) {
        if (data.isEmpty())
            return;

        const QString key = slotKeyFor(QDateTime::currentDateTime());
        if (key != slotKey_) {
            closeFile();
            slotKey_ = key;
            openForSlot();
        }
        if (out_.isOpen())
            out_.write(data);
    }

private:
    static QString slotKeyFor(const QDateTime &now) {
        const QTime t = now.time();
        const int m = (t.minute() / 10) * 10;
        const QDateTime start(now.date(), QTime(t.hour(), m, 0));
        return start.toString(QStringLiteral("yyyyMMdd_HHmm"));
    }

    void closeFile() {
        if (out_.isOpen()) {
            out_.flush();
            out_.close();
        }
    }

    void openForSlot() {
        const QString dirPath = Tools::screenVideoSaveDirectory(deviceId_);
        QDir().mkpath(dirPath);
        const QString path = QDir(dirPath).filePath(QStringLiteral("screenvid_%1.h264").arg(slotKey_));
        out_.setFileName(path);
        const bool exists = QFileInfo::exists(path);
        const QIODevice::OpenMode mode =
            exists ? (QIODevice::WriteOnly | QIODevice::Append) : QIODevice::WriteOnly;
        if (!out_.open(mode))
            slotKey_.clear();
    }

    QString deviceId_;
    QString slotKey_;
    QFile out_;
};
