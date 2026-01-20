#pragma once

#include "global.h"
#include "SafeObject.h"
#include "Safe.h"
#include "NetworkUtils.h"
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

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        
        titleLabel = new QLabel("用户登录");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font: bold 26px; margin: 20px; color: palette(text);");

        auto phoneValidator = new QRegularExpressionValidator(QRegularExpression("^1[3-9]\\d{9}$"), this);
        // ^(?=.*[A-Za-z])   : 必须包含至少一个字母
        // (?=.*\\d)         : 必须包含至少一个数字
        // [\\S]{6,32}$      : 允许任何非空白字符(含特殊符号)，长度6-32位
        auto passwordValidator = new QRegularExpressionValidator(QRegularExpression("^(?=.*[A-Za-z])(?=.*\\d)[\\S]{6,32}$"), this);

        phoneLineEdit = new QLineEdit;
        phoneLineEdit->setPlaceholderText("手机号");
        phoneLineEdit->setValidator(phoneValidator);
        phoneLineEdit->setMaxLength(11);

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

        actionButton = new QPushButton("登录");
        actionButton->setObjectName("mainBtn");
        switchButton = new QPushButton("注册新账号");
        switchButton->setObjectName("subBtn");
        
        statusLabel = new QLabel("正在连接服务器...");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setOpenExternalLinks(true);

        auto buttonLayout = new QHBoxLayout;
        buttonLayout->addWidget(actionButton);
        buttonLayout->addWidget(switchButton);

        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 30, 40, 30);
        mainLayout->setSpacing(16);
        mainLayout->addStretch();
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(phoneLineEdit);
        mainLayout->addWidget(passwordLineEdit);
        mainLayout->addWidget(confirmLineEdit);
        mainLayout->addWidget(rememberCheckBox);
        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(statusLabel);
        mainLayout->addStretch();

        setMinimumWidth(400);
        updateStyle();

        loadCredentials();

        connect(actionButton, &QPushButton::clicked, this, &LoginWidget::onAction);
        connect(switchButton, &QPushButton::clicked, this, &LoginWidget::toggleMode);

        connect(webSocketClient, &QWebSocket::connected, this, [this]() {
            setStatus("已连接服务器");
            actionButton->setEnabled(true);
        });
        connect(webSocketClient, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            setStatus("连接失败: " + webSocketClient->errorString(), true);
            actionButton->setEnabled(false);
        });

        connect(webSocketClient, &QWebSocket::sslErrors, this, [&](const QList<QSslError> &errors) {
            qDebugEx() << "====== 捕获到 SSL 错误 ======";

            for (const auto &error : errors) {
                qDebugEx() << "错误描述:" << error.errorString();

                // 如果是主机名不匹配，打印证书里的名字看看
                if (error.error() == QSslError::HostNameMismatch) {
                    qDebugEx() << "证书内的 Common Name:"
                             << error.certificate().subjectInfo(QSslCertificate::CommonName);
                }
            }

            qDebugEx() << "正在执行 ignoreSslErrors() 以忽略上述错误...";
            webSocketClient->ignoreSslErrors();
        });

        webSocketClient->open(QUrl("ws://" + Config::SERVER_IP + ":" + QString::number(Config::SERVER_PORT)));

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
    QLineEdit *passwordLineEdit;
    QLineEdit *confirmLineEdit;
    QCheckBox *rememberCheckBox;
    QPushButton *actionButton;
    QPushButton *switchButton;
    QLabel *statusLabel;
    bool isRegisterMode = false;

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

    void toggleMode()
    {
        isRegisterMode = !isRegisterMode;
        confirmLineEdit->setVisible(isRegisterMode);
        confirmLineEdit->clear();
        
        // 注册模式下隐藏“记住密码”
        rememberCheckBox->setVisible(!isRegisterMode);

        if (isRegisterMode) {
            titleLabel->setText("创建账号");
            actionButton->setText("立即注册");
            switchButton->setText("返回登录");
        } else {
            titleLabel->setText("用户登录");
            actionButton->setText("登录");
            switchButton->setText("注册新账号");
        }
        updateStyle();
    }

    void onAction()
    {
        if (!NetworkUtils::isFirewallAllowed()) {
            setStatus("防火墙阻止了连接，需要点击允许访问\n如果已经关闭弹窗，请换到其他路径再次运行程序");
            return;
        }

        const auto& phone = phoneLineEdit->text().trimmed();
        const auto& password = passwordLineEdit->text();

        if (phone.isEmpty() || password.isEmpty()) {
            setStatus("手机号和密码不能为空");
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

        setStatus("处理中...");
        actionButton->setEnabled(false);

        if (isRegisterMode) {
            const auto& confirm = confirmLineEdit->text();

            if (confirm.isEmpty()) {
                setStatus("请确认密码");
                actionButton->setEnabled(true);
                return;
            }

            if (password != confirm) {
                setStatus("两次密码不一致");
                actionButton->setEnabled(true);
                return;
            }

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
        } else {
            webSocketClient->emitEvent("login", QJsonObject{{"phone", phone}, {"password", password}, {"version", Config::VERSION}}, [=](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    saveCredentials(phone, password);
                    for (const QJsonValue& device: res["devices"].toArray()) {
                        const auto& udid = device["udid"].toString();
                        DeviceInfo::expirations[udid] = device[HIDE("expireAt")].toInteger();
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
        if (isRegisterMode) {
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
