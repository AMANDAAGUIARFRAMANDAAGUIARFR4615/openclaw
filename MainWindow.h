#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include "BitMaskEditorDialog.h"
#include "SwitchButton.h"
#include "ViewportAwareBehavior.h"
#include <QMainWindow>
#include <QKeyEvent>
#include <QDialog>
#include <QListWidget>
#include <QSlider>
#include <QSpinBox>

class DeviceView;
class DeviceWidget;
class DeviceWindow;
class QSplitter;
class QToolButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static MainWindow* getInstance() { static MainWindow* instance = new MainWindow; return instance; }

    void addItem(DeviceConnection* connection);
    QList<DeviceConnection*> getDeviceConnections(DeviceView* mainDeviceView = nullptr);
    QList<DeviceWidget*> getDeviceWidgets(DeviceView* mainDeviceView = nullptr);
    QList<DeviceWindow*> getDeviceWindows(DeviceView* mainDeviceView = nullptr);
    QList<QString> getDeviceUdids(DeviceView* mainDeviceView = nullptr);
    
    const QList<BitMaskEditorDialog::Item>& getTabs() const { return tabs; }
    BitMaskEditorDialog::Item& getTab() { return tabs[tabWidget->currentIndex()]; }
    void showSupportDialog();
    int getRandomDelay();

    void relayoutDevices();
    void doRelayoutDevices();

    QTabWidget* const tabWidget = new QTabWidget(this);

    SwitchButton* const multiControlSwitchButton = new SwitchButton("同屏操作");
    SwitchButton* const lineDispatcherSwitchButton = new SwitchButton("文本逐行分发");

    QCheckBox* const randomDelayCheckBox = new QCheckBox("鼠标随机延迟");
    QSpinBox* const minDelaySpinBox = new QSpinBox;
    QSpinBox* const maxDelaySpinBox = new QSpinBox;

private:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void addOptionMenu(QMenu* parent, const QString& title, const QStringList& items, std::optional<int>* targetVar, std::function<void()> onChanged);
    void syncVideoSettingsToDevices();
    void onTabMoved(int fromIndex, int toIndex);
    void showTabBarContextMenu(const QPoint &pos);
    void loadTabs();
    void saveTabs(int index = -1);
    int findAvailableTabId();
    void setSidebarCollapsed(bool collapsed);
    void updateSidebarToggleButtonPosition();

    QList<BitMaskEditorDialog::Item> tabs;

    const int frameItemWidth = 300;
    const int spacing = 10;

    QListWidget* sideBarList;
    QSplitter* mainSplitter;
    QToolButton* sideBarToggleButton;
    int sideBarExpandedWidth = 80;
    bool sideBarCollapsed = false;
    QListWidget* deviceListWidget;
    QSlider* zoomSlider = nullptr;
    QTimer* relayoutTimer;

    ViewportAwareBehavior* viewportAwareBehavior;
};
