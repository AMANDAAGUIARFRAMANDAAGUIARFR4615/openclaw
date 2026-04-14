#pragma once

#include "global.h"
#include "Safe.h"
#include "NetworkUtils.h"
#include "Tools.h"
#include "DeviceInfo.h"
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QCheckBox>
#include <QByteArray>
#include <QRandomGenerator>
#include <QJsonArray>
#include <QApplication>
#include <QStyleHints>
#include <QRegularExpressionValidator>
#include <QTcpServer>
#include <QKeyEvent>

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        
        titleLabel = new QLabel("用户登录");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 22pt; padding: 10px 0px; min-height: 40px; color: palette(text);");

        auto phoneValidator = new QRegularExpressionValidator(QRegularExpression("^1[3-9]\\d{9}$"), this);
        // ^(?=.*[A-Za-z])   : 必须包含至少一个字母
        // (?=.*\\d)         : 必须包含至少一个数字
        // [\\S]{6,32}$      : 允许任何非空白字符(含特殊符号)，长度6-32位
        auto passwordValidator = new QRegularExpressionValidator(QRegularExpression("^(?=.*[A-Za-z])(?=.*\\d)[\\S]{6,32}$"), this);

        phoneLineEdit = new QLineEdit;
        phoneLineEdit->setPlaceholderText("手机号");
        phoneLineEdit->setValidator(phoneValidator);
        phoneLineEdit->setMaxLength(11);

        udidLineEdit = new QLineEdit;
        udidLineEdit->setPlaceholderText("请输入账号使用过的手机设备UDID");
        udidLineEdit->setVisible(false);

        passwordLineEdit = new QLineEdit;
        passwordLineEdit->setPlaceholderText("密码 (6-32位，字母+数字+符号)");
        passwordLineEdit->setValidator(passwordValidator);
        passwordLineEdit->setEchoMode(QLineEdit::Password);

        confirmLineEdit = new QLineEdit;
        confirmLineEdit->setPlaceholderText("确认密码");
        confirmLineEdit->setValidator(passwordValidator);
        confirmLineEdit->setEchoMode(QLineEdit::Password);
        confirmLineEdit->setVisible(false);

        rememberCheckBox = new QCheckBox("记住账号和密码");

        findPwdBtn = new QPushButton("忘记密码?");
        findPwdBtn->setStyleSheet("border: none; color: palette(link); text-decoration: underline; background: transparent; padding: 0px;");

        actionButton = new QPushButton("登录");
        actionButton->setObjectName("mainBtn");
        actionButton->setEnabled(false);
        switchButton = new QPushButton("注册新账号");
        switchButton->setObjectName("subBtn");
        
        statusLabel = new QLabel("正在连接服务器...");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setOpenExternalLinks(true);

        auto buttonLayout = new QHBoxLayout;
        buttonLayout->addWidget(actionButton);
        buttonLayout->addWidget(switchButton);

        // 将记住密码和找回密码放在同一行
        auto optionsLayout = new QHBoxLayout;
        optionsLayout->addWidget(rememberCheckBox);
        optionsLayout->addStretch();
        optionsLayout->addWidget(findPwdBtn);

        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 30, 40, 30);
        mainLayout->setSpacing(16);
        mainLayout->addStretch();
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(phoneLineEdit);
        mainLayout->addWidget(udidLineEdit);
        mainLayout->addWidget(passwordLineEdit);
        mainLayout->addWidget(confirmLineEdit);
        mainLayout->addLayout(optionsLayout);
        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(statusLabel);
        mainLayout->addStretch();

        setMinimumWidth(450);
        updateStyle();

        loadCredentials();

        connect(actionButton, &QPushButton::clicked, this, &LoginWidget::onAction);
        connect(switchButton, &QPushButton::clicked, this, &LoginWidget::toggleMode);
        
        connect(findPwdBtn, &QPushButton::clicked, this, [this]() {
            isFindPwdMode = true;
            isRegisterMode = false;
            udidLineEdit->clear();
            updateUIState();
        });

        connect(webSocketClient, &QWebSocket::connected, this, [this]() {
            setStatus("已连接服务器");
            actionButton->setEnabled(true);
        });
        connect(webSocketClient, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            setStatus("连接失败: " + webSocketClient->errorString(), true);
            actionButton->setEnabled(false);
        });

        webSocketClient->open(QUrl("ws://" + Config::SERVER_IP() + ":" + QString::number(Config::SERVER_PORT)));

        auto server = new QTcpServer(this);
        server->listen(QHostAddress::Any, 0);
    }
    
