#pragma once

#include <QTextBrowser>
#include <QMetaObject>
#include <QMessageLogContext>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QMenu>
#include <magic_enum/magic_enum.hpp>

class LogWindow : public QTextBrowser
{
    Q_OBJECT
public:
    explicit LogWindow(QWidget *parent = nullptr) : QTextBrowser(parent)
    {
        logWindow = this;
        setVisible(false);

        logFile.setFileName(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/app_log.txt");
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            append("<span style='color:red;'>无法打开日志文件！</span>");
        }

        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
            QMetaObject::invokeMethod(logWindow, [=]() {
                QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
                QString formattedMessage = QString("[%1] %2 | %3").arg(time).arg(magic_enum::enum_name(type)).arg(message);

                QString color = (type == QtCriticalMsg || type == QtFatalMsg) ? "red" : (type == QtWarningMsg) ? "orange" : "black";

                QString htmlMessage = QString("<span style='color:%1;'>%2</span>")
                    .arg(color)
                    .arg(formattedMessage);

                logWindow->allLogs.append(htmlMessage);
                logWindow->appendLog(htmlMessage);

                QTextStream out(&logWindow->logFile);
                out << formattedMessage << "\n";
                out.flush();
            });
        });
    }

    void toggleVisibility()
    {
        setVisible(!isVisible());
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu *menu = createStandardContextMenu();
        menu->addSeparator();

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
        delete menu;
    }

    inline static LogWindow* logWindow;
    QFile logFile;
    bool showOnlyErrors = false;
    QStringList allLogs;

    void appendLog(const QString& message)
    {
        if (!showOnlyErrors || message.contains("color:red"))
            append(message);
    }
};
