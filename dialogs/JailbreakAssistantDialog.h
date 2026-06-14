#include <QtGlobal>
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)

#pragma once
#include "Tools.h"
#include "../Theme.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QTimer>
#include <QFrame>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDialog>
#include <QFile>
#include <QProcess>
#include <QStyleHints>
#include <QStyle>

class StepBase : public QFrame {
    Q_OBJECT
public:
    StepBase(QString title, QString desc = nullptr, QWidget *parent = nullptr) 
        : QFrame(parent) 
    {
        setFrameShape(QFrame::NoFrame);
        setStyleSheet(".StepBase { background-color: transparent; }");

        QString iconBg = Theme::surfaceAlt();
        QString iconFg = Theme::textSecondary();
        QString lineBg = Theme::border();

        QWidget *leftSide = new QWidget;
        leftSide->setFixedWidth(40);
        QVBoxLayout *leftLayout = new QVBoxLayout(leftSide);
        leftLayout->setContentsMargins(0,0,0,0);
        leftLayout->setSpacing(0);

        statusIcon = new QLabel("?"); 
        statusIcon->setFixedSize(30, 30);
        statusIcon->setAlignment(Qt::AlignCenter);
        statusIcon->setStyleSheet(QString("background-color: %1; color: %2; border-radius: 15px; font-weight: bold;")
                                  .arg(iconBg, iconFg));

        line = new QFrame;
        line->setFixedWidth(2);
        line->setStyleSheet(QString("border: none; background-color: %1;").arg(lineBg)); 
        line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

        leftLayout->addWidget(statusIcon, 0, Qt::AlignTop | Qt::AlignHCenter);
        leftLayout->addWidget(line, 1, Qt::AlignHCenter);

        QWidget *rightSide = new QWidget;
        QVBoxLayout *rightLayout = new QVBoxLayout(rightSide);
        rightLayout->setContentsMargins(10, 5, 10, 20);

        QLabel *lblTitle = new QLabel(title);
        QFont titleFont = lblTitle->font();
        titleFont.setPixelSize(16);
        titleFont.setBold(true);
        lblTitle->setFont(titleFont);
        rightLayout->addWidget(lblTitle);
        
        if (desc != nullptr) {
            QLabel *lblDesc = new QLabel(desc);
            QColor descColor = palette().color(QPalette::Text);
            descColor.setAlpha(180);
            lblDesc->setStyleSheet(QString("color: %1; margin-bottom: 5px;").arg(descColor.name(QColor::HexArgb)));
            rightLayout->addWidget(lblDesc);
        }
        
        contentArea = new QWidget;
        contentLayout = new QVBoxLayout(contentArea);
        contentLayout->setContentsMargins(0, 10, 0, 0);
        rightLayout->addWidget(contentArea);

        QHBoxLayout *mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->addWidget(leftSide);
        mainLayout->addWidget(rightSide, 1);

        contentArea->setVisible(false);
        this->setDisabled(true); 
    }

    void setIndex(int index) {
        if (statusIcon->text() != "✔") {
            statusIcon->setText(QString::number(index));
        }
    }

    void activate() {
        this->setEnabled(true);
        contentArea->setVisible(true); 

        QColor highlight = palette().color(QPalette::Highlight);
        QColor highlightText = palette().color(QPalette::HighlightedText);

        statusIcon->setStyleSheet(QString("background-color: %1; color: %2; border-radius: 15px; font-weight: bold;")
                                  .arg(highlight.name(), highlightText.name()));

        line->setStyleSheet(QString("background-color: %1;").arg(Theme::palette().borderStrong));
        onActivated(); 
    }

    void finish() {
        const QString green = Theme::success();

        statusIcon->setText("✔");
        statusIcon->setStyleSheet(QString("background-color: %1; color: white; border-radius: 15px; font-weight: bold;").arg(green));
        line->setStyleSheet(QString("border: none; background-color: %1;").arg(green));
        contentArea->setDisabled(true);
        emit finished(); 
    }

    void hideLine() {
        line->setVisible(false);
    }

protected:
    virtual void onActivated() = 0;
    QVBoxLayout *contentLayout;

signals:
    void finished();

private:
    QLabel *statusIcon;
    QFrame *line;
    QWidget *contentArea;
};

