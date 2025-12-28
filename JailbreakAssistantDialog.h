#include "Tools.h"
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
#include <QFile>
#include <QProcess>

class StepBase : public QFrame {
    Q_OBJECT
public:
    StepBase(QString title, QString desc = nullptr, QWidget *parent = nullptr) 
        : QFrame(parent) 
    {
        setFrameShape(QFrame::NoFrame);
        setStyleSheet(".StepBase { background-color: transparent; }");

        QWidget *leftSide = new QWidget;
        leftSide->setFixedWidth(40);
        QVBoxLayout *leftLayout = new QVBoxLayout(leftSide);
        leftLayout->setContentsMargins(0,0,0,0);
        leftLayout->setSpacing(0);

        statusIcon = new QLabel("?"); 
        statusIcon->setFixedSize(30, 30);
        statusIcon->setAlignment(Qt::AlignCenter);
        statusIcon->setStyleSheet("background-color: #e0e0e0; color: #555; border-radius: 15px; font-weight: bold;");

        line = new QFrame;
        line->setFixedWidth(2);
        line->setStyleSheet("border: none; background-color: #cccccc;"); 
        line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

        leftLayout->addWidget(statusIcon, 0, Qt::AlignTop | Qt::AlignHCenter);
        leftLayout->addWidget(line, 1, Qt::AlignHCenter);

        QWidget *rightSide = new QWidget;
        QVBoxLayout *rightLayout = new QVBoxLayout(rightSide);
        rightLayout->setContentsMargins(10, 5, 10, 20);

        QLabel *lblTitle = new QLabel(title);
        lblTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
        rightLayout->addWidget(lblTitle);
        
        if (desc != nullptr) {
            QLabel *lblDesc = new QLabel(desc);
            lblDesc->setStyleSheet("color: #666; margin-bottom: 5px;");
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
        statusIcon->setStyleSheet("background-color: #007bff; color: white; border-radius: 15px; font-weight: bold;");
        line->setStyleSheet("background-color: #e0e0e0;");
        onActivated(); 
    }

    void finish() {
        statusIcon->setText("✔");
        statusIcon->setStyleSheet("background-color: #28a745; color: white; border-radius: 15px; font-weight: bold;");
        line->setStyleSheet("border: none; background-color: #28a745;");
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
        btnAction->setCursor(Qt::PointingHandCursor);
        btnAction->setFixedHeight(35);

        statusLabel = new QLabel("");
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");

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
        statusLabel->setStyleSheet("color: #007bff;");

        auto file = new QFile(savePath);
        if (!file->open(QIODevice::WriteOnly)) {
            statusLabel->setText("错误：无法写入文件，请检查权限 " + savePath);
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

            if (reply->error() == QNetworkReply::NoError) {
                bar->setValue(100);
                statusLabel->setText("下载成功！");
                statusLabel->setStyleSheet("color: #28a745;");
                finish();
            } else {
                btnAction->setEnabled(true);
                btnAction->setText("重试下载");
                statusLabel->setText("下载失败: " + reply->errorString());
                statusLabel->setStyleSheet("color: red;");
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
        btnConfirm->setCursor(Qt::PointingHandCursor);
        btnConfirm->setStyleSheet("QPushButton { background-color: #007bff; color: white; border-radius: 4px; }"
                                  "QPushButton:hover { background-color: #0069d9; }"
                                  "QPushButton:disabled { background-color: #ccc; }");

        lblResult = new QLabel("");
        lblResult->setStyleSheet("color: #28a745; font-weight: bold; margin-top: 5px;");

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

        btnStart = new QPushButton("开始注入 (运行 TrollRestore)");
        btnStart->setFixedHeight(35);
        btnStart->setCursor(Qt::PointingHandCursor);
        btnStart->setStyleSheet("QPushButton { background-color: #d63384; color: white; border-radius: 4px; font-weight: bold; }"
                                "QPushButton:hover { background-color: #c21b6c; }"
                                "QPushButton:disabled { background-color: #ccc; }");
        
        btnSkip = new QPushButton("已注入，直接跳过");
        btnSkip->setFixedHeight(35);
        btnSkip->setCursor(Qt::PointingHandCursor);
        btnSkip->setStyleSheet("QPushButton { background-color: #6c757d; color: white; border-radius: 4px; }"
                               "QPushButton:hover { background-color: #5a6268; }"
                               "QPushButton:disabled { background-color: #ccc; }");

        btnLayout->addWidget(btnStart, 2);
        btnLayout->addWidget(btnSkip, 1);

        consoleOutput = new QTextEdit;
        consoleOutput->setReadOnly(true);
        consoleOutput->setPlaceholderText("等待执行...日志将在此显示");
        consoleOutput->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #00ff00; font-family: Consolas, Monospace; border: 1px solid #555; border-radius: 4px; }");
        consoleOutput->setMinimumHeight(150);

        contentLayout->addLayout(btnLayout);
        contentLayout->addWidget(consoleOutput);

        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);

        connect(btnStart, &QPushButton::clicked, this, &StepExecute::startProcess);
        connect(btnSkip, &QPushButton::clicked, this, &StepExecute::skipProcess);
        connect(process, &QProcess::readyReadStandardOutput, this, &StepExecute::readOutput);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &StepExecute::onProcessFinished);
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
        consoleOutput->append("<span style='color:orange;'>用户手动跳过了注入步骤。</span>");
        finish();
    }

    void startProcess() {
        btnStart->setDisabled(true);
        btnSkip->setDisabled(true);
        consoleOutput->clear();
        consoleOutput->append(QString("正在启动注入程序，目标应用: <span style='color:yellow;'>%1</span> ...<br>").arg(targetAppName));

        QString program = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/TrollRestore";

        #ifdef Q_OS_MAC
        QFile localFile(program);
        // 获取当前权限并加上 所有者的可执行权限 + 用户组可执行 + 其他人可执行
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
        consoleOutput->append(QString("<br><span style='color:red;'>启动进程失败: %1</span>").arg(process->errorString()));
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
        btnFinish->setCursor(Qt::PointingHandCursor);
        btnFinish->setMinimumWidth(150);
        btnFinish->setStyleSheet("QPushButton { background-color: #28a745; color: white; border-radius: 4px; padding: 8px; font-weight: bold; }"
                                 "QPushButton:hover { background-color: #218838; }");

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

        layout->addWidget(nameLabel);
        layout->addWidget(qrLabel);

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
        setModal(true);
        setWindowTitle("越狱助手【只支持15.0 - 16.5.1系统】");
        resize(600, 800);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        QScrollArea *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        
        QWidget *container = new QWidget;
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(20, 20, 20, 20);
        containerLayout->setSpacing(0);

        QString url = "https://gitee.com/coding202208/pandora/releases/download/v1/TrollRestore";

#ifdef Q_OS_WIN
        url.append(".exe");
#endif

        auto step1 = new StepDownload("下载TrollRestore", "帮你给手机装上“巨魔商店”，解锁随意安装App且永不过期的强大功能。", url);
        auto step2 = new StepSelectApp("选择一个将被替换的系统应用", "请选择一个已安装在手机上的系统应用进行注入：");
        auto step3 = new StepExecute("开始注入", "运行程序并将 Helper 注入目标应用。（请确保仅有一台手机连接并已信任此电脑）");
        auto step4 = new StepScan("使用相机app扫码下载Dopamine", "稳定且现代化的越狱工具", "https://gitee.com/coding202208/pandora/releases/download/v1/Dopamine.tipa", "https://gitee.com/coding202208/pandora/releases/download/v1/Dopamine2.tipa");
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
