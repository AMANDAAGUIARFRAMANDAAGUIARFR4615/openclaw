#pragma once

#include "TcpServer.h"

#include <QJsonObject>
#include <QTcpSocket>
#include <QFile>
#include <QCryptographicHash>
#include <QImage>
#include <QBuffer>

class Tools {
public:
    // 解锁
    static void unlockScreen(QTcpSocket* socket) {
        sendEvent(socket, "changeScreenLockedStatus", 0);
    }

    // 锁屏
    static void lockScreen(QTcpSocket* socket) {
        sendEvent(socket, "changeScreenLockedStatus", 1);
    }

    static void homeScreen(QTcpSocket* socket) {
        sendEvent(socket, "homeScreen");
    }

    static void killAllApp(QTcpSocket* socket) {
        sendEvent(socket, "killAllApp");
    }

    // 重启
    static void reboot(QTcpSocket* socket) {
        sendEvent(socket, "reboot");
    }

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

    static QString formatByteSize(quint64 size) {
        if (size < 1024)
            return QString::number(size) + " B";
        if (size < 1024 * 1024)
            return QString::number(size / 1024.0, 'f', 2) + " KB";
        if (size < 1024 * 1024 * 1024)
            return QString::number(size / (1024.0 * 1024), 'f', 2) + " MB";
        return QString::number(size / (1024.0 * 1024 * 1024), 'f', 2) + " GB";
    }

    static QString imageToBase64(const QImage &image) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);

        image.save(&buffer, "PNG");

        return byteArray.toBase64();
    }

private:
    static void sendEvent(QTcpSocket* socket, const QString& event, int data = -1) {
        QJsonObject jsonObject;
        jsonObject["event"] = event;
        
        if (data != -1)
            jsonObject["data"] = data;
        
        TcpServer::sendData(socket, jsonObject);
    }
};