class StepDownload : public StepBase {
    Q_OBJECT
public:
    StepDownload(QString title, QString desc, QString url, QWidget *parent = nullptr) 
        : StepBase(title, desc, parent), m_url(url)
    {
        QString fileName = url.section('/', -1);
        savePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/" + fileName;

        btnAction = new QPushButton("");
        btnAction->setFixedHeight(35);

        statusLabel = new QLabel("");
        QColor txtColor = palette().color(QPalette::PlaceholderText);
        statusLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(txtColor.name()));

        bar = new QProgressBar;
        bar->setTextVisible(true);
        bar->setValue(0);
        bar->setVisible(false);

        contentLayout->addWidget(btnAction);
        contentLayout->addWidget(statusLabel);
        contentLayout->addWidget(bar);
        
        manager = new QNetworkAccessManager(this);

        connect(btnAction, &QPushButton::clicked, this, &StepDownload::onActionClicked);
    }

protected:
    void onActivated() override {
        if (QFile::exists(savePath)) {
            btnAction->setText("本地已存在，跳过下载");
            finish();
        } else {
            btnAction->setText("开始下载");
        }
    }

private slots:
    void onActionClicked() {
        btnAction->setEnabled(false);
        bar->setVisible(true);
        bar->setValue(0);
        statusLabel->setText("正在连接服务器...");
        
        QColor highlight = palette().color(QPalette::Highlight);
        statusLabel->setStyleSheet(QString("color: %1;").arg(highlight.name()));

        auto file = new QFile(savePath);
        if (!file->open(QIODevice::WriteOnly)) {
            QColor errorColor = (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark) ? QColor("#e74c3c") : Qt::red;
            statusLabel->setText("错误：无法写入文件，请检查权限 " + savePath);
            statusLabel->setStyleSheet(QString("color: %1;").arg(errorColor.name()));
            return;
        }

        QNetworkRequest request(m_url);
        auto reply = manager->get(request);
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal){
            if (bytesTotal > 0) {
                int percent = (bytesReceived * 100) / bytesTotal;
                bar->setValue(percent);
                statusLabel->setText(QString("下载中: %1 KB / %2 KB")
                                        .arg(bytesReceived / 1024)
                                        .arg(bytesTotal / 1024));
            }
        });
        connect(reply, &QNetworkReply::readyRead, this, [=]() {
            if (file->isOpen())
                file->write(reply->readAll());
        });
        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (file) {
                file->close();
                file->deleteLater();
            }
            
            bool isDark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
            QString green = isDark ? "#2ecc71" : "#28a745";
            QString red = isDark ? "#e74c3c" : "red";

            if (reply->error() == QNetworkReply::NoError) {
                bar->setValue(100);
                statusLabel->setText("下载成功！");
                statusLabel->setStyleSheet(QString("color: %1;").arg(green));
                finish();
            } else {
                btnAction->setEnabled(true);
                btnAction->setText("重试下载");
                statusLabel->setText("下载失败: " + reply->errorString());
                statusLabel->setStyleSheet(QString("color: %1;").arg(red));
                QFile::remove(savePath);
            }
            reply->deleteLater();
        });
    }

private:
    QPushButton *btnAction;
    QProgressBar *bar;
    QLabel *statusLabel;
    QNetworkAccessManager *manager;
    QString savePath;
    QString m_url;
};

