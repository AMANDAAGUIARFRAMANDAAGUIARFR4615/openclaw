#pragma once

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

class RegisterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RegisterWidget(QTcpSocket *socket, QWidget *parent = nullptr)
        : QWidget(parent), tcpSocket(socket)
    {
        setWindowTitle("用户注册");
        resize(420, 480);

        auto *titleLabel = new QLabel("创建账号");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font: bold 26px; color: #2c3e50; margin: 20px;");

        usernameLineEdit = new QLineEdit; usernameLineEdit->setPlaceholderText("用户名");
        passwordLineEdit = new QLineEdit; passwordLineEdit->setPlaceholderText("密码"); passwordLineEdit->setEchoMode(QLineEdit::Password);
        confirmPasswordLineEdit = new QLineEdit; confirmPasswordLineEdit->setPlaceholderText("确认密码"); confirmPasswordLineEdit->setEchoMode(QLineEdit::Password);

        registerButton = new QPushButton("注册");
        backButton = new QPushButton("返回登录");
        statusLabel = new QLabel;
        statusLabel->setAlignment(Qt::AlignCenter);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addWidget(registerButton);
        buttonLayout->addWidget(backButton);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 30, 40, 30);
        mainLayout->setSpacing(16);
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(usernameLineEdit);
        mainLayout->addWidget(passwordLineEdit);
        mainLayout->addWidget(confirmPasswordLineEdit);
        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(statusLabel);
        mainLayout->addStretch();

        setStyleSheet(R"(
            QLineEdit { padding: 12px; font-size: 15px; border: 2px solid #ddd; border-radius: 10px; }
            QLineEdit:focus { border-color: #e74c3c; }
            QPushButton { padding: 12px; font-size: 16px; border-radius: 10px; }
            QPushButton:first { background: #e74c3c; color: white; }
            QPushButton:last  { background: #95a5a6; color: white; }
        )");

        for (auto *w : {usernameLineEdit, passwordLineEdit, confirmPasswordLineEdit}) w->setMinimumHeight(44);
        for (auto *b : {registerButton, backButton}) b->setMinimumHeight(50);

        connect(registerButton, &QPushButton::clicked, this, &RegisterWidget::attemptRegister);
        connect(backButton,     &QPushButton::clicked, this, &QWidget::close);
        connect(tcpSocket,      &QTcpSocket::readyRead, this, &RegisterWidget::readServerResponse);
    }

private:
    void attemptRegister()
    {
        QString username = usernameLineEdit->text().trimmed();
        QString password = passwordLineEdit->text();
        QString confirm  = confirmPasswordLineEdit->text();

        if (username.isEmpty() || password.isEmpty()) {
            QMessageBox::warning(this, "错误", "请填写完整信息");
            return;
        }
        if (password != confirm) {
            QMessageBox::warning(this, "错误", "两次密码不一致");
            return;
        }

        QJsonObject obj{ {"type", "register"}, {"username", username}, {"password", password} };
        tcpSocket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
        statusLabel->setText("注册中...");
        registerButton->setEnabled(false);
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
            if (doc.isNull() || doc.object()["type"].toString() != "register") continue;

            auto obj = doc.object();
            bool success = obj["success"].toBool();
            QString msg = obj["message"].toString();

            registerButton->setEnabled(true);
            if (success) {
                QMessageBox::information(this, "成功", "注册成功，请登录");
                close();
            } else {
                statusLabel->setText(msg);
                QMessageBox::warning(this, "注册失败", msg);
            }
        }
    }

    QTcpSocket *tcpSocket;
    QLineEdit *usernameLineEdit;
    QLineEdit *passwordLineEdit;
    QLineEdit *confirmPasswordLineEdit;
    QPushButton *registerButton;
    QPushButton *backButton;
    QLabel *statusLabel;
    QByteArray receiveBuffer;
};
