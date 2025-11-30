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
        setAttribute(Qt::WA_DeleteOnClose);
        
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
        actionButton->setCursor(Qt::PointingHandCursor);
        switchButton = new QPushButton("注册新账号");
        switchButton->setObjectName("subBtn");
        switchButton->setCursor(Qt::PointingHandCursor);
        
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

        connect(&webSocketClient, &QWebSocket::connected, [this]() {
            setStatus("已连接服务器");
            actionButton->setEnabled(true);
        });
        connect(&webSocketClient, &QWebSocket::errorOccurred, [this](QAbstractSocket::SocketError) {
            setStatus("连接失败: " + webSocketClient.errorString(), true);
            actionButton->setEnabled(false);
        });
        webSocketClient.open(QUrl("ws://localhost:3000"));
    }

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
        QString phone = phoneLineEdit->text().trimmed();
        QString password = passwordLineEdit->text();

        if (phone.isEmpty() || password.isEmpty()) {
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
            if (password != confirmLineEdit->text()) {
                new ToastWidget("两次密码不一致", this);
                actionButton->setEnabled(true);
                return;
            }

            webSocketClient.emitEvent("register", QJsonObject{{"phone", phone}, {"password", password}}, [this](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    g_mainWindow->show();
                    close();
                    return;
                }
   
                setStatus(res["msg"].toString(), true);
            });
        } else {
            webSocketClient.emitEvent("login", QJsonObject{{"phone", phone}, {"password", password}}, [this](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    g_mainWindow->show();
                    close();
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
                padding: 12px; 
                font-size: 15px; 
                border: 2px solid #e0e0e0; 
                border-radius: 8px; 
                background: #f9f9f9;
            }
            QLineEdit:focus { 
                border-color: %1; 
                background: #ffffff;
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
                color: white; 
                border: 2px solid %1;
            }
            QPushButton#mainBtn:hover {
                background-color: palette(button-text);
                border-color: %1;
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
                border: 2px solid %1; 
            }
            QPushButton#subBtn:hover { 
                background-color: %1; 
                color: white;
            }
            QPushButton#subBtn:pressed {
                padding-top: 14px;
                padding-bottom: 10px;
            }
        )").arg(color);

        setStyleSheet(qss);
    }
};
