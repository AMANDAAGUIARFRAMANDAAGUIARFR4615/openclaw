#pragma once

#include "SafeObject.h"
#include <QDateTime>
#include <QObject>

class Account : public QObject {
    Q_OBJECT

public:
    static Account* getInstance() { static Account instance; return &instance; }

    QString id;
    QString phone;
    int balance = 0;
    SafeObject<qint64> loginTime;

private:
    explicit Account(QObject *parent = nullptr) : QObject(parent) {}
};
