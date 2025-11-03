#pragma once

#include "Logger.h"
#include "qrcodegen.hpp"
#include <QJsonObject>
#include <QFile>
#include <QCryptographicHash>
#include <QImage>
#include <QBuffer>
#include <QProcess>
#include <QDir>
#include <QPainter>

using namespace qrcodegen;

class Tools {
public:
    // 计算文件的 MD5 值
    static QString getFileMd5(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "无法打开文件: " << filePath;
            return QString();
        }

        QCryptographicHash hash(QCryptographicHash::Md5);
        if (!hash.addData(&file)) {
            qWarning() << "无法读取文件数据: " << filePath;
            return QString();
        }

        return hash.result().toHex();
    }

    // 计算字符串的 MD5 值
    static QString getStringMd5(const QString &input) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(input.toUtf8());
        return hash.result().toHex();
    }

    // 获取文件大小
    static qint64 getFileSize(const QString &filePath) {
        QFile file(filePath);
        if (!file.exists()) {
            qWarning() << "文件不存在: " << filePath;
            return -1;  // 文件不存在时返回 -1
        }

        return file.size();
    }

    static QString formatByteSize(qint64 size) {
        if (size < 0)
            return "";
        if (size < 1024)
            return QString::number(size) + " B";
        if (size < 1024 * 1024)
            return QString::number(size / 1024.0, 'f', 2) + " KB";
        if (size < 1024 * 1024 * 1024)
            return QString::number(size / (1024.0 * 1024), 'f', 2) + " MB";
        return QString::number(size / (1024.0 * 1024 * 1024), 'f', 2) + " GB";
    }

    static qint64 parseByteSize(const QString &text)
    {
        QString str = text.trimmed().toUpper();

        if (str.isEmpty())
            return -1;

        static const QRegularExpression regex(
            R"(^\s*([0-9]*\.?[0-9]+)\s*([KMGT]?B)?\s*$)"
        );
        QRegularExpressionMatch match = regex.match(str);
        if (!match.hasMatch())
            return -1;

        double value = match.captured(1).toDouble();
        QString unit = match.captured(2);

        if (unit == "B" || unit.isEmpty())
            return static_cast<qint64>(value);
        else if (unit == "KB")
            return static_cast<qint64>(value * 1024);
        else if (unit == "MB")
            return static_cast<qint64>(value * 1024 * 1024);
        else if (unit == "GB")
            return static_cast<qint64>(value * 1024 * 1024 * 1024);
        else if (unit == "TB")
            return static_cast<qint64>(value * 1024 * 1024 * 1024 * 1024);

        return -1;
    }

    static void showInFileExplorer(const QString& path) {
#if defined(Q_OS_WIN)
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer.exe", args);
#elif defined(Q_OS_MAC)
        QString escapedFilePath = path;
        escapedFilePath.replace(" ", "\\ ");  // Escape spaces in path
        
        QStringList scriptArgs;
        scriptArgs << "-e"
                   << QString("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(escapedFilePath);
        QProcess::execute("/usr/bin/osascript", scriptArgs);
        QProcess::execute("/usr/bin/osascript", QStringList() << "-e" << "tell application \"Finder\" to activate");
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
    }

    static QString imageToBase64(const QImage &image) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);

        image.save(&buffer, "PNG");

        return byteArray.toBase64();
    }

    static QImage generateQrImage(const QString& text, int scale = 10, int border = 4) {
        qDebugEx() << "generateQrImage" << text;

        QrCode qr = QrCode::encodeText(text.toStdString().c_str(), QrCode::Ecc::MEDIUM);

        int qrSize = qr.getSize();
        int size = (qrSize + border * 2) * scale;
        QImage image(size, size, QImage::Format_RGB888);
        image.fill(Qt::white);  // 白底

        QPainter painter(&image);
        painter.setBrush(Qt::black);
        painter.setPen(Qt::NoPen);

        for (int y = 0; y < qrSize; y++) {
            for (int x = 0; x < qrSize; x++) {
                if (qr.getModule(x, y)) {
                    int rx = (x + border) * scale;
                    int ry = (y + border) * scale;
                    painter.drawRect(rx, ry, scale, scale);
                }
            }
        }
        
        return image;
    }
};
