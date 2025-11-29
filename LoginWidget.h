#pragma once

#include "global.h"
#include "ToastWidget.h"
#include "MainWindow.h"
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        titleLabel = new QLabel("用户登录");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font: bold 26px; color: #2c3e50; margin: 20px;");

        phoneLineEdit = new QLineEdit;
        phoneLineEdit->setPlaceholderText("手机号");

        passwordLineEdit = new QLineEdit;
        passwordLineEdit->setPlaceholderText("密码");
        passwordLineEdit->setEchoMode(QLineEdit::Password);

        confirmLineEdit = new QLineEdit;
        confirmLineEdit->setPlaceholderText("确认密码");
        confirmLineEdit->setEchoMode(QLineEdit::Password);
        confirmLineEdit->setVisible(false);

        actionButton = new QPushButton("登录");
        actionButton->setObjectName("mainBtn");
        switchButton = new QPushButton("注册新账号");
        switchButton->setObjectName("subBtn");
        
        statusLabel = new QLabel("正在连接服务器...");
        statusLabel->setAlignment(Qt::AlignCenter);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addWidget(actionButton);
        buttonLayout->addWidget(switchButton);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 30, 40, 30);
        mainLayout->setSpacing(16);
        mainLayout->addStretch();
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(phoneLineEdit);
        mainLayout->addWidget(passwordLineEdit);
        mainLayout->addWidget(confirmLineEdit);
        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(statusLabel);
        mainLayout->addStretch();

        setMinimumWidth(400);
        updateStyle();

        connect(actionButton, &QPushButton::clicked, this, &LoginWidget::onAction);
        connect(switchButton, &QPushButton::clicked, this, &LoginWidget::toggleMode);
        
        if (!webSocketClient.isValid()) {
            connect(&webSocketClient, &QWebSocket::connected, [this]() {
                setStatus("已连接服务器");
                actionButton->setEnabled(true);
            });
            connect(&webSocketClient, &QWebSocket::errorOccurred, [this](QAbstractSocket::SocketError) {
                setStatus("连接失败: " + webSocketClient.errorString(), true);
                actionButton->setEnabled(false);
            });
            webSocketClient.open(QUrl("ws://localhost:3000"));
        } else {
            setStatus("已连接服务器");
        }
    }

private:
    QLabel *titleLabel;
    QLineEdit *phoneLineEdit;
    QLineEdit *passwordLineEdit;
    QLineEdit *confirmLineEdit;
    QPushButton *actionButton;
    QPushButton *switchButton;
    QLabel *statusLabel;
    bool isRegisterMode = false;

    void toggleMode()
    {
        isRegisterMode = !isRegisterMode;
        confirmLineEdit->setVisible(isRegisterMode);
        confirmLineEdit->clear();
        
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
        QString user = phoneLineEdit->text().trimmed();
        QString pass = passwordLineEdit->text();

        if (user.isEmpty() || pass.isEmpty()) {
            new ToastWidget("请输入完整信息", this);
            return;
        }

        if (!webSocketClient.isValid()) {
            new ToastWidget("未连接服务器", this);
            return;
        }

        setStatus("处理中...");
        actionButton->setEnabled(false);

        if (isRegisterMode) {
            if (pass != confirmLineEdit->text()) {
                new ToastWidget("两次密码不一致", this);
                actionButton->setEnabled(true);
                return;
            }

            webSocketClient.emitEvent("register", QJsonObject{{"username", user}, {"password", pass}}, [this](const QJsonValue &res) {
                actionButton->setEnabled(true);
                // 根据实际协议修改判断逻辑，假设 msg 为空即成功
                if (res["msg"].isNull() || res["msg"].toString().isEmpty()) {
                    new ToastWidget("注册成功，请登录", this);
                    toggleMode();
                    setStatus("注册成功");
                } else {
                    setStatus(res["msg"].toString(), true);
                }
            });
        } else {
            webSocketClient.emitEvent("login", QJsonObject{{"phone", user}, {"password", pass}}, [this](const QJsonValue &res) {
                actionButton->setEnabled(true);
                if (res["msg"].isNull()) {
                    if (g_mainWindow) g_mainWindow->show();
                    this->close();
                    return;
                }
                setStatus(res["msg"].toString(), true);
            });
        }
    }

    void setStatus(QString text, bool isError = false) 
    {
        statusLabel->setStyleSheet(isError ? "color: #c0392b;" : "color: black;");
        statusLabel->setText(text);
    }

    void updateStyle()
    {
        QString color = isRegisterMode ? "#e74c3c" : "#3498db";
        QString qss = QString(R"(
            QLineEdit { 
                padding: 12px; font-size: 15px; border: 2px solid #ddd; border-radius: 10px; 
            }
            QLineEdit:focus { 
                border-color: %1; 
            }
            QPushButton { 
                padding: 12px; font-size: 16px; border-radius: 10px; 
            }
            QPushButton#mainBtn { 
                background: %1; color: white; border: none; font-weight: bold;
            }
            QPushButton#mainBtn:hover {
                opacity: 0.9;
            }
            QPushButton#subBtn  { 
                background: transparent; color: #7f8c8d; border: none; 
            }
            QPushButton#subBtn:hover { 
                color: %1; text-decoration: underline;
            }
        )").arg(color);
        setStyleSheet(qss);
    }
};