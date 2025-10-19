#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

class AppListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppListWidget(QWidget *parent = nullptr);

    // 添加一行应用
    void addApp(const QString &iconPath, const QString &appName, const QString &packageName);

signals:
    // 发射操作信号
    void openApp(const QString &packageName);
    void uninstallApp(const QString &packageName);
    void showDetail(const QString &packageName);

private slots:
    void handleOpen();
    void handleUninstall();
    void handleDetail();

private:
    QTableWidget *table;
    void setupTable();
};
