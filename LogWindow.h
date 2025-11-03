#pragma once

#include <QTextBrowser>
#include <QMetaObject>
#include <QMessageLogContext>
#include <QTextBlock>
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
                logWindow->appendWithLimit(htmlMessage);

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
    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu *menu = createStandardContextMenu();
        menu->addSeparator();

        connect(menu->addAction("清空日志"), &QAction::triggered, this, [this]() {
            clear();
            allLogs.clear();
            logFile.resize(0);
        });

        QAction *filterAction = menu->addAction("只显示错误日志");
        filterAction->setCheckable(true);
        filterAction->setChecked(showOnlyErrors);

        connect(filterAction, &QAction::toggled, this, [this](bool checked) {
            showOnlyErrors = checked;
            
            clear();
            for (const QString &msg : allLogs) {
                appendWithLimit(msg);
            }
        });

        menu->exec(event->globalPos());
        delete menu;
    }

private:
    inline static LogWindow* logWindow;
    QFile logFile;
    bool showOnlyErrors = false;
    QStringList allLogs;

    void appendWithLimit(const QString& message)
    {
        if (document()->blockCount() > 5000) {
            QTextBlock firstBlock = document()->firstBlock();
            QTextCursor cursor(firstBlock);
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, 1);
            cursor.removeSelectedText();
        }

        if (!showOnlyErrors || message.contains("color:red"))
            append(message);
    }
};
