#pragma once

#include "RegisterWidget.h"
#include "global.h"
#include "ToastWidget.h"
#include "MainWindow.h"
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget()
    {
        auto *titleLabel = new QLabel("用户登录");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font: bold 26px; color: #2c3e50; margin: 20px;");

        phoneLineEdit = new QLineEdit;
        phoneLineEdit->setPlaceholderText("用户名");
        passwordLineEdit = new QLineEdit;
        passwordLineEdit->setPlaceholderText("密码");
        passwordLineEdit->setEchoMode(QLineEdit::Password);

        loginButton = new QPushButton("登录");
        registerButton = new QPushButton("注册新账号");
        statusLabel = new QLabel("正在连接服务器...");
        statusLabel->setAlignment(Qt::AlignCenter);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addWidget(loginButton);
        buttonLayout->addWidget(registerButton);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 30, 40, 30);
        mainLayout->setSpacing(16);
        mainLayout->addStretch();
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(phoneLineEdit);
        mainLayout->addWidget(passwordLineEdit);
        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(statusLabel);
        mainLayout->addStretch();

        setMinimumWidth(400);

        setStyleSheet(R"(
            QLineEdit { padding: 12px; font-size: 15px; border: 2px solid #ddd; border-radius: 10px; }
            QLineEdit:focus { border-color: #3498db; }
            QPushButton { padding: 12px; font-size: 16px; border-radius: 10px; }
            QPushButton:first { background: #3498db; color: white; }
            QPushButton:last  { background: #95a5a6; color: white; }
            QPushButton:hover { opacity: 0.9; }
        )");

        connect(loginButton, &QPushButton::clicked, [this]() {
            QString phone = phoneLineEdit->text().trimmed();
            QString password = passwordLineEdit->text();
            if (phone.isEmpty() || password.isEmpty()) {
                new ToastWidget("请输入账号密码", this);
                return;
            }

            if (!webSocketClient.isValid()) {
                new ToastWidget("未连上服务器", this);
                return;
            }

            webSocketClient.emitEvent("login", QJsonObject { {"phone", phone}, {"password", password} }, [this](const QJsonValue &res) {
                qDebugEx() << "收到登录 Ack:" << res;
                qDebugEx() << "收到登录 Ack:" << res["msg"] << res["msg"].isNull() << res["msg"].isUndefined();
                if (res["msg"].isNull()) {
                    g_mainWindow->show();
                    return;
                }

                setStatusLabel(res["msg"].toString(), true);
                loginButton->setEnabled(true);
            });
            setStatusLabel("登录中...");
            loginButton->setEnabled(false);
        });
        connect(registerButton, &QPushButton::clicked, [this]() {
            auto window = new RegisterWidget();
            window->show();
            hide();
        });
        
        connect(&webSocketClient, &QWebSocket::connected, [this]() {
            setStatusLabel("已连接服务器");
        });

        connect(&webSocketClient, &QWebSocket::errorOccurred, [this](QAbstractSocket::SocketError socketError) {
            setStatusLabel("连接失败: " + webSocketClient.errorString(), true);
        });
        
        webSocketClient.open(QUrl("ws://localhost:3000"));
    }

    void setStatusLabel(QString text, bool isError = false) {
        if (isError)
            statusLabel->setStyleSheet("color: #c0392b; font-weight: bold;");
        else
            statusLabel->setStyleSheet("color: #000000;");

        statusLabel->setText(text);
    }

private:
    QLineEdit *phoneLineEdit;
    QLineEdit *passwordLineEdit;
    QPushButton *loginButton;
    QPushButton *registerButton;
    QLabel *statusLabel;
};
