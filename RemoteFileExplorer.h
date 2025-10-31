#pragma once

#include "DeviceConnection.h"
#include <QWidget>
#include <QNetworkAccessManager>
#include <QTreeView>
#include <QStandardItemModel>
#include <QMap>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStatusBar>
#include <QListWidget>
#include <QSettings>
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
    explicit RemoteFileExplorer(DeviceConnection* connection, const QString& rootPath = "/", QWidget *parent = nullptr);
    ~RemoteFileExplorer();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void setStatusMessage(const QString &message);
    void fetchDirectoryContents(const QString &path);
    void fetchDirectoryContents(const QModelIndex &index);
    void addItemToTreeView(QStandardItem* parentItem, const QString& fullPath, const QString& type, const QString& date, int size, const QString& symbolicLink = "");
    void updateDirectoryView(const QString &path, const QJsonArray &list);
    void onDirectoryExpanded(const QModelIndex &index);
    void startFileTransfer(int type, const QString &localPath, const QString &remotePath, int size);
    void keyPressEvent(QKeyEvent *event) override;
    void showTreeContextMenu(const QPoint &pos);
    void showTableContextMenu(const QPoint &pos);
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    DeviceConnection* connection;
    QTreeView *treeView;
    QStandardItemModel *model;
    QString rootPath;
    QHash<QString, QStandardItem*> pathToItem;
    QStatusBar *statusBar;
    QPoint m_dragStartPos;

    QListWidget* quickAccessList = nullptr;
    QSettings* settings;
    QStringList favorites;

    void loadFavorites();
    void saveFavorites();
    void refreshQuickAccessList();
    void addToFavorites(const QString& path);
    void removeFromFavorites(const QString& path);

    QTableWidget* transferTable = nullptr;
};
