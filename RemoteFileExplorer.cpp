#include "RemoteFileExplorer.h"
#include "Logger.h"
#include "EventHub.h"
#include "TcpServer.h"
#include "Tools.h"
#include "FileTransfer.h"
#include "ToastWidget.h"
#include "FileViewer.h"
#include <QVBoxLayout>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QMessageBox>
#include <QStyle>
#include <QApplication>
#include <QFileIconProvider>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>
#include <QInputDialog>
#include <QHeaderView>
#include <QDrag>
#include <QTextEdit>
#include <QClipboard>
#include <QPushButton>
#include <QDesktopServices>

RemoteFileExplorer::RemoteFileExplorer(DeviceConnection* connection, const QString& rootPath, QWidget *parent) 
    : connection(connection), rootPath(rootPath), QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);

    // 初始化收藏功能
    settings = new QSettings("MyApp", "RemoteFileExplorer", this);

    QLabel* quickAccessLabel = new QLabel("⭐ 快速访问", this);
    quickAccessLabel->setStyleSheet("font-weight: bold; padding: 4px;");

    quickAccessList = new QListWidget(this);
    quickAccessList->setFixedHeight(120);
    quickAccessList->setSelectionMode(QAbstractItemView::SingleSelection);
    quickAccessList->setDragDropMode(QAbstractItemView::InternalMove);

    // 当顺序改变时，更新 favorites
    connect(quickAccessList->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &, int, int, const QModelIndex &, int){
        QStringList newOrder;
        for (int i = 1; i < quickAccessList->count(); i++) {
            newOrder.append(quickAccessList->item(i)->text());
        }
        favorites = newOrder;
        saveFavorites();
    });

    treeView = new QTreeView(this);
    QFont font = treeView->font();
    font.setPointSize(16);
    treeView->setFont(font);
    treeView->setIconSize(QSize(24, 24));

    treeView->viewport()->installEventFilter(this);
    treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(quickAccessLabel);
    layout->addWidget(quickAccessList);
    layout->addWidget(treeView);

    QPushButton* addQuickAccessButton = new QPushButton("添加路径", this);
    layout->insertWidget(1, addQuickAccessButton);
    connect(addQuickAccessButton, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString path = QInputDialog::getText(this, "添加快速访问路径", "请输入路径:", QLineEdit::Normal, "", &ok);
        if (!ok)
            return;

        while (path.endsWith('/'))
            path.chop(1);
        
        if (!path.isEmpty())
            addToFavorites(path);
    });

    setLayout(layout);

    model = new QStandardItemModel();
    model->setHorizontalHeaderLabels({"名称", "修改时间", "大小"});
    treeView->setModel(model);

    treeView->setItemDelegate(new VirtualItemDelegate(treeView));

    connect(treeView, &QTreeView::expanded, this, &RemoteFileExplorer::onDirectoryExpanded);

    fetchDirectoryContents(rootPath);

    EventHub::StartListening("fileList", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto path = data["path"].toString();
        auto list = data["list"].toArray();
        updateDirectoryView(path, list);
    });

    EventHub::StartListening("transferStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("传输失败: " + data["msg"].toString());
            return;
        }

        auto result = data["result"];
        auto fullPath = result["path"].toString();
        auto date = result["date"].toString();
        auto size = result["size"].toInteger();

        addItemToTreeView(fullPath, "NSFileTypeRegular", date, size);
    });

    EventHub::StartListening("compressArchiveStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("压缩失败: " + data["msg"].toString());
            return;
        }

        auto result = data["result"];
        addItemToTreeView(result["path"].toString(), "NSFileTypeRegular", result["date"].toString(), result["size"].toInteger());
    });

    EventHub::StartListening("extractArchiveStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("解压失败: " + data["msg"].toString());
            return;
        }

        auto result = data["result"];
        addItemToTreeView(result["path"].toString(), "NSFileTypeDirectory", result["date"].toString(), -1);
    });

    EventHub::StartListening("createDirectoryStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("新建文件夹失败: " + data["msg"].toString());
            return;
        }

        addItemToTreeView(data["path"].toString(), "NSFileTypeDirectory", "", -1);
    });

    EventHub::StartListening("renameItemStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("重命名失败: " + data["msg"].toString());
            return;
        }

        auto atPath = data["atPath"].toString();
        auto name = data["toPath"].toString();
        auto toPath = atPath.left(atPath.lastIndexOf('/') + 1) + name;
        auto item = pathToItem[atPath];

        auto index = item->index();
        auto date = model->index(index.row(), 1, index.parent()).data().toString();
        auto size = model->index(index.row(), 2, index.parent()).data().toString();

        bool isDir = index.data(Qt::UserRole + 2).toBool();

        removeItemPaths(item);
        addItemToTreeView(toPath, isDir ? "NSFileTypeDirectory" : "NSFileTypeRegular", date, Tools::parseByteSize(size));
    });

    EventHub::StartListening("removeItemStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("删除失败: " + data["msg"].toString());
            return;
        }

        auto path = data["path"].toString();
        removeItemPaths(pathToItem[path]);
    });

    treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    treeView->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    treeView->header()->setStretchLastSection(false);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeView, &QTreeView::customContextMenuRequested, this, &RemoteFileExplorer::showTreeContextMenu);

    loadFavorites();
    refreshQuickAccessList();

    connect(quickAccessList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString path = item->text();
        setStatusMessage("打开收藏路径: " + path);
        this->rootPath = path;
        model->removeRows(0, model->rowCount());
        fetchDirectoryContents(path);
    });

    QLabel* transferLabel = new QLabel("📂 文件传输列表", this);
    transferLabel->setStyleSheet("font-weight: bold; padding: 4px;");

    transferTable = new QTableWidget(this);
    transferTable->setColumnCount(8);
    transferTable->setHorizontalHeaderLabels({ "名称", "状态", "进度", "大小", "本地路径", "远程路径", "平均速度", "用时" });
    transferTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    transferTable->setFixedHeight(180);
    transferTable->setAlternatingRowColors(true);
    transferTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(transferTable, &QTableWidget::customContextMenuRequested, this, &RemoteFileExplorer::showTableContextMenu);

    for (int i = 0; i < transferTable->columnCount(); i++) {
        if (i == 0 || i == 4 || i == 5)
            transferTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
        else
            transferTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    layout->addWidget(transferLabel);
    layout->addWidget(transferTable);

    statusBar = new QStatusBar(this);
    layout->addWidget(statusBar);

    setStatusMessage("就绪");
}

