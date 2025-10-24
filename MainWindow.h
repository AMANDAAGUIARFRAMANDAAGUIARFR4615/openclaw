#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include <QMainWindow>
#include <QKeyEvent>
#include <QGridLayout>
#include <QTcpSocket>

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
    void onTabClicked(int index);

    QWidget* bottomWidget;
    QGridLayout* gridLayout;
};
