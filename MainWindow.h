#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include <QMainWindow>
#include <QKeyEvent>
#include <QDialog>
#include <QScrollArea>
#include <QSettings>

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
    void onTabMoved(int fromIndex, int toIndex);
    void showTabBarContextMenu(const QPoint &pos);
    void loadTabs();
    void saveTabs();
    void addTab(int id, const QString &name);
    int findAvailableTabId();

    struct TabInfo {
        int id;
        QString name;
    };

    QList<TabInfo> tabs;

    QList<QFrame*> devices;

    const int minItemWidth = 150;
    const int minItemHeight = minItemWidth * 1.7786;
    const int spacing = 10;

    QTabWidget* tabWidget;
    QScrollArea* scrollArea;
    QDialog *qrDialog = nullptr;
    QSettings settings;
};