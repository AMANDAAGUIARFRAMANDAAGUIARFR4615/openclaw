#include "DeviceInfo.h"
#include "AppListWidget.h"
#include "Recorder.h"
#include "RemoteFileExplorer.h"

QList<DeviceInfo*> DeviceInfo::allDevices;
QMap<DeviceConnection*, AppListWidget*> AppListWidget::instanceMap;
QMap<DeviceConnection*, Recorder*> Recorder::instanceMap;
QMap<QString, RemoteFileExplorer*> RemoteFileExplorer::instanceMap;
