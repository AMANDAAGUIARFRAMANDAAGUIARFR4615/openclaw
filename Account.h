#pragma once

#include "SafeObject.h"
#include <QDateTime>
#include <QJsonArray>
#include <QObject>

class Account : public QObject {
    Q_OBJECT

public:
    static Account* getInstance() { static Account* instance = new Account; return instance; }

    QString id;
    QString phone;
    int balance = 0;
    bool hasRedeemCode = false;
    SafeObject<qint64> loginTime;
    QJsonArray tabs;

private:
    explicit Account(QObject *parent = nullptr) : QObject(parent) {}
};
