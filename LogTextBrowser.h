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
#include <QLineEdit>
#include <QEvent>
#include <QLayout>
#include <magic_enum/magic_enum.hpp>

class LogTextBrowser : public QTextBrowser
{
    Q_OBJECT
public:
    explicit LogTextBrowser(QWidget *parent = nullptr) : QTextBrowser(parent)
    {
        instance = this;

        searchEdit = new QLineEdit(this);
        searchEdit->setPlaceholderText("🔍 搜索日志...");
        searchEdit->setClearButtonEnabled(true);
        searchEdit->setContentsMargins(5, 5, 5, 0);
        searchEdit->setStyleSheet(R"(
            QLineEdit {
                padding: 8px 15px;
                border: 1px solid palette(mid);
                border-radius: 18px;
                background-color: palette(window);
                font-size: 14px;
                color: palette(text);
                selection-background-color: palette(highlight);
                selection-color: palette(highlighted-text);
            }
            QLineEdit:focus {
                border: 1px solid palette(highlight);
                background-color: palette(base);
            }
        )");
        // 设置视口顶部边距，防止文字被搜索框遮挡
        setViewportMargins(0, 38, 0, 0);

        connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
            clear();
            for (const QString &text : allLogs) {
                appendIfMatch(text);
            }
        });

        const auto& filePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/app_log.txt";

        logFile.setFileName(filePath);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            append("<span style='color:red;'>无法打开日志文件！</span>");
        }

        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
            QMetaObject::invokeMethod(instance, [=]() {
                QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
                QString formattedMessage = QString("[%1] %2 | %3").arg(time).arg(magic_enum::enum_name(type)).arg(message);

                QString color = (type == QtCriticalMsg || type == QtFatalMsg) ? "red" : (type == QtWarningMsg) ? "orange" : "black";

                QString htmlMessage = QString("<span style='color:%1;'>%2</span>")
                    .arg(color)
                    .arg(formattedMessage);

                instance->allLogs.append(htmlMessage);
                instance->appendIfMatch(htmlMessage);

                QTextStream out(&instance->logFile);
                out << formattedMessage << "\n";
                out.flush();
            });
        });
    }

    static LogTextBrowser* getInstance() {return instance;}

    void toggleVisibility()
    {
        setVisible(!isVisible());
    }

protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::ParentChange) {
            if (QWidget *p = parentWidget()) {
                QLayout *layout = p->layout();
                if (!layout) {
                    QVBoxLayout *vbox = new QVBoxLayout(p);
                    vbox->setContentsMargins(0, 0, 0, 0);
                    vbox->setSpacing(0);
                    layout = vbox;
                }
                
                layout->addWidget(this); 
            }
        }
        return QTextBrowser::event(e);
    }

    void resizeEvent(QResizeEvent *event) override {
        QTextBrowser::resizeEvent(event);
        searchEdit->setGeometry(0, 0, width(), 40);
    }

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
            for (const QString &text : allLogs) {
                appendIfMatch(text);
            }
        });
        filterAction->setCheckable(true);
        filterAction->setChecked(showOnlyErrors);

        menu->exec(event->globalPos());
        menu->deleteLater();
    }

    inline static LogTextBrowser* instance;
    QFile logFile;
    bool showOnlyErrors = false;
    QStringList allLogs;
    QLineEdit *searchEdit;

    void appendIfMatch(const QString& text)
    {
        bool matchError = !showOnlyErrors || (text.contains("color:red") || text.contains("color:orange"));
        bool matchSearch = searchEdit->text().isEmpty() || text.contains(searchEdit->text(), Qt::CaseInsensitive);

        if (matchError && matchSearch)
            append(text);
    }
};