RemoteFileExplorer::~RemoteFileExplorer()
{
    
}

bool RemoteFileExplorer::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == treeView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton)
                m_dragStartPos = me->pos();
        }
        else if (event->type() == QEvent::MouseMove) {
            auto me = static_cast<QMouseEvent*>(event);
            if (!(me->buttons() & Qt::LeftButton)) return false;
            if ((me->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) return false;

            QModelIndex index = treeView->indexAt(m_dragStartPos);
            if (!index.isValid()) return false;

            QString path = index.data(Qt::UserRole).toString();
            if (path.isEmpty()) return false;

            auto drag = new QDrag(treeView);
            auto mime = new QMimeData();
      
            QString tempPath = QDir::temp().filePath(QFileInfo(path).fileName());

            QFile tempFile(tempPath);
            if (!tempFile.exists()) {
                if (tempFile.open(QIODevice::WriteOnly)) {
                    tempFile.write("");  // 创建一个空文件占位
                    tempFile.close();
                } else {
                    qWarning() << "无法创建临时文件：" << tempPath;
                    return false;
                }
            }

            mime->setUrls({ QUrl::fromLocalFile(tempPath) });

            drag->setMimeData(mime);
            auto result = drag->exec(Qt::CopyAction);

            if (result == Qt::IgnoreAction) {
                qDebug() << "拖拽取消，删除占位文件";
            } else if (result == Qt::CopyAction) {
                qDebug() << "拖拽完成，开始下载远程文件";
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void RemoteFileExplorer::setStatusMessage(const QString &message)
{
    auto timestamp = QTime::currentTime().toString("HH:mm:ss");
    statusBar->showMessage("[" + timestamp + "] " + message);
}

void RemoteFileExplorer::fetchDirectoryContents(const QString &path)
{
    qDebugEx() << "fetchDirectoryContents" << path;

    connection->send("fileList", path);
}

void RemoteFileExplorer::removeItemPaths(QStandardItem* item) {
    for (int i = item->rowCount() - 1; i >= 0; i--) {
        auto childItem = item->child(i);
        if (!childItem)
            continue;

        removeItemPaths(childItem);
    }

    QString fullPath = item->data(Qt::UserRole).toString();

    pathToItem.remove(fullPath);

    if (item->parent())
        item->parent()->removeRow(item->row());
    else
        model->removeRow(item->row());
}

void RemoteFileExplorer::updateDirectoryView(const QString &path, const QJsonArray &list)
{
    if (path == rootPath) {
        pathToItem.clear();
        pathToItem[rootPath] = model->invisibleRootItem();
    }

    QStandardItem* parentItem = pathToItem.value(path);

    if (!parentItem) {
        setStatusMessage("目录加载失败: " + path);
        return;
    }

    qDebugEx() << "updateDirectoryView" << path << parentItem->rowCount();

    QSet<QString> currentPaths;
    for (const auto &value : list) {
        auto obj = value.toObject();
        auto name = obj["name"].toString();
        currentPaths.insert(name);
    }

    qDebugEx() << "currentPaths" << currentPaths;

    for (int i = parentItem->rowCount() - 1; i >= 0; i--) {
        auto childItem = parentItem->child(i);
        if (!childItem) {
            parentItem->removeRow(i);
            continue;
        }

        auto childName = childItem->text();
        if (!currentPaths.contains(childName))
            removeItemPaths(childItem);
    }

    if (list.count() == 0) {
        setStatusMessage("目录为空: " + path);
        return;
    }

    for (const auto &value : list) {
        auto obj = value.toObject();
        auto name = obj["name"].toString();
        auto type = obj["type"].toString();

        auto symbolicLink = obj["symbolicLink"].toString();
        auto fullPath = path + '/' + name;
        if (path.endsWith('/')) fullPath = path + name;

        addItemToTreeView(fullPath, type, obj["date"].toString(), obj["size"].toInteger(), symbolicLink);
    }
}

void RemoteFileExplorer::addItemToTreeView(const QString& fullPath, const QString& type, const QString& date, int size, const QString& symbolicLink)
{
    auto parentPath = fullPath.left(fullPath.lastIndexOf('/'));
    auto parentItem = pathToItem[parentPath.isEmpty() ? "/" : parentPath];
    
    if (!parentItem) {
        qCriticalEx() << parentPath << "不存在";
        return;
    }

    auto name = QFileInfo(fullPath).fileName();
    auto& item = pathToItem[fullPath];

    if (item) {
        int row = item->row();
        auto parent = item->index().parent();
        model->setData(model->index(row, 1, parent), date);
        model->setData(model->index(row, 2, parent), Tools::formatByteSize(size));
        return;
    }
    
    static QFileIconProvider iconProvider;

    auto isDirectory = type == "NSFileTypeDirectory" || type == "NSFileTypeSymbolicLink";

    item = new QStandardItem(name);
    item->setData(fullPath, Qt::UserRole);
    pathToItem[fullPath] = item;

    if (isDirectory) {
        item->setIcon(QIcon(symbolicLink.isEmpty() ? ":/icons/folder.png" : ":/icons/folder_link.png"));
    } else {
        auto suffix = name.section('.', -1).toLower();
        auto iconPath = ":/icons/" + suffix + ".png";
        QIcon fileIcon;

        if (QFile::exists(iconPath)) {
            fileIcon = QIcon(iconPath);
        } else {
            if (symbolicLink.isEmpty())
                fileIcon = iconProvider.icon(QFileInfo(name));
            else
                fileIcon = QIcon(":/icons/file_link.png");
        }

        item->setIcon(fileIcon);
    }

    item->setEditable(false);

    item->setData(isDirectory, Qt::UserRole + 2);

    if (isDirectory) item->setChild(0, nullptr);

    auto dateItem = new QStandardItem(date);
    auto sizeItem = new QStandardItem(Tools::formatByteSize(isDirectory ? -1 : size));
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    parentItem->appendRow({item, dateItem, sizeItem});
}

void RemoteFileExplorer::onDirectoryExpanded(const QModelIndex &index)
{
    QString path = index.data(Qt::UserRole).toString();
    setStatusMessage("展开目录: " + path);
    fetchDirectoryContents(path);
}

QString RemoteFileExplorer::getLocalPath(const QString& remotePath) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + connection->deviceInfo->uniqueName() + "/" + QFileInfo(remotePath).fileName();
}

void RemoteFileExplorer::startFileTransfer(int type, const QString &localPath, const QString &remotePath, int size)
{
    auto transfer = new FileTransfer(connection, type, localPath, size);

    int row = transferTable->rowCount();
    transferTable->insertRow(row);

    QStringList texts = {
        QFileInfo(localPath).fileName(),
        type == 1 ? "接收中" : "发送中",
        "0%",
        "",
        localPath,
        remotePath,
        "0 B/s",
        "0 s"
    };

    for (int col = 0; col < texts.size(); ++col) {
        auto item = new QTableWidgetItem(texts[col]);
        item->setTextAlignment(Qt::AlignCenter);

        if (col == 0 || col == 4 || col == 5)
            item->setToolTip(texts[col]);

        transferTable->setItem(row, col, item);
    }

    connect(transfer, &FileTransfer::progressUpdated, this, [=](quint64 transferred, quint64 total) {
        double percent = (double)transferred / total * 100;
        transferTable->item(row, 2)->setText(QString::number(percent, 'f', 1) + "%");
        transferTable->item(row, 3)->setText(QString("%1/%2").arg(Tools::formatByteSize(transferred)).arg(Tools::formatByteSize(total)));

        double elapsed = transfer->elapsedTime();
        transferTable->item(row, 6)->setText(Tools::formatByteSize(transferred / elapsed) + "/s");
        transferTable->item(row, 7)->setText(QString::number(elapsed, 'f', 2) + " s");

        if (transferred == total)
            transferTable->item(row, 1)->setText(type == 1 ? "接收完成" : "发送完成");
    });

    QJsonObject dataObject;
    dataObject["id"] = transfer->id;
    dataObject["type"] = type;
    dataObject["port"] = transfer->serverPort();
    dataObject["path"] = remotePath;

    if (type == 2)
        dataObject["size"] = size;

    connection->send("transferFile", dataObject);
}

void RemoteFileExplorer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        auto item = quickAccessList->currentItem();
        if (item) {
            if (item->text() == "/") {
                new ToastWidget("根目录不能删除");
                return;
            }

            removeFromFavorites(item->text());
            return;
        }
    }
    
    QWidget::keyPressEvent(event);
}

