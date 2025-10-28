#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include <QMainWindow>
#include <QKeyEvent>
#include <QFrame>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void addItem(DeviceConnection* connection = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void relayoutDevices();
    void onTabClicked(int index);

    QList<QFrame*> devices;
};