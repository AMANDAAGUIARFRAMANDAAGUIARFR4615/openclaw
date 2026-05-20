#pragma once

#include "global.h"
#include "Safe.h"
#include "NetworkUtils.h"
#include "Tools.h"
#include "DeviceInfo.h"
#include "Account.h"
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
#include <QResizeEvent>
#include <QSizePolicy>
#include <QFrame>
#include <QLabel>
#include <QToolButton>
#include <QPainter>
#include <QLinearGradient>
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>
#include <functional>

class LoginInputFocusFilter : public QObject
{
public:
    LoginInputFocusFilter(QLineEdit *edit, QWidget *wrapper, std::function<void()> refresh, QObject *parent = nullptr)
        : QObject(parent ? parent : edit), lineEdit(edit), wrapper(wrapper), refreshFocus(std::move(refresh))
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == lineEdit
            && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
            refreshFocus();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QLineEdit *lineEdit;
    QWidget *wrapper;
    std::function<void()> refreshFocus;
};

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setAutoFillBackground(false);
        setObjectName("loginWidget");
        // 尺寸比例参考设计稿（1024×918，白色卡片约占窗口宽度 66%）
        resize(kLoginWindowWidth, kLoginWindowHeight);
        setMinimumSize(kLoginFormCardWidth + 96, 560);

        titleLabel = new QLabel("用户登录");
        titleLabel->setObjectName("loginTitle");
        titleLabel->setAlignment(Qt::AlignCenter);

        auto phoneValidator = new QRegularExpressionValidator(QRegularExpression("^1[3-9]\\d{9}$"), this);
        auto passwordValidator = new QRegularExpressionValidator(
            QRegularExpression("^(?=.*[A-Za-z])(?=.*\\d)[\\S]{6,32}$"), this);

        phoneLineEdit = new QLineEdit;
        phoneLineEdit->setPlaceholderText("手机号");
        phoneLineEdit->setValidator(phoneValidator);
        phoneLineEdit->setMaxLength(11);

        udidLineEdit = new QLineEdit;
        udidLineEdit->setPlaceholderText("请输入账号使用过的手机设备UDID");

        passwordLineEdit = new QLineEdit;
        passwordLineEdit->setPlaceholderText("密码 (6-32位，字母+数字+符号)");
        passwordLineEdit->setValidator(passwordValidator);
        passwordLineEdit->setEchoMode(QLineEdit::Password);

        passwordToggleBtn = new QToolButton;
        passwordToggleBtn->setObjectName("passwordToggleBtn");
        passwordToggleBtn->setCursor(Qt::PointingHandCursor);
        passwordToggleBtn->setAutoRaise(true);
        passwordToggleBtn->setFocusPolicy(Qt::NoFocus);
        updatePasswordToggleIcon();

        confirmLineEdit = new QLineEdit;
        confirmLineEdit->setPlaceholderText("确认密码");
        confirmLineEdit->setValidator(passwordValidator);
        confirmLineEdit->setEchoMode(QLineEdit::Password);

        rememberCheckBox = new QCheckBox("记住账号和密码");
        lanModeCheckBox = new QCheckBox("局域网模式");
        lanModeCheckBox->setToolTip("广域网只能连接已在局域网连接过的设备");
        lanModeCheckBox->setChecked(settings->value("isLanMode", true).toBool());

        findPwdBtn = new QPushButton("忘记密码?");
        findPwdBtn->setObjectName("linkBtn");
        findPwdBtn->setCursor(Qt::PointingHandCursor);

        actionButton = new QPushButton("登录");
        actionButton->setObjectName("mainBtn");
        actionButton->setEnabled(false);
        actionButton->setCursor(Qt::PointingHandCursor);
        switchButton = new QPushButton("注册新账号");
        switchButton->setObjectName("subBtn");
        switchButton->setCursor(Qt::PointingHandCursor);

        statusLabel = new QLabel("正在连接服务器...");
        statusLabel->setObjectName("loginStatus");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setOpenExternalLinks(true);
        statusLabel->setWordWrap(true);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->setSpacing(14);
        buttonLayout->addWidget(actionButton, 1);
        buttonLayout->addWidget(switchButton, 1);

        auto *optionsLayout = new QHBoxLayout;
        optionsLayout->setSpacing(18);
        optionsLayout->addWidget(rememberCheckBox);
        optionsLayout->addWidget(lanModeCheckBox);
        optionsLayout->addStretch();
        optionsLayout->addWidget(findPwdBtn);

        auto *formContainer = new QFrame(this);
        formContainer->setObjectName("loginFormCard");
        auto *formLayout = new QVBoxLayout(formContainer);
        formLayout->setContentsMargins(40, 32, 40, 32);
        formLayout->setSpacing(14);
        formLayout->addWidget(titleLabel, 0, Qt::AlignHCenter);
        phoneField = wrapInputField(phoneLineEdit, InputIcon::User);
        udidField = wrapInputField(udidLineEdit, InputIcon::Device);
        passwordField = wrapInputField(passwordLineEdit, InputIcon::Lock, passwordToggleBtn);
        confirmField = wrapInputField(confirmLineEdit, InputIcon::Lock);
        formLayout->addWidget(phoneField);
        formLayout->addWidget(udidField);
        formLayout->addWidget(passwordField);
        formLayout->addWidget(confirmField);
        formLayout->addLayout(optionsLayout);
        formLayout->addSpacing(6);
        formLayout->addLayout(buttonLayout);

        formContainer->setFixedWidth(kLoginFormCardWidth);
        formContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

        auto *shadow = new QGraphicsDropShadowEffect(formContainer);
        shadow->setBlurRadius(32);
        shadow->setOffset(0, 8);
        shadow->setColor(QColor(74, 134, 247, 45));
        formContainer->setGraphicsEffect(shadow);

        auto *windowLayout = new QVBoxLayout(this);
        windowLayout->setContentsMargins(24, 24, 24, 28);
        windowLayout->setSpacing(0);
        windowLayout->addStretch(1);
        windowLayout->addWidget(formContainer, 0, Qt::AlignHCenter);
        windowLayout->addStretch(1);
        windowLayout->addWidget(statusLabel, 0, Qt::AlignHCenter);

        updateStyle();
        loadCredentials();
        updateUIState();

        connect(actionButton, &QPushButton::clicked, this, &LoginWidget::onAction);
        connect(switchButton, &QPushButton::clicked, this, &LoginWidget::toggleMode);
        connect(passwordToggleBtn, &QToolButton::clicked, this, &LoginWidget::togglePasswordVisible);
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
    void authorized(const QJsonValue &account, bool isLanMode);

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal w = width();
        const qreal h = height();
        const bool dark = isDarkMode();

        QLinearGradient sky(0, 0, 0, h);
        if (dark) {
            sky.setColorAt(0.0, QColor("#0D1117"));
            sky.setColorAt(0.45, QColor("#111827"));
            sky.setColorAt(1.0, QColor("#1A2332"));
        } else {
            sky.setColorAt(0.0, QColor("#F3F9FF"));
            sky.setColorAt(0.45, QColor("#E4F1FF"));
            sky.setColorAt(1.0, QColor("#D4E8FF"));
        }
        painter.fillRect(rect(), sky);

        QRadialGradient topGlow(w * 0.08, h * 0.06, w * 0.55);
        if (dark) {
            topGlow.setColorAt(0.0, QColor(74, 134, 247, 55));
            topGlow.setColorAt(0.55, QColor(45, 90, 160, 25));
            topGlow.setColorAt(1.0, QColor(17, 24, 39, 0));
        } else {
            topGlow.setColorAt(0.0, QColor(120, 178, 255, 95));
            topGlow.setColorAt(0.55, QColor(170, 210, 255, 35));
            topGlow.setColorAt(1.0, QColor(227, 241, 255, 0));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(topGlow);
        painter.drawEllipse(QRectF(-w * 0.18, -h * 0.12, w * 0.72, h * 0.55));

        QRadialGradient bottomGlow(w * 0.82, h * 0.92, w * 0.42);
        if (dark) {
            bottomGlow.setColorAt(0.0, QColor(74, 134, 247, 30));
            bottomGlow.setColorAt(1.0, QColor(17, 24, 39, 0));
        } else {
            bottomGlow.setColorAt(0.0, QColor(255, 255, 255, 120));
            bottomGlow.setColorAt(1.0, QColor(255, 255, 255, 0));
        }
        painter.setBrush(bottomGlow);
        painter.drawEllipse(QRectF(w * 0.48, h * 0.58, w * 0.62, h * 0.42));

        auto drawDotGrid = [&](qreal originX, qreal originY, int rows, int cols, qreal spacing, qreal radius, int alpha) {
            painter.setBrush(QColor(dark ? 74 : 96, dark ? 134 : 153, dark ? 247 : 240, alpha));
            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col)
                    painter.drawEllipse(QPointF(originX + col * spacing, originY + row * spacing), radius, radius);
            }
        };
        drawDotGrid(w - 118, 42, 7, 6, 16, 2.4, dark ? 28 : 42);
        drawDotGrid(34, h - 128, 5, 5, 15, 2.2, dark ? 24 : 36);

        auto drawWave = [&](qreal baseY, const QColor &color, qreal amplitude) {
            QPainterPath path;
            path.moveTo(-20, h + 20);
            path.lineTo(-20, baseY);
            const int segments = 6;
            const qreal step = (w + 40) / segments;
            for (int i = 0; i <= segments; ++i) {
                const qreal x = -20 + i * step;
                const qreal ctrlY = baseY + ((i % 2 == 0) ? -amplitude : amplitude * 0.55);
                const qreal nextX = -20 + (i + 1) * step;
                if (i == segments) {
                    path.lineTo(nextX, baseY);
                    break;
                }
                path.quadTo((x + nextX) / 2, ctrlY, nextX, baseY);
            }
            path.lineTo(w + 20, h + 20);
            path.closeSubpath();
            painter.setBrush(color);
            painter.drawPath(path);
        };

        if (dark) {
            drawWave(h * 0.78, QColor(31, 35, 43, 120), h * 0.05);
            drawWave(h * 0.86, QColor(37, 43, 53, 100), h * 0.04);
            drawWave(h * 0.93, QColor(26, 35, 50, 90), h * 0.03);
        } else {
            drawWave(h * 0.78, QColor(255, 255, 255, 95), h * 0.05);
            drawWave(h * 0.86, QColor(214, 232, 255, 150), h * 0.04);
            drawWave(h * 0.93, QColor(196, 221, 255, 120), h * 0.03);
        }

        QWidget::paintEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        update();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