void RemoteFileExplorer::showTreeContextMenu(const QPoint &pos)
{
    QMenu menu(this);

    QModelIndex index = treeView->indexAt(pos);
    if (index.column() != 0)
        index = index.sibling(index.row(), 0);

    auto remotePath = index.isValid() ? index.data(Qt::UserRole).toString() : rootPath;
    bool isDir = index.isValid() ? index.data(Qt::UserRole + 2).toBool() : true;

    QModelIndexList selectedIndexes = treeView->selectionModel()->selectedIndexes();

    QStringList paths;

    for (const QModelIndex &index : selectedIndexes) {
        if (index.column() == 0)
            paths.append(index.data(Qt::UserRole).toString());
    }

    qDebugEx() << paths;

    int selectedCount = paths.count();

    if (selectedCount > 0) {
        auto localPath = getLocalPath(remotePath);

        if (isDir)
        {
            if (favorites.contains(remotePath)) {
                connect(menu.addAction("从快速访问移除"), &QAction::triggered, this, [=]() {
                    removeFromFavorites(remotePath);
                });
            } else {
                connect(menu.addAction("添加到快速访问"), &QAction::triggered, this, [=]() {
                    addToFavorites(remotePath);
                });
            }

            connect(menu.addAction("压缩"), &QAction::triggered, this, [=]() {
                for (const QString& remotePath : paths) {
                    connection->send("compressArchive", remotePath);
                }
            });
        }
        else
        {
            QAction *viewAction = menu.addAction("查看");
            connect(viewAction, &QAction::triggered, this, [=]() {
                if (!QFile::exists(localPath))
                {
                    new ToastWidget("文件不存在，请先下载", this);
                    return;
                }

                new FileViewer(localPath, this);
            });
            viewAction->setEnabled(selectedCount == 1);

            connect(menu.addAction("下载"), &QAction::triggered, this, [=]() {
                for (const QString& remotePath : paths) {
                    auto localPath = getLocalPath(remotePath);
                    startFileTransfer(1, localPath, remotePath, 0);
                }
            });

            connect(menu.addAction("解压"), &QAction::triggered, this, [=]() {
                for (const QString& remotePath : paths) {
                    if (remotePath.endsWith(".zip") || remotePath.endsWith(".rar"))
                        connection->send("extractArchive", remotePath);
                }
            });
        }

        QAction *renameAction = menu.addAction("重命名");
        connect(renameAction, &QAction::triggered, this, [=]() {
            bool ok;
            auto name = QInputDialog::getText(this, "重命名", "请输入名称:", QLineEdit::Normal, "", &ok);
            
            if (!ok || name.isEmpty())
                return;

            setStatusMessage("重命名: " + name);

            QJsonObject dataObject;
            dataObject["atPath"] = remotePath;
            dataObject["toPath"] = name;

            connection->send("renameItem", dataObject);
        });
        renameAction->setEnabled(selectedCount == 1);

        connect(menu.addAction("删除"), &QAction::triggered, this, [=]() {
            auto description = paths.count() > 1 ? QString("%1项").arg(paths.count()) : QFileInfo(remotePath).fileName();
            auto reply = QMessageBox::question(this, "确认删除", "你确定要删除【" + description + "】吗？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

            if (reply != QMessageBox::Yes)
                return;

            qDebugEx() << "删除: " + paths.join(", ");
            
            for (const QString& remotePath : paths) {
                connection->send("removeItem", remotePath);
            }
        });

        connect(menu.addAction("在文件资源管理器中显示"), &QAction::triggered, [=]() {
            Tools::showInFileExplorer(localPath);
        });

        connect(menu.addAction("复制本地路径"), &QAction::triggered, this, [=]() {
            QStringList list;
            for (const QString& remotePath : paths) {
                list << getLocalPath(remotePath);
            }

            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(list.join("\n"));
        });

        connect(menu.addAction("复制远程路径"), &QAction::triggered, this, [=]() {
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(paths.join("\n"));
        });

        connect(menu.addAction("复制文件"), &QAction::triggered, this, [=]() {
            QMimeData *mimeData = new QMimeData();
            QList<QUrl> urlList;
            QList<QString> pendingDownloadPaths;
            for (const QString& remotePath : paths) {
                auto localPath = getLocalPath(remotePath);
                if (!QFileInfo::exists(localPath)) 
                    pendingDownloadPaths << remotePath;

                urlList << QUrl::fromLocalFile(localPath);
            }
            mimeData->setUrls(urlList);
  
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setMimeData(mimeData, QClipboard::Clipboard);

            if (pendingDownloadPaths.count() == 0)
                return;

            auto reply = QMessageBox::question(this, "下载提示", QString("有%1个文件还未下载不能复制，是否下载？").arg(pendingDownloadPaths.count()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

            if (reply != QMessageBox::Yes)
                return;

            for (const QString &remotePath : pendingDownloadPaths) {
                auto localPath = getLocalPath(remotePath);
                startFileTransfer(1, localPath, remotePath, 0);
            }
        });
    }

    QAction *newFolderAction = menu.addAction("新建文件夹");
    connect(newFolderAction, &QAction::triggered, [=]() {
        bool ok;
        auto name = QInputDialog::getText(this, "新建文件夹", "请输入名称:", QLineEdit::Normal, "", &ok);
        
        if (!ok || name.isEmpty())
            return;

        setStatusMessage("新建文件夹: " + name);

        auto dir = isDir ? remotePath : remotePath.left(remotePath.lastIndexOf('/'));
        connection->send("createDirectory", dir + "/" + name);
    });
    newFolderAction->setEnabled(selectedCount == 1 || (selectedCount == 0 && rootPath != "/"));

    menu.exec(treeView->viewport()->mapToGlobal(pos));
}

void RemoteFileExplorer::showTableContextMenu(const QPoint &pos)
{
    QModelIndex index = transferTable->indexAt(pos);
    if (!index.isValid())
        return;

    QString localPath = transferTable->item(index.row(), 4)->text();
    QString remotePath = transferTable->item(index.row(), 5)->text();

    QMenu menu(this);

    QAction *viewAction = menu.addAction("查看");
    connect(viewAction, &QAction::triggered, [=]() {
        new FileViewer(localPath, this);
    });

    connect(menu.addAction("在文件资源管理器中显示"), &QAction::triggered, [=]() {
        Tools::showInFileExplorer(localPath);
    });

    connect(menu.addAction("复制本地路径"), &QAction::triggered, [=]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(localPath);
    });

    connect(menu.addAction("复制远程路径"), &QAction::triggered, [=]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(remotePath);
    });

    menu.exec(transferTable->viewport()->mapToGlobal(pos));
}

void RemoteFileExplorer::dragEnterEvent(QDragEnterEvent *event)
{
    qDebugEx() << "dragEnterEvent";

    if (!event->mimeData()->hasUrls()) 
        event->ignore();

    auto urls = event->mimeData()->urls();

    for (const QUrl &url : urls) {
        if (QFileInfo(url.toLocalFile()).isDir()) {
            event->ignore();
            return;
        }
    }
    
    event->accept();
}

void RemoteFileExplorer::dragMoveEvent(QDragMoveEvent *event)
{
    QPoint globalPos = mapToGlobal(event->pos());
    QPoint localPos = treeView->viewport()->mapFromGlobal(globalPos);
    
    treeView->selectionModel()->clearSelection();

    QModelIndex index = treeView->indexAt(localPos);
    if (index.isValid())
        treeView->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    event->accept();
}

void RemoteFileExplorer::dropEvent(QDropEvent *event)
{
    auto urls = event->mimeData()->urls();

    QPoint globalPos = mapToGlobal(event->pos());
    QPoint localPos = treeView->viewport()->mapFromGlobal(globalPos);
    
    QModelIndex index = treeView->indexAt(localPos);
    QString targetPath = index.data(Qt::UserRole).toString();
    bool isDir = index.data(Qt::UserRole + 2).toBool();

    for (const QUrl &url : urls) {
        auto localPath = url.toLocalFile();
        auto size = Tools::getFileSize(localPath);
        if (size == -1) {
            qCriticalEx() << localPath << "=>" << targetPath;
            continue;
        }

        qDebugEx() << localPath << "=>" << targetPath;

        auto dir = isDir ? targetPath : targetPath.left(targetPath.lastIndexOf('/'));

        startFileTransfer(2, localPath, dir + QString("/") + QFileInfo(localPath).fileName(), size);
    }

    event->accept();
}

void RemoteFileExplorer::loadFavorites() {
    favorites = settings->value("favorites").toStringList();
}

void RemoteFileExplorer::saveFavorites() {
    settings->setValue("favorites", favorites);
}

void RemoteFileExplorer::refreshQuickAccessList() {
    quickAccessList->clear();
    
    auto item = new QListWidgetItem("/");
    quickAccessList->addItem(item);

    for (const QString& path : favorites) {
        auto item = new QListWidgetItem(path);
        quickAccessList->addItem(item);
    }
}

void RemoteFileExplorer::addToFavorites(const QString& path) {
    favorites.append(path);
    saveFavorites();
    refreshQuickAccessList();
    setStatusMessage("已添加到快速访问: " + path);
}

void RemoteFileExplorer::removeFromFavorites(const QString& path) {
    favorites.removeAll(path);
    saveFavorites();
    refreshQuickAccessList();
    setStatusMessage("已从快速访问移除: " + path);
}
