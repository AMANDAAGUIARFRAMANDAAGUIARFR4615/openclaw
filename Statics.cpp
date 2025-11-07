#include "AppListWidget.h"
#include "Recorder.h"

QMap<DeviceConnection*, AppListWidget*> AppListWidget::instanceMap;
QMap<DeviceConnection*, Recorder*> Recorder::instanceMap;
