#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include "BitMaskEditorDialog.h"
#include <QMainWindow>
#include <QKeyEvent>
#include <QDialog>
#include <QScrollArea>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static MainWindow* getInstance() { static MainWindow instance; return &instance; }

    void addItem(DeviceConnection* connection);
    const QList<BitMaskEditorDialog::Item>& getTabs() const { return tabs; }

private:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void relayoutDevices();
    void onTabMoved(int fromIndex, int toIndex);
    void showTabBarContextMenu(const QPoint &pos);
    void loadTabs();
    void saveTabs(int index = -1);
    int findAvailableTabId();

    QList<BitMaskEditorDialog::Item> tabs;

    QHash<DeviceInfo*, QFrame*> deviceFrames;

    const int frameItemWidth = 150;
    const int frameItemHeight = frameItemWidth * 1.7786;
    const int spacing = 10;

    QTabWidget* tabWidget;
    QScrollArea* scrollArea;
};