class StepSelectApp : public StepBase {
    Q_OBJECT
public:
    StepSelectApp(QString title, QString desc, QWidget *parent = nullptr)
        : StepBase(title, desc, parent)
    {
        appCombo = new QComboBox;
        appCombo->setFixedHeight(35);
        appCombo->setStyleSheet("QComboBox { padding: 5px; font-size: 14px; }");

        appCombo->addItem("测距仪 (Measure)", "Measure");
        appCombo->addItem("Apple TV (AppleTV)", "AppleTV");
        appCombo->addItem("通讯录 (Contacts)", "Contacts");
        appCombo->addItem("iTunes Store", "iTunes Store");
        appCombo->addItem("天气 (Weather)", "Weather");
        appCombo->addItem("语音备忘录 (VoiceMemos)", "VoiceMemos");
        appCombo->addItem("FaceTime 通话 (FaceTime)", "FaceTime");
        appCombo->addItem("地图 (Maps)", "Maps");
        appCombo->addItem("家庭 (Home)", "Home");
        appCombo->addItem("邮件 (Mail)", "Mail");
        appCombo->addItem("提醒事项 (Reminders)", "Reminders");
        appCombo->addItem("文件 (Files)", "Files");
        appCombo->addItem("快捷指令 (Shortcuts)", "Shortcuts");
        appCombo->addItem("Watch", "Watch");
        appCombo->addItem("提示 (Tips)", "Tips");
        appCombo->addItem("计算器 (Calculator)", "Calculator");
        appCombo->addItem("备忘录 (Notes)", "Notes");
        appCombo->addItem("翻译 (Translate)", "Translate");
        appCombo->addItem("放大器 (Magnifier)", "Magnifier");
        appCombo->addItem("日历 (Calendar)", "Calendar");
        appCombo->addItem("音乐 (Music)", "Music");
        appCombo->addItem("图书 (Books)", "Books");
        appCombo->addItem("指南针 (Compass)", "Compass");
        appCombo->addItem("股市 (Stocks)", "Stocks");
        appCombo->addItem("播客 (Podcasts)", "Podcasts");

        appCombo->setCurrentIndex(14);

        btnConfirm = new QPushButton("确认选择");
        btnConfirm->setFixedHeight(35);
        
        btnConfirm->setStyleSheet(Theme::fill(QStringLiteral(
            "QPushButton { background-color: @{primary}; color: white; border-radius: 6px; }"
            "QPushButton:hover { background-color: @{primaryHover}; }"
            "QPushButton:disabled { background-color: @{primaryDisabled}; }")));

        lblResult = new QLabel("");

        lblResult->setStyleSheet(QString("color: %1; font-weight: bold; margin-top: 5px;").arg(Theme::success()));

        contentLayout->addWidget(appCombo);
        contentLayout->addWidget(btnConfirm);
        contentLayout->addWidget(lblResult);

        connect(btnConfirm, &QPushButton::clicked, this, &StepSelectApp::onConfirmClicked);
    }

    QString getSelectedAppValue() const {
        return selectedValue;
    }

protected:
    void onActivated() override {
        appCombo->setFocus();
    }

private slots:
    void onConfirmClicked() {
        QString displayName = appCombo->currentText();
        selectedValue = appCombo->currentData().toString();
        appCombo->setDisabled(true);
        btnConfirm->setVisible(false);
        lblResult->setText("已选定注入应用: " + displayName);
        finish();
    }

private:
    QComboBox *appCombo;
    QPushButton *btnConfirm;
    QLabel *lblResult;
    QString selectedValue;
};

class StepExecute : public StepBase {
    Q_OBJECT
public:
    StepExecute(QString title, QString desc, QWidget *parent = nullptr) 
        : StepBase(title, desc, parent) 
    {
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(10);

        QString gray = Theme::isDark() ? "#5A6270" : "#6c757d";
        QString grayHover = Theme::isDark() ? "#6B7280" : "#5a6268";

        btnStart = new QPushButton("开始注入 (运行 TrollRestore)");
        btnStart->setFixedHeight(35);
        btnStart->setStyleSheet(Theme::fill(QStringLiteral(
            "QPushButton { background-color: @{primary}; color: white; border-radius: 6px; font-weight: bold; }"
            "QPushButton:hover { background-color: @{primaryHover}; }"
            "QPushButton:disabled { background-color: @{primaryDisabled}; }")));

        btnSkip = new QPushButton("已注入，直接跳过");
        btnSkip->setFixedHeight(35);
        btnSkip->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 6px; }"
                                       "QPushButton:hover { background-color: %2; }").arg(gray, grayHover));

        btnLayout->addWidget(btnStart, 2);
        btnLayout->addWidget(btnSkip, 1);

        consoleOutput = new QTextEdit;
        consoleOutput->setReadOnly(true);
        consoleOutput->setPlaceholderText("等待执行...日志将在此显示");
        consoleOutput->setMinimumHeight(150);
        QFont monoFont("Consolas");
        if(monoFont.exactMatch() == false) monoFont.setFamily("Monospace");
        consoleOutput->setFont(monoFont);

        contentLayout->addLayout(btnLayout);
        contentLayout->addWidget(consoleOutput);

        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);

        connect(btnStart, &QPushButton::clicked, this, &StepExecute::startProcess);
        connect(btnSkip, &QPushButton::clicked, this, &StepExecute::skipProcess);
        connect(process, &QProcess::readyReadStandardOutput, this, &StepExecute::readOutput);
        connect(process, &QProcess::finished, this, &StepExecute::onProcessFinished);
        connect(process, &QProcess::errorOccurred, this, &StepExecute::onProcessError);
    }

    void setTargetAppName(const QString &appName) {
        this->targetAppName = appName;
        btnStart->setText(QString("开始注入 -> %1").arg(appName));
    }

