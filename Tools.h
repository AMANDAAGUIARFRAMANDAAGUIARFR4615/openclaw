#pragma once

#include "TcpServer.h"

#include <QJsonObject>
#include <QFile>
#include <QCryptographicHash>
#include <QImage>
#include <QBuffer>

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

    static QString imageToBase64(const QImage &image) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);

        image.save(&buffer, "PNG");

        return byteArray.toBase64();
    }
};
