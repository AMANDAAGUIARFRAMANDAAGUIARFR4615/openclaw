#pragma once

#include "DeviceConnection.h"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

class AppListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppListWidget(DeviceConnection* connection, QWidget *parent = nullptr);
    ~AppListWidget();

    void addApp(const QString &iconPath, const QString &appName, const QString &packageName);

private:
    DeviceConnection* const connection;
    QTableWidget *table;
    void setupTable();
    void applyStyle();
};
