#pragma once

#include "DeviceConnection.h"
#include "DeviceView.h"
#include <QWidget>
#include <QNetworkAccessManager>
#include <QTreeView>
#include <QStandardItemModel>
#include <QHash>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStatusBar>
#include <QListWidget>
#include <QTableWidget>

class VirtualItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    VirtualItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();

        QStyleOptionViewItem modifiedOption = option;

        bool isHidden = index.data(Qt::DisplayRole).toString().startsWith('.');

        if (isHidden) {
            modifiedOption.palette.setColor(QPalette::Text, QColor(150, 150, 150));
            painter->setOpacity(0.5);
        }

        QStyledItemDelegate::paint(painter, modifiedOption, index);

        painter->restore();
    }
};

class RemoteFileExplorer : public QWidget
{
    Q_OBJECT

public:
    static RemoteFileExplorer* open(DeviceConnection* connection, const QString& openPath, DeviceView* deviceView) {
        QString key = QString("%1:%2").arg(reinterpret_cast<quintptr>(connection), 0, 16).arg(openPath);
        auto existing = instanceMap.value(key);
        if (existing) {
            existing->setWindowState(existing->windowState() & ~Qt::WindowMinimized);
            existing->raise();
            existing->activateWindow();
            return existing;
        }

        auto explorer = new RemoteFileExplorer(connection, openPath, deviceView);
        explorer->setWindowTitle(connection->displayName() + " - 文件管理");
        QSize screenSize = qApp->primaryScreen()->availableSize();
        explorer->resize(screenSize.width() * 0.7, screenSize.height() * 0.7);
        explorer->show();
        return explorer;
    }

private:
    explicit RemoteFileExplorer(DeviceConnection* connection, const QString& openPath, DeviceView* deviceView);
    ~RemoteFileExplorer();

protected:
    void setStatusMessage(const QString &message);
    void fetchDirectoryContents(const QString &path);
    void addItemToTreeView(const QString& fullPath, const QString& type, const QString& date, qint64 size, const QString& symbolicLink = "");
    void removeItemPaths(QStandardItem* item);
    void updateDirectoryView(const QString &path, const QJsonArray &list);
    void onDirectoryExpanded(const QModelIndex &index);
    QString getLocalPath(const QString& remotePath);
    void startFileTransfer(int type, const QString &localPath, const QString &remotePath);
    void keyPressEvent(QKeyEvent *event) override;
    void showTreeContextMenu(const QPoint &pos);
    void showTableContextMenu(const QPoint &pos);
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    DeviceConnection* const connection;
    DeviceView* const deviceView;
    const QString openPath;
    QString rootPath;
    
    QTreeView *treeView;
    QStandardItemModel *model;
    QHash<QString, QStandardItem*> pathToItem;
    QStatusBar *statusBar;
    QPoint m_dragStartPos;

    QListWidget* quickAccessList = nullptr;
    QStringList favorites;

    void loadFavorites();
    void saveFavorites();
    void refreshQuickAccessList();
    void addToFavorites(const QString& path);
    void removeFromFavorites(const QString& path);

    QTableWidget* transferTable = nullptr;

    inline static QHash<QString, RemoteFileExplorer*> instanceMap;
};