protected:
    void onActivated() override {
        btnStart->setFocus();
    }

private slots:
    void skipProcess() {
        btnStart->setDisabled(true);
        btnSkip->setDisabled(true);
        QColor warnColor = (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark) ? QColor("#f39c12") : QColor("orange");
        consoleOutput->append(QString("<span style='color:%1;'>用户手动跳过了注入步骤。</span>").arg(warnColor.name()));
        finish();
    }

    void startProcess() {
        btnStart->setDisabled(true);
        btnSkip->setDisabled(true);
        consoleOutput->clear();
        
        QColor targetColor = (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark) ? QColor("#f1c40f") : QColor("#d35400");
        
        consoleOutput->append(QString("正在启动注入程序，目标应用: <span style='color:%1;'>%2</span> ...<br>").arg(targetColor.name(), targetAppName));

        QString program = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/TrollRestore";

        #ifdef Q_OS_MACOS
        QFile localFile(program);
        if (!localFile.setPermissions(localFile.permissions() | 
                                      QFileDevice::ExeOwner | 
                                      QFileDevice::ExeGroup | 
                                      QFileDevice::ExeOther)) {
            qCriticalEx() << "设置可执行权限失败";
        }
        #endif
        
        QStringList arguments;
        if (!targetAppName.isEmpty()) {
            arguments << "--app" << targetAppName;
        }

        process->start(program, arguments);
    }

    void readOutput() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromLocal8Bit(data);
        consoleOutput->moveCursor(QTextCursor::End);
        consoleOutput->insertPlainText(text);
        consoleOutput->moveCursor(QTextCursor::End);
    }

    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            finish();
            return;
        }

        btnStart->setEnabled(true);
        btnSkip->setEnabled(true);
        btnStart->setText("重试注入");
    }

    void onProcessError(QProcess::ProcessError error) {
        QColor errColor = (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark) ? QColor("#e74c3c") : Qt::red;
        consoleOutput->append(QString("<br><span style='color:%1;'>启动进程失败: %2</span>").arg(errColor.name(), process->errorString()));
        btnStart->setEnabled(true);
        btnSkip->setEnabled(true);
    }

private:
    QPushButton *btnStart;
    QPushButton *btnSkip;
    QTextEdit *consoleOutput;
    QProcess *process;
    QString targetAppName;
};

class StepScan : public StepBase {
    Q_OBJECT
public:
    StepScan(QString title, QString desc, QString urlNoRoot, QString urlSilentRoot, QWidget *parent = nullptr) 
        : StepBase(title, desc, parent) 
    {
        QHBoxLayout *qrContainerLayout = new QHBoxLayout;
        qrContainerLayout->setSpacing(30);
        qrContainerLayout->setAlignment(Qt::AlignCenter);

        qrContainerLayout->addLayout(createQrBlock("无根版本", urlNoRoot));
        qrContainerLayout->addLayout(createQrBlock("隐根版本", urlSilentRoot));

        QHBoxLayout *btnLayout = new QHBoxLayout;
        btnLayout->addStretch();

        QPushButton *btnFinish = new QPushButton("我已完成下载");
        btnFinish->setMinimumWidth(150);
        
        const QString green = Theme::success();

        btnFinish->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px; font-weight: bold; }"
                                         "QPushButton:hover { background-color: %1; }").arg(green));

        btnLayout->addWidget(btnFinish);
        btnLayout->addStretch();

        contentLayout->addLayout(qrContainerLayout);
        contentLayout->addSpacing(20);
        contentLayout->addLayout(btnLayout);

        connect(btnFinish, &QPushButton::clicked, this, [this](){ finish(); });
    }

