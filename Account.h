#pragma once

#include "SafeObject.h"
#include <QObject>

class Account : public QObject {
    Q_OBJECT

public:
    static Account* getInstance() { static Account instance; return &instance; }

    QString id;
    QString phone;
    int balance = 0;
    SafeObject<QDateTime> loginTime;

private:
    explicit Account(QObject *parent = nullptr) : QObject(parent) {}
};
