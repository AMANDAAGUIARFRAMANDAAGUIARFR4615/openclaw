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
#include <QLayout>
#include <QWidget>
#include <QToolTip>
#include <QOperatingSystemVersion>
#include <QSettings>
#include <QDirIterator>
#include <QDesktopServices>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

using namespace qrcodegen;

class Tools {
public:
    static constexpr bool isDebug() {
#ifdef QT_DEBUG
        return true;
#else
        return false;
#endif
    }

    // 计算文件的 MD5 值
    static QString getFileMd5(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qCriticalEx() << "无法打开文件: " << filePath;
            return QString();
        }

        QCryptographicHash hash(QCryptographicHash::Md5);
        if (!hash.addData(&file)) {
            qCriticalEx() << "无法读取文件数据: " << filePath;
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
            qCriticalEx() << "文件不存在: " << filePath;
            return -1;
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

        float value = match.captured(1).toFloat();
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
        if (!QFileInfo::exists(path)) {
            QToolTip::showText(QCursor::pos(), "文件不存在");
            return; 
        }

#if defined(Q_OS_WIN)
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer.exe", args);
#elif defined(Q_OS_MACOS)
        QString escapedFilePath = path;
        escapedFilePath.replace(" ", "\\ ");  // Escape spaces in path
        
        QStringList scriptArgs;
        scriptArgs << "-e"
                   << QString("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(escapedFilePath);
        QProcess::execute("/usr/bin/osascript", scriptArgs);
        QProcess::execute("/usr/bin/osascript", QStringList() << "-e" << "tell application \"Finder\" to activate");
#elif !defined(Q_OS_WASM)
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

    static void setLayoutVisible(QLayout *layout, bool visible) {
        for (int i = 0; i < layout->count(); ++i) {
            auto widget = layout->itemAt(i)->widget();
            if (widget)
                widget->setVisible(visible);
        }
    }

    static bool isWindows11() {
        auto os = QOperatingSystemVersion::current();
        // Windows 11 内核版本仍是 10.0，但 Build Number 至少是 22000
        return os.majorVersion() == 10 && os.microVersion() >= 22000;
    }

    static bool isAppleMobileDeviceSupportInstalled()
    {
#ifdef Q_OS_WIN
        // 需要检查两个位置，因为在64位系统上，32位程序和64位程序的注册表位置不同
        QStringList registryPaths = {
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
        };

        for (const QString &path : registryPaths) {
            QSettings settings(path, QSettings::NativeFormat);
            QStringList groups = settings.childGroups();

            for (const QString &group : groups) {
                settings.beginGroup(group);
                QString displayName = settings.value("DisplayName").toString();
                settings.endGroup();

                if (displayName.contains("Apple Mobile Device", Qt::CaseInsensitive)) {
                    qDebugEx() << "Found installed software:" << displayName;
                    return true;
                }
            }
        }

        return false;
#else
        return true;
#endif
    }

    /**
     * @brief 递归删除指定目录下符合过滤条件的所有文件
     * @param targetPath 要扫描的根目录
     * @param nameFilters 文件名过滤器列表 (例如: {"*.old", "*.tmp"})
     */
    static void removeFilesRecursively(const QString &targetPath, const QStringList &nameFilters)
    {
        QDirIterator it(targetPath, nameFilters, QDir::Files, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            QString filePath = it.next();
            
            if (QFile::remove(filePath))
                qDebugEx() << "成功删除:" << filePath;
            else
                qInfoEx() << "删除失败 (可能被占用或无权限):" << filePath;
        }
    }

    static bool isStartedByQtCreator() {
#ifdef Q_OS_WIN
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        DWORD myPid = GetCurrentProcessId();
        DWORD parentPid = 0;
        QString parentName;

        // 第一次遍历：找到当前进程，获取其父进程 PID
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID == myPid) {
                    parentPid = pe.th32ParentProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }

        // 第二次遍历：根据父进程 PID 获取父进程的程序名
        if (parentPid != 0 && Process32FirstW(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID == parentPid) {
                    parentName = QString::fromWCharArray(pe.szExeFile).toLower();
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);

        return parentName == "qtcreator.exe" || parentName == "gdborig.exe";
#else
        return false;
#endif
    }
};
