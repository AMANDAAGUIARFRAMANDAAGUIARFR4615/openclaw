#pragma once

#include <QTextBrowser>
#include <QMetaObject>
#include <QMessageLogContext>
#include <QTextBlock>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QMenu>

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
                QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
                QString formattedMessage = QString("[%1] %2").arg(time, message);

                if (type == QtCriticalMsg || type == QtFatalMsg || type == QtWarningMsg) {
                    logWindow->appendWithLimit(QString("<span style='color:red;'>%1</span>").arg(formattedMessage));
                } else {
                    logWindow->appendWithLimit(QString("<span style='color:black;'>%1</span>").arg(formattedMessage));
                }

                if (logWindow->logFile.isOpen()) {
                    QTextStream out(&logWindow->logFile);
                    out << formattedMessage << "\n";
                    out.flush();
                }
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
            logFile.resize(0);
        });

        menu->exec(event->globalPos());
        delete menu;
    }

private:
    inline static LogWindow* logWindow;
    QFile logFile;

    void appendWithLimit(const QString& message)
    {
        if (document()->blockCount() > 5000) {
            QTextBlock firstBlock = document()->firstBlock();
            QTextCursor cursor(firstBlock);
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, 1);
            cursor.removeSelectedText();
        }

        append(message);
    }
};