private:
    // 参考设计稿比例（卡片宽约等于窗口宽 66%），尺寸按常见登录窗缩放
    static constexpr int kLoginWindowWidth = 520;
    static constexpr int kLoginWindowHeight = 660;
    static constexpr int kLoginFormCardWidth = kLoginWindowWidth * 66 / 100; // 343

    enum class InputIcon { User, Lock, Device };

    QLabel *titleLabel = nullptr;
    QWidget *phoneField = nullptr;
    QWidget *udidField = nullptr;
    QWidget *passwordField = nullptr;
    QWidget *confirmField = nullptr;
    QLineEdit *phoneLineEdit = nullptr;
    QLineEdit *udidLineEdit = nullptr;
    QLineEdit *passwordLineEdit = nullptr;
    QLineEdit *confirmLineEdit = nullptr;
    QToolButton *passwordToggleBtn = nullptr;
    QCheckBox *rememberCheckBox = nullptr;
    QCheckBox *lanModeCheckBox = nullptr;
    QPushButton *findPwdBtn = nullptr;
    QPushButton *actionButton = nullptr;
    QPushButton *switchButton = nullptr;
    QLabel *statusLabel = nullptr;

    bool isRegisterMode = false;
    bool isFindPwdMode = false;
    bool passwordVisible = false;

    static QPixmap makeInputIconPixmap(InputIcon type, const QColor &color)
    {
        QPixmap pixmap(22, 22);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        switch (type) {
        case InputIcon::User:
            painter.drawEllipse(QRectF(6.5, 3.5, 9, 9));
            painter.drawArc(QRectF(3.5, 12.5, 15, 8), 0, 180 * 16);
            break;
        case InputIcon::Lock:
            painter.drawRoundedRect(QRectF(5.5, 10.0, 11, 9), 2, 2);
            painter.drawArc(QRectF(7.0, 4.0, 8, 8), 0, 180 * 16);
            painter.drawLine(QPointF(11, 13.5), QPointF(11, 16.0));
            break;
        case InputIcon::Device:
            painter.drawRoundedRect(QRectF(4.5, 5.0, 13, 13), 2.5, 2.5);
            painter.drawLine(QPointF(8.0, 15.5), QPointF(14.0, 15.5));
            break;
        }

        return pixmap;
    }

    QWidget *wrapInputField(QLineEdit *lineEdit, InputIcon iconType, QToolButton *trailingButton = nullptr)
    {
        auto *wrapper = new QFrame;
        wrapper->setObjectName("inputField");
        wrapper->setProperty("focused", false);

        auto *layout = new QHBoxLayout(wrapper);
        layout->setContentsMargins(14, 0, trailingButton ? 6 : 14, 0);
        layout->setSpacing(10);

        auto *iconLabel = new QLabel;
        iconLabel->setObjectName("inputIcon");
        iconLabel->setPixmap(makeInputIconPixmap(iconType, QColor("#4A86F7")));
        iconLabel->setFixedSize(22, 22);
        iconLabel->setAlignment(Qt::AlignCenter);

        lineEdit->setFrame(false);
        lineEdit->setObjectName("loginLineEdit");

        layout->addWidget(iconLabel);
        layout->addWidget(lineEdit, 1);
        if (trailingButton)
            layout->addWidget(trailingButton);

        connect(lineEdit, &QLineEdit::textChanged, wrapper, [wrapper, lineEdit]() {
            wrapper->setProperty("hasText", !lineEdit->text().isEmpty());
            wrapper->style()->unpolish(wrapper);
            wrapper->style()->polish(wrapper);
        });
        connect(lineEdit, &QLineEdit::selectionChanged, wrapper, [wrapper, lineEdit]() {
            const bool focused = lineEdit->hasFocus();
            if (wrapper->property("focused").toBool() == focused)
                return;
            wrapper->setProperty("focused", focused);
            wrapper->style()->unpolish(wrapper);
            wrapper->style()->polish(wrapper);
        });

        auto refreshFocus = [wrapper, lineEdit]() {
            const bool focused = lineEdit->hasFocus();
            wrapper->setProperty("focused", focused);
            wrapper->style()->unpolish(wrapper);
            wrapper->style()->polish(wrapper);
        };
        auto *focusFilter = new LoginInputFocusFilter(lineEdit, wrapper, refreshFocus, lineEdit);
        lineEdit->installEventFilter(focusFilter);

        wrapper->setProperty("hasText", !lineEdit->text().isEmpty());
        return wrapper;
    }

    static bool isDarkMode()
    {
        return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }

    void updatePasswordToggleIcon()
    {
        const QColor color(isDarkMode() ? "#8B95A8" : "#6B7C93");
        QPixmap pixmap(22, 22);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        painter.drawEllipse(QRectF(4.5, 7.0, 13, 9));
        painter.drawEllipse(QRectF(8.5, 8.5, 5, 5));
        if (!passwordVisible) {
            painter.drawLine(QPointF(4.0, 17.0), QPointF(18.0, 6.0));
        }

        passwordToggleBtn->setIcon(QIcon(pixmap));
        passwordToggleBtn->setIconSize(QSize(22, 22));
    }

    void togglePasswordVisible()
    {
        passwordVisible = !passwordVisible;
        const auto mode = passwordVisible ? QLineEdit::Normal : QLineEdit::Password;
        passwordLineEdit->setEchoMode(mode);
        if (confirmField->isVisible())
            confirmLineEdit->setEchoMode(mode);
        updatePasswordToggleIcon();
    }

    QString encrypt(const QString &input)
    {
        QByteArray data = input.toUtf8();
        QByteArray salt;
        for (int i = 0; i < 4; ++i)
            salt.append(static_cast<char>(QRandomGenerator::global()->generate() % 256));

        QByteArray baseKey = "MySecretSaltKey";
        QByteArray realKey = QCryptographicHash::hash(baseKey + salt, QCryptographicHash::Sha1);

        for (int i = 0; i < data.size(); ++i)
            data[i] = data[i] ^ realKey[i % realKey.size()];

        return (salt + data).toBase64();
    }

    QString decrypt(const QString &input)
    {
        QByteArray rawData = QByteArray::fromBase64(input.toUtf8());
        if (rawData.size() < 4)
            return "";

        QByteArray salt = rawData.left(4);
        QByteArray data = rawData.mid(4);

        QByteArray baseKey = "MySecretSaltKey";
        QByteArray realKey = QCryptographicHash::hash(baseKey + salt, QCryptographicHash::Sha1);

        for (int i = 0; i < data.size(); ++i)
            data[i] = data[i] ^ realKey[i % realKey.size()];

        return QString::fromUtf8(data);
    }

    void loadCredentials()
    {
        bool remember = settings->value("remember", true).toBool();
        if (remember) {
            phoneLineEdit->setText(settings->value("phone").toString());
            QString encryptedPass = settings->value("password").toString();
            passwordLineEdit->setText(decrypt(encryptedPass));
            rememberCheckBox->setChecked(true);
        }
    }

    void saveCredentials(const QString &phone, const QString &password)
    {
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
        udidField->setVisible(isFindPwdMode);
        confirmField->setVisible(isRegisterMode || isFindPwdMode);
        if (!isRegisterMode && !isFindPwdMode)
            confirmLineEdit->clear();

        rememberCheckBox->setVisible(!isRegisterMode && !isFindPwdMode);
        lanModeCheckBox->setVisible(!isRegisterMode && !isFindPwdMode);
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
            passwordToggleBtn->setVisible(true);
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
            setStatus("请输入正确的11位手机号", true);
            return;
        }

        if (!passwordLineEdit->hasAcceptableInput()) {
            setStatus("密码需为6-16位字母+数字组合", true);
            return;
        }

        if (!webSocketClient->isValid()) {
            setStatus("未连接服务器", true);
            return;
        }

        const auto &password = passwordLineEdit->text();
        if (isRegisterMode || isFindPwdMode) {
            const auto &confirm = confirmLineEdit->text();
            if (password != confirm) {
                setStatus("两次密码不一致", true);
                return;
            }
        }

        setStatus("处理中...");
        actionButton->setEnabled(false);

        const auto &phone = phoneLineEdit->text().trimmed();

        if (isFindPwdMode) {
            const auto &udid = udidLineEdit->text().trimmed();
            if (udid.isEmpty()) {
                setStatus("请输入设备UDID", true);
                actionButton->setEnabled(true);
                return;
            }

            webSocketClient->emitEvent("findPassword", QJsonObject{{"phone", phone}, {"udid", udid}, {"password", password}}, [=](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    setStatus("密码重置成功，请登录");
                    toggleMode();
                    return;
                }
                setStatus(res["msg"].toString(), true);
            });
        } else if (isRegisterMode) {
            webSocketClient->emitEvent("register", QJsonObject{{"phone", phone}, {"password", password}, {"version", Config::VERSION}}, [=](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    settings->setValue("isLanMode", lanModeCheckBox->isChecked());
                    saveCredentials(phone, password);
                    Account::getInstance()->tabs = res["tabs"].toArray();
                    emit authorized(res["account"], lanModeCheckBox->isChecked());
                    close();
                    return;
                }

                setStatus(res["msg"].toString(), true);
            });
        } else {
            webSocketClient->emitEvent("login", QJsonObject{{"phone", phone}, {"password", password}, {"version", Config::VERSION}}, [=](const QJsonValue &res) {
                actionButton->setEnabled(true);

                if (res["msg"].isUndefined()) {
                    settings->setValue("isLanMode", lanModeCheckBox->isChecked());
                    saveCredentials(phone, password);
                    for (const QJsonValue &device : res[HIDE_STR("devices")].toArray()) {
                        const auto &udid = device[HIDE_STR("udid")].toString();
                        DeviceInfo::expirations[udid] = device[HIDE_STR("expireAt")].toInteger();
                        DeviceInfo::remotePorts[udid] = static_cast<quint16>(device["remotePort"].toInt());
                        DeviceInfo::setLocker(udid, device["locker"].toString());
                    }
                    Account::getInstance()->tabs = res["tabs"].toArray();
                    emit authorized(res["account"], lanModeCheckBox->isChecked());
                    close();
                    return;
                }

                setStatus(res["msg"].toString(), true);
            });
        }
    }

    void setStatus(const QString &text, bool isError = false)
    {
        if (text.contains(QLatin1Char('<'))) {
            statusLabel->setText(text);
            return;
        }

        const bool dark = isDarkMode();
        QString color;
        QString prefix;
        if (isError) {
            color = dark ? "#F87171" : "#E74C3C";
            prefix = QStringLiteral("<span style='color:%1;font-size:14px;'>●</span> ").arg(color);
        } else if (text.contains("已连接") || text.contains("成功")) {
            color = dark ? "#34D399" : "#22C55E";
            prefix = QStringLiteral("<span style='color:%1;font-size:14px;'>●</span> ").arg(color);
        } else {
            color = dark ? "#A8B4C4" : "#5B6B82";
            prefix = QStringLiteral("<span style='color:%1;font-size:14px;'>●</span> ").arg(dark ? "#6B7C93" : "#94A3B8");
        }

        statusLabel->setText(prefix
                             + QString("<span style='color:%1;font-size:13px;'>%2</span>")
                                   .arg(color, text.toHtmlEscaped()));
    }

    void updateStyle()
    {
        QString accent;
        QString accentHover;
        QString accentPressed;

        if (isRegisterMode || isFindPwdMode) {
            accent = "#E74C3C";
            accentHover = "#D44334";
            accentPressed = "#C0392B";
        } else {
            accent = "#4A86F7";
            accentHover = "#3B78EE";
            accentPressed = "#2F6AE0";
        }

        const bool dark = isDarkMode();
        const QString cardBg = dark ? "#1F232B" : "#FFFFFF";
        const QString cardBorder = dark ? "#3A3F4B" : "rgba(255, 255, 255, 220)";
        const QString titleColor = dark ? "#E2E8F0" : "#1E3A6E";
        const QString inputBg = dark ? "#252B35" : "#FFFFFF";
        const QString inputBorder = dark ? "#3A3F4B" : "#D7E2F1";
        const QString inputText = dark ? "#E2E8F0" : "#1F2937";
        const QString placeholder = dark ? "#6B7C93" : "#9AA8BC";
        const QString checkText = dark ? "#C8D1DC" : "#4B5563";
        const QString checkBorder = dark ? "#4A5568" : "#C5D3E8";
        const QString checkBg = dark ? "#252B35" : "#FFFFFF";
        const QString subBtnBg = dark ? "#252B35" : "#FFFFFF";
        const QString subBtnHover = dark ? "rgba(74, 134, 247, 24)" : "rgba(74, 134, 247, 10)";
        const QString subBtnPressed = dark ? "rgba(74, 134, 247, 36)" : "rgba(74, 134, 247, 20)";
        const QString toggleHover = dark ? "rgba(74, 134, 247, 28)" : "rgba(74, 134, 247, 18)";
        const QString disabledBtn = dark ? "#3A4A6B" : "#AFC6F8";

        const QString qss = QString(R"(
            QWidget#loginWidget {
                background: transparent;
            }

            QFrame#loginFormCard {
                background: %4;
                border: 1px solid %5;
                border-radius: 18px;
            }

            QLabel#loginTitle {
                color: %6;
                font-size: 26px;
                font-weight: 700;
                padding: 0px 0px 6px 0px;
                min-height: 36px;
            }

            QFrame#inputField {
                background: %7;
                border: 1px solid %8;
                border-radius: 12px;
                min-height: 48px;
            }
            QFrame#inputField[focused="true"] {
                border: 2px solid %1;
            }
            QFrame#inputField[hasText="true"] {
                border: 1px solid %1;
            }
            QFrame#inputField[focused="true"][hasText="true"] {
                border: 2px solid %1;
            }

            QLineEdit#loginLineEdit {
                border: none;
                background: transparent;
                color: %9;
                font-size: 15px;
                padding: 12px 0px;
                selection-background-color: %1;
            }
            QLineEdit#loginLineEdit::placeholder {
                color: %10;
            }

            QToolButton#passwordToggleBtn {
                border: none;
                background: transparent;
                padding: 4px;
                min-width: 28px;
                min-height: 28px;
            }
            QToolButton#passwordToggleBtn:hover {
                background: %16;
                border-radius: 8px;
            }

            QCheckBox {
                color: %11;
                font-size: 13px;
                spacing: 8px;
            }
            QCheckBox::indicator {
                width: 18px;
                height: 18px;
                border-radius: 4px;
                border: 1px solid %12;
                background: %13;
            }
            QCheckBox::indicator:checked {
                background: %1;
                border-color: %1;
                image: url(:/icons/check_white.svg);
            }
            QCheckBox::indicator:hover {
                border-color: %1;
            }

            QPushButton#linkBtn {
                border: none;
                color: %1;
                background: transparent;
                font-size: 13px;
                padding: 0px;
                text-align: right;
            }
            QPushButton#linkBtn:hover {
                color: %2;
                text-decoration: underline;
            }

            QPushButton {
                min-height: 46px;
                font-size: 15px;
                border-radius: 12px;
                font-weight: 600;
                padding: 0px 16px;
            }

            QPushButton#mainBtn {
                background-color: %1;
                color: #FFFFFF;
                border: 1px solid %1;
            }
            QPushButton#mainBtn:hover {
                background-color: %2;
                border-color: %2;
            }
            QPushButton#mainBtn:pressed {
                background-color: %3;
                border-color: %3;
            }
            QPushButton#mainBtn:disabled {
                background-color: %17;
                border-color: %17;
                color: rgba(255, 255, 255, 210);
            }

            QPushButton#subBtn {
                background-color: %14;
                color: %1;
                border: 1px solid %1;
            }
            QPushButton#subBtn:hover {
                background-color: %15;
            }
            QPushButton#subBtn:pressed {
                background-color: %18;
            }

            QLabel#loginStatus {
                background: transparent;
                padding: 4px 12px 0px 12px;
            }
        )").arg(accent, accentHover, accentPressed,
                 cardBg, cardBorder, titleColor, inputBg, inputBorder, inputText, placeholder,
                 checkText, checkBorder, checkBg, subBtnBg, subBtnHover, toggleHover, disabledBtn, subBtnPressed);

        setStyleSheet(qss);

        if (auto *card = findChild<QFrame *>("loginFormCard")) {
            if (auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(card->graphicsEffect())) {
                shadow->setColor(dark ? QColor(0, 0, 0, 90) : QColor(74, 134, 247, 45));
            }
        }
    }
};
