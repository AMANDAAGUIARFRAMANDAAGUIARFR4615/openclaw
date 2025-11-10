#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include <QMainWindow>
#include <QKeyEvent>
#include <QFrame>
#include <QDialog>

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
    void showTabManager(const QPoint &pos);

    QList<QFrame*> devices;

    const int minItemWidth = 150;
    const int minItemHeight = minItemWidth * 1.7786;
    const int spacing = 10;

    QTabWidget* tabWidget;
    QDialog *qrDialog = nullptr;
};