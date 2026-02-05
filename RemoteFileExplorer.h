#pragma once

#include "DeviceConnection.h"
#include <QDialog>
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

class RemoteFileExplorer : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteFileExplorer(DeviceConnection* connection, const QString& openPath, QWidget* parent);
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
};
