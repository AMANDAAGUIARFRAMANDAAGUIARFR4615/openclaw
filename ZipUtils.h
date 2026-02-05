#pragma once

#include "Logger.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QDateTime>
#include <QtCore/private/qzipreader_p.h>

/**
 * @brief 通用 ZIP 操作工具类
 * 基于 Qt 私有 API (QZipReader) 封装。
 * 提供了从内存读取、单文件解压、全量解压以及智能差异化解压的功能。
 */
class ZipUtils {
public:
    /**
     * @brief 智能解压 (Smart Extract)
     * 遍历 ZIP 包并解压到目标目录，采用差异化更新策略：
     * 1. [新增] 目标文件不存在 -> 直接解压写入。
     * 2. [跳过] 目标文件存在且内容完全一致 -> 跳过不处理（保留原文件修改时间，减少磁盘IO）。
     * 3. [更新] 目标文件存在但内容不同 -> 将本地旧文件重命名为 .old 进行备份，然后写入新文件。
     * @param zipPath ZIP 文件的完整路径
     * @param destDir 解压的目标根目录
     * @return bool 全部文件处理成功返回 true；只要有一个文件写入失败则返回 false。
     */
    static bool extractSmart(const QString &zipPath, const QString &destDir) {
        QZipReader reader(zipPath);
        if (!isReaderValid(reader, zipPath)) return false;

        // 确保目标根目录存在
        QDir rootDir(destDir);
        if (!rootDir.exists() && !rootDir.mkpath(".")) {
            qCriticalEx() << "ZipUtils: 无法创建目标根目录 ->" << destDir;
            return false;
        }

        bool allSuccess = true;
        const auto fileInfos = reader.fileInfoList();

        qInfoEx() << "ZipUtils: 开始智能解压 [" << zipPath << "] 到 [" << destDir << "]";

        for (const auto &info : fileInfos) {
            QString destFilePath = rootDir.filePath(info.filePath);

            // [处理目录]
            if (info.isDir) {
                QDir().mkpath(destFilePath);
                continue;
            }

            // [处理文件]
            // 确保父目录存在
            QFileInfo destFi(destFilePath);
            QDir().mkpath(destFi.dir().path());

            // 读取 ZIP 中的数据到内存
            QByteArray zipData = reader.fileData(info.filePath);
            if (zipData.isEmpty() && info.size > 0) {
                qWarning() << "ZipUtils: 读取压缩包内文件失败 ->" << info.filePath;
                allSuccess = false;
                continue;
            }

            QFile localFile(destFilePath);

            // 策略 1: 目标文件不存在 -> 直接写入
            if (!localFile.exists()) {
                if (writeToFile(destFilePath, zipData)) {
                    qDebugEx() << "[新增] " << info.filePath;
                } else {
                    allSuccess = false;
                }
                continue;
            }

            // 策略 2: 目标文件存在 -> 对比内容
            if (isContentSame(localFile, zipData)) {
                qDebugEx() << "[跳过] " << info.filePath << "(内容一致)";
                continue;
            }

            // 策略 3: 内容不同 -> 备份旧文件并覆盖
            QString backupPath = destFilePath + ".old";
            
            // 删除可能存在的旧备份
            if (QFile::exists(backupPath)) {
                QFile::remove(backupPath);
            }

            if (localFile.rename(backupPath)) {
                if (writeToFile(destFilePath, zipData)) {
                    qDebugEx() << "[更新] " << info.filePath << "(已备份为 .old)";
                } else {
                    qCriticalEx() << "ZipUtils: 写入新文件失败，尝试恢复备份 ->" << destFilePath;
                    QFile::rename(backupPath, destFilePath); 
                    allSuccess = false;
                }
            } else {
                qCriticalEx() << "ZipUtils: 无法备份文件 (可能被占用) ->" << destFilePath;
                allSuccess = false;
            }
        }

        return allSuccess;
    }

    /**
     * @brief 读取 ZIP 包内部指定文件的内容到内存
     * 适用于读取配置文件、图片数据等，无需解压到磁盘。
     * @param zipPath ZIP 文件的完整路径
     * @param fileNameInZip ZIP 包内部的文件路径 (如 "config/app.json")
     * @return QByteArray 文件数据，读取失败返回空。
     */
    static QByteArray readFileToMemory(const QString &zipPath, const QString &fileNameInZip) {
        QZipReader reader(zipPath);
        if (!isReaderValid(reader, zipPath)) return QByteArray();
        return reader.fileData(fileNameInZip);
    }

    /**
     * @brief 全量解压 (标准解压)
     * 将压缩包内容全部解压到指定目录，如果有同名文件将直接覆盖。
     * @param zipPath ZIP 文件的完整路径
     * @param destDir 解压的目标目录
     * @return bool 成功返回 true
     */
    static bool extractAll(const QString &zipPath, const QString &destDir) {
        QZipReader reader(zipPath);
        if (!isReaderValid(reader, zipPath)) return false;

        QDir dir(destDir);
        if (!dir.exists()) dir.mkpath(".");

        return reader.extractAll(destDir);
    }

    /**
     * @brief 获取文件列表
     * @return QStringList 压缩包内所有文件的路径列表
     */
    static QStringList getFileList(const QString &zipPath) {
        QStringList list;
        QZipReader reader(zipPath);
        if (isReaderValid(reader, zipPath)) {
            foreach (const auto &info, reader.fileInfoList()) {
                list << info.filePath;
            }
        }
        return list;
    }

private:
    // 检查 Reader 状态
    static bool isReaderValid(QZipReader &reader, const QString &zipPath) {
        if (!QFile::exists(zipPath)) {
            qWarning() << "ZipUtils: 文件不存在 ->" << zipPath;
            return false;
        }
        if (reader.status() != QZipReader::NoError) {
            qWarning() << "ZipUtils: ZIP 文件损坏或格式错误 ->" << zipPath;
            return false;
        }
        return true;
    }

    // 写入文件辅助函数
    static bool writeToFile(const QString &filePath, const QByteArray &data) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            return true;
        }
        qCriticalEx() << "ZipUtils: 写入失败 ->" << filePath << file.errorString();
        return false;
    }

    // 比较内容一致性 (先比大小，再比二进制内容)
    static bool isContentSame(QFile &localFile, const QByteArray &zipData) {
        if (localFile.size() != zipData.size()) return false;
        
        if (!localFile.open(QIODevice::ReadOnly)) return false;
        QByteArray localData = localFile.readAll();
        localFile.close();

        return localData == zipData;
    }
};