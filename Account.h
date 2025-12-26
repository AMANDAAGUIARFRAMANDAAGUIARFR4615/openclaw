#pragma once

#include <QObject>

class Account : public QObject {
    Q_OBJECT

public:
    static Account* getInstance() { static Account instance; return &instance; }

    QString id;
    int balance = 0;

private:
    explicit Account(QObject *parent = nullptr) : QObject(parent) {}
};