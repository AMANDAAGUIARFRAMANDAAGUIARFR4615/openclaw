#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include "BitMaskEditorDialog.h"
#include "VideoVisibilityManager.h"
#include "SwitchButton.h"
#include <QMainWindow>
#include <QKeyEvent>
#include <QDialog>
#include <QListWidget>
#include <QSlider>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static MainWindow* getInstance() { static MainWindow instance; return &instance; }

    void addItem(DeviceConnection* connection);
    const QList<BitMaskEditorDialog::Item>& getTabs() const { return tabs; }
    BitMaskEditorDialog::Item& getTab() { return tabs[tabWidget->currentIndex()]; }
    void showSupportDialog();

    QTabWidget* const tabWidget;

    SwitchButton* const multiControlSwitchButton;
    SwitchButton* const lineDispatcherSwitchButton;

private:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void addOptionMenu(QMenu* parent, const QString& title, const QStringList& items, std::optional<int>* targetVar, std::function<void()> onChanged);
    void syncVideoQualityToDevices();
    void relayoutDevices();
    void onTabMoved(int fromIndex, int toIndex);
    void showTabBarContextMenu(const QPoint &pos);
    void loadTabs();
    void saveTabs(int index = -1);
    int findAvailableTabId();

    QList<BitMaskEditorDialog::Item> tabs;

    const int frameItemWidth = 300;
    const int spacing = 10;

    QListWidget* sideBarList;
    QListWidget* deviceListWidget;
    QSlider* zoomSlider;

    VideoVisibilityManager* videoVisibilityManager;
};
