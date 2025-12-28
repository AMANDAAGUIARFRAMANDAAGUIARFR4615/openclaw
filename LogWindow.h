#pragma once

#include <QTextBrowser>
#include <QMetaObject>
#include <QMessageLogContext>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QMenu>
#include <QFileInfo>
#include <QDir>
#include <QClipboard>
#include <QPainter>
#include <QPixmap>
#include <magic_enum/magic_enum.hpp>

class LogWindow : public QTextBrowser
{
    Q_OBJECT
public:
    explicit LogWindow(QWidget *parent = nullptr) : QTextBrowser(parent)
    {
        instance = this;

        const auto& filePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/app_log.txt";

        logFile.setFileName(filePath);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            append("<span style='color:red;'>无法打开日志文件！</span>");
        }

        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
#ifndef QT_DEBUG
            if (type == QtDebugMsg)
                return;
#endif

            QMetaObject::invokeMethod(instance, [=]() {
                QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
                QString formattedMessage = QString("[%1] %2 | %3").arg(time).arg(magic_enum::enum_name(type)).arg(message);

                QString color = (type == QtCriticalMsg || type == QtFatalMsg) ? "red" : (type == QtWarningMsg) ? "orange" : "black";

                QString htmlMessage = QString("<span style='color:%1;'>%2</span>")
                    .arg(color)
                    .arg(formattedMessage);

                instance->allLogs.append(htmlMessage);
                instance->appendLog(htmlMessage);

                QTextStream out(&instance->logFile);
                out << formattedMessage << "\n";
                out.flush();
            });
        });
    }

    static LogWindow* getInstance() {return instance;}

    void toggleVisibility()
    {
        setVisible(!isVisible());
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QTextBrowser::keyPressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu *menu = createStandardContextMenu();
        menu->addSeparator();

        menu->addAction("生成长图到剪切板", [this]() {
            QPixmap pixmap(document()->size().toSize());
            pixmap.fill(Qt::white);

            QPainter painter(&pixmap);
            document()->drawContents(&painter);
            
            qApp->clipboard()->setPixmap(pixmap);
        });

        menu->addAction("清空日志", [this]() {
            clear();
            allLogs.clear();
            logFile.resize(0);
        });

        QAction *filterAction = menu->addAction("只显示错误日志", [this](bool checked) {
            showOnlyErrors = checked;
            
            clear();
            for (const QString &msg : allLogs) {
                appendLog(msg);
            }
        });
        filterAction->setCheckable(true);
        filterAction->setChecked(showOnlyErrors);

        menu->exec(event->globalPos());
        menu->deleteLater();
    }

    inline static LogWindow* instance;
    QFile logFile;
    bool showOnlyErrors = false;
    QStringList allLogs;

    void appendLog(const QString& message)
    {
        if (!showOnlyErrors || message.contains("color:red"))
            append(message);
    }
};