#pragma once

#include "RegisterWidget.h"
// #include "MainWindow.h"
#include <QWidget>
#include <QTcpSocket>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr)
        : QWidget(parent), tcpSocket(new QTcpSocket(this)), receiveBuffer()
    {
        auto *titleLabel = new QLabel("用户登录");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font: bold 26px; color: #2c3e50; margin: 20px;");

        usernameLineEdit = new QLineEdit;
        usernameLineEdit->setPlaceholderText("用户名");
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
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(usernameLineEdit);
        mainLayout->addWidget(passwordLineEdit);
        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(statusLabel);
        mainLayout->addStretch();

        setStyleSheet(R"(
            QLineEdit { padding: 12px; font-size: 15px; border: 2px solid #ddd; border-radius: 10px; }
            QLineEdit:focus { border-color: #3498db; }
            QPushButton { padding: 12px; font-size: 16px; border-radius: 10px; }
            QPushButton:first { background: #3498db; color: white; }
            QPushButton:last  { background: #95a5a6; color: white; }
            QPushButton:hover { opacity: 0.9; }
        )");

        for (auto *w : {usernameLineEdit, passwordLineEdit}) w->setMinimumHeight(44);
        for (auto *b : {loginButton, registerButton}) b->setMinimumHeight(50);

        connect(loginButton,    &QPushButton::clicked, this, &LoginWidget::attemptLogin);
        connect(registerButton, &QPushButton::clicked, this, &LoginWidget::openRegister);
        connect(tcpSocket,      &QTcpSocket::connected, this, [=]{ statusLabel->setText("已连接服务器"); });
        connect(tcpSocket,      &QTcpSocket::readyRead, this, &LoginWidget::readServerResponse);
        connect(tcpSocket,      &QTcpSocket::errorOccurred, this, [=](auto){ statusLabel->setText(tcpSocket->errorString()); });

        tcpSocket->connectToHost("127.0.0.1", 12345);
    }

private:
    void attemptLogin()
    {
        QString username = usernameLineEdit->text().trimmed();
        QString password = passwordLineEdit->text();
        if (username.isEmpty() || password.isEmpty()) {
            QMessageBox::warning(this, "错误", "请填写完整信息");
            return;
        }

        QJsonObject obj{ {"type", "login"}, {"username", username}, {"password", password} };
        tcpSocket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
        statusLabel->setText("登录中...");
        loginButton->setEnabled(false);
    }

    void openRegister()
    {
        auto window = new RegisterWidget(tcpSocket);
        window->show();
        hide();
    }

    void readServerResponse()
    {
        receiveBuffer.append(tcpSocket->readAll());
        while (true) {
            int pos = receiveBuffer.indexOf('\n');
            if (pos == -1) break;
            auto line = receiveBuffer.left(pos);
            receiveBuffer.remove(0, pos + 1);

            auto doc = QJsonDocument::fromJson(line);
            if (doc.isNull() || doc.object()["type"].toString() != "login") continue;

            auto obj = doc.object();
            bool success = obj["success"].toBool();
            QString msg = obj["message"].toString();

            loginButton->setEnabled(true);
            if (success) {
                // new MainWindow(tcpSocket, this)->show();
                close();
            } else {
                statusLabel->setText(msg);
                QMessageBox::warning(this, "登录失败", msg);
            }
        }
    }

    QTcpSocket *tcpSocket;
    QLineEdit *usernameLineEdit;
    QLineEdit *passwordLineEdit;
    QPushButton *loginButton;
    QPushButton *registerButton;
    QLabel *statusLabel;
    QByteArray receiveBuffer;
};