signals:
    void authorized(const QJsonValue &account);

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }    

    QLabel *titleLabel;
    QLineEdit *phoneLineEdit;
    QLineEdit *udidLineEdit;
    QLineEdit *passwordLineEdit;
    QLineEdit *confirmLineEdit;
    QCheckBox *rememberCheckBox;
    QPushButton *findPwdBtn;
    QPushButton *actionButton;
    QPushButton *switchButton;
    QLabel *statusLabel;
    
    bool isRegisterMode = false;
    bool isFindPwdMode = false;

    QString encrypt(const QString &input) {
        QByteArray data = input.toUtf8();
        // 1. 生成 4 字节随机盐
        QByteArray salt;
        for(int i=0; i<4; ++i) salt.append(static_cast<char>(QRandomGenerator::global()->generate() % 256));

        // 2. 将原始 Key 与 Salt 混合生成真正的运行密钥
        QByteArray baseKey = "MySecretSaltKey";
        QByteArray realKey = QCryptographicHash::hash(baseKey + salt, QCryptographicHash::Sha1);

        // 3. 执行加密 (使用之前提到的 XOR 逻辑)
        for(int i = 0; i < data.size(); ++i) {
            data[i] = data[i] ^ realKey[i % realKey.size()];
        }

        // 4. 将 Salt 放在密文头部一起返回
        return (salt + data).toBase64();
    }

    QString decrypt(const QString &input) {
        QByteArray rawData = QByteArray::fromBase64(input.toUtf8());
        if(rawData.size() < 4) return "";

        // 1. 提取前 4 字节盐值
        QByteArray salt = rawData.left(4);
        QByteArray data = rawData.mid(4);

        // 2. 用同样的逻辑合成密钥
        QByteArray baseKey = "MySecretSaltKey";
        QByteArray realKey = QCryptographicHash::hash(baseKey + salt, QCryptographicHash::Sha1);

        // 3. 还原
        for(int i = 0; i < data.size(); ++i) {
            data[i] = data[i] ^ realKey[i % realKey.size()];
        }

        return QString::fromUtf8(data);
    }

    void loadCredentials() {
        bool remember = settings->value("remember", true).toBool();
        if (remember) {
            phoneLineEdit->setText(settings->value("phone").toString());
            QString encryptedPass = settings->value("password").toString();
            passwordLineEdit->setText(decrypt(encryptedPass));
            rememberCheckBox->setChecked(true);
        }
    }

    void saveCredentials(const QString &phone, const QString &password) {
        if (rememberCheckBox->isChecked()) {
            settings->setValue("remember", true);
            settings->setValue("phone", phone);
            settings->setValue("password", encrypt(password));
        } else {
            settings->setValue("remember", false);
            settings->remove("phone");
            settings->remove("password");
        }
    }

    void updateUIState() 
    {
        udidLineEdit->setVisible(isFindPwdMode);
        confirmLineEdit->setVisible(isRegisterMode || isFindPwdMode);
        confirmLineEdit->clear();
        
        rememberCheckBox->setVisible(!isRegisterMode && !isFindPwdMode);
        findPwdBtn->setVisible(!isRegisterMode && !isFindPwdMode);

        if (isRegisterMode) {
            titleLabel->setText("创建账号");
            actionButton->setText("立即注册");
            switchButton->setText("返回登录");
            passwordLineEdit->setPlaceholderText("密码 (6-32位，字母+数字+符号)");
        } else if (isFindPwdMode) {
            titleLabel->setText("找回密码");
            actionButton->setText("重置密码");
            switchButton->setText("返回登录");
            passwordLineEdit->setPlaceholderText("新密码 (6-32位，字母+数字+符号)");
        } else {
            titleLabel->setText("用户登录");
            actionButton->setText("登录");
            switchButton->setText("注册新账号");
            passwordLineEdit->setPlaceholderText("密码 (6-32位，字母+数字+符号)");
        }
        updateStyle();
    }

    void toggleMode()
    {
        if (isFindPwdMode) {
            isFindPwdMode = false;
            isRegisterMode = false;
        } else {
            isRegisterMode = !isRegisterMode;
        }
        udidLineEdit->clear();
        updateUIState();
    }

    void onAction()
    {
        if (!NetworkUtils::isCurrentNetworkAllowed()) {
            setStatus(QStringLiteral(R"(
                <div style="text-align: left; line-height: 1.6; font-size: 12px; color: #555555;">
                    <span style="font-weight: bold; color: #333333;">防火墙阻止了连接，请尝试以下操作：</span><br>
                    1. <b>有系统弹窗：</b>直接点击“允许访问”放行。<br>
                    2. <b>无系统弹窗：</b>在任务栏搜索并打开“防火墙”<br>
                    &nbsp;&nbsp;&nbsp;&nbsp;<b>➔</b> 允许应用通过 Windows 防火墙 <b>➔</b> 更改设置<br>
                    &nbsp;&nbsp;&nbsp;&nbsp;<b>➔</b> 允许其他应用 <b>➔</b> 添加RemotePro.exe路径<br>
                    &nbsp;&nbsp;&nbsp;&nbsp;<b>➔</b> 勾选“专用与公用”并保存。
                </div>
            )"));
            return;
        }

        if (!phoneLineEdit->hasAcceptableInput()) {
            setStatus("请输入正确的11位手机号", this);
            return;
        }

        if (!passwordLineEdit->hasAcceptableInput()) {
            setStatus("密码需为6-16位字母+数字组合", this);
            return;
        }

        if (!webSocketClient->isValid()) {
            setStatus("未连接服务器", this);
            return;
        }

        const auto& password = passwordLineEdit->text();
        if (isRegisterMode || isFindPwdMode) {
            const auto& confirm = confirmLineEdit->text();
            if (password != confirm) {
                setStatus("两次密码不一致", true);
                return;
            }
        }

        setStatus("处理中...");
        actionButton->setEnabled(false);

        const auto& phone = phoneLineEdit->text().trimmed();

        if (isFindPwdMode) {
            const auto& udid = udidLineEdit->text().trimmed();
            if (udid.isEmpty()) {
                setStatus("请输入设备UDID", true);
                actionButton->setEnabled(true);
                return;
            }

            webSocketClient->emitEvent("findPassword", QJsonObject{{"phone", phone}, {"udid", udid}, {"password", password}}, [=](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    setStatus("密码重置成功，请登录");
                    toggleMode(); // 自动返回登录界面
                    return;
                }
                setStatus(res["msg"].toString(), true);
            });
        }
        else if (isRegisterMode) {
            webSocketClient->emitEvent("register", QJsonObject{{"phone", phone}, {"password", password}, {"version", Config::VERSION}}, [=](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    saveCredentials(phone, password);
                    emit authorized(res["account"]);
                    close();
                    return;
                }
   
                setStatus(res["msg"].toString(), true);
            });
        }
        else {
            webSocketClient->emitEvent("login", QJsonObject{{"phone", phone}, {"password", password}, {"version", Config::VERSION}}, [=](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    saveCredentials(phone, password);
                    for (const QJsonValue& device: res[HIDE_STR("devices")].toArray()) {
                        const auto& udid = device[HIDE_STR("udid")].toString();
                        DeviceInfo::expirations[udid] = device[HIDE_STR("expireAt")].toInteger();
                        DeviceInfo::setLocker(udid, device["locker"].toString());
                    }
                    emit authorized(res["account"]);
                    close();
                    return;
                }

                setStatus(res["msg"].toString(), true);
            });
        }
    }

    void setStatus(QString text, bool isError = false) 
    {
        bool isDarkMode = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        QString color;
        if (isError)
            color = isDarkMode ? "#ef5350" : "#c0392b";
        else
            color = "palette(text)";
        
        statusLabel->setStyleSheet(QString("color: %1;").arg(color));
        statusLabel->setText(text);
    }

    void updateStyle()
    {
        bool isDarkMode = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        
        QString accent, normal;

        if (isRegisterMode || isFindPwdMode) {
            accent = isDarkMode ? "#e74c3c" : "#c0392b";
            normal = "white";
        } else {
            accent = "palette(highlight)";
            normal = "palette(highlighted-text)";
        }
        
        QString qss = QString(R"(
            QLineEdit { 
                padding: 12px; 
                font-size: 15px; 
                border: 1px solid palette(mid); 
                border-radius: 8px; 
                background: palette(base);
                color: palette(text);
                selection-background-color: palette(highlight);
            }
            QLineEdit:focus { 
                border: 2px solid %1; 
            }
            
            QPushButton { 
                padding: 12px; 
                font-size: 15px; 
                border-radius: 8px; 
                font-weight: bold;
            }

            /* 主按钮：实心颜色 */
            QPushButton#mainBtn { 
                background-color: %1; 
                color: %2; 
                border: 1px solid %1;
            }
            QPushButton#mainBtn:hover {
                opacity: 0.9;
            }
            QPushButton#mainBtn:pressed {
                padding-top: 14px;
                padding-bottom: 10px;
            }

            /* 副按钮：描边风格 */
            QPushButton#subBtn  { 
                background-color: transparent; 
                color: %1; 
                border: 1px solid %1; 
            }
            QPushButton#subBtn:hover { 
                background-color: %1; 
                color: %2;
            }
            QPushButton#subBtn:pressed {
                padding-top: 14px;
                padding-bottom: 10px;
            }
        )").arg(accent, normal);

        setStyleSheet(qss);
    }
};