protected:
    void onActivated() override {}

private:
    QVBoxLayout* createQrBlock(QString name, QString url) {
        QVBoxLayout *layout = new QVBoxLayout;
        layout->setSpacing(10);
        layout->setAlignment(Qt::AlignCenter);

        QLabel *nameLabel = new QLabel(name);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("font-weight: bold;"); 

        int qrSize = 200;
        QImage img = Tools::generateQrImage(url);
        QPixmap pixmap = QPixmap::fromImage(img).scaled(
            qrSize, qrSize, 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        );

        QLabel *qrLabel = new QLabel;
        qrLabel->setPixmap(pixmap);
        qrLabel->setAlignment(Qt::AlignCenter);

        QLabel *urlLabel = new QLabel(url);
        urlLabel->setAlignment(Qt::AlignCenter);
        urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse); // 允许鼠标复制
        urlLabel->setStyleSheet("color: gray; font-size: 11px;");

        layout->addWidget(nameLabel);
        layout->addWidget(qrLabel);
        layout->addWidget(urlLabel);

        return layout;
    }
};

class StepFinal : public StepBase {
public:
    StepFinal(QString title, QString desc, QWidget *parent = nullptr) : StepBase(title, desc, parent) {}
protected:
    void onActivated() override { finish(); }
};

class JailbreakAssistantDialog : public QDialog {
public:
    JailbreakAssistantDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("越狱助手【完美支持15.2 - 16.6.1系统】");
        resize(750, 800);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        QScrollArea *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        
        QWidget *container = new QWidget;
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(20, 20, 20, 20);
        containerLayout->setSpacing(0);

        QString url = "https://remotepro.cn/TrollRestore";

#ifdef Q_OS_WIN
        url.append(".exe");
#endif

        auto step1 = new StepDownload("下载TrollRestore", "给手机装上“巨魔商店”，用于安装App且永不掉签。\n系統要求：iOS 15.2 - 16.7 RC (20H18) and 17.0", url);
        auto step2 = new StepSelectApp("选择一个将被替换的系统应用", "请选择一个已安装在手机上的系统应用进行注入：");
        auto step3 = new StepExecute("开始注入", "运行程序并将安装器注入目标应用。（请确保仅有一台手机连接并已信任此电脑）");
        auto step4 = new StepScan("使用相机app扫码下载Dopamine", "无根是不修改系统分区以实现越狱，而隐根是在无根的基础上彻底隐藏越狱文件，让App检测不到越狱状态。\n系統要求：iOS 15.0 - 16.5.1 (arm64e) and iOS 15.0 - 16.6.1 (arm64)", "https://remotepro.cn/rootless", "https://remotepro.cn/roothide");
        auto step5 = new StepFinal("最后一步", "在手机上打开巨魔商店（TrollStore）\n点击右上角加号\n选择Install IPA File就可以安装Dopamine\n安装后打开Dopamine点击越狱等待完成即可。");

        QList<StepBase*> steps;
        steps << step1 << step2 << step3 << step4 << step5;

        for (int i = 0; i < steps.count(); ++i) {
            StepBase* step = steps[i];
            step->setIndex(i + 1);

            if (i == steps.count() - 1)
                step->hideLine();
            else
                connect(step, &StepBase::finished, steps[i + 1], &StepBase::activate);

            containerLayout->addWidget(step);
        }

        connect(step2, &StepBase::finished, step3, [=]() {
            step3->setTargetAppName(step2->getSelectedAppValue());
        });

        containerLayout->addStretch();
        scroll->setWidget(container);
        mainLayout->addWidget(scroll);

        QTimer::singleShot(100, step1, &StepBase::activate);
    }
};

#endif
