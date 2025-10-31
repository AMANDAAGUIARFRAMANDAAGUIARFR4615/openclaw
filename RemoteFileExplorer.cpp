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
    layout->insertWidget(1, addQuickAccessButton); // 插入到快速访问列表上方
    connect(addQuickAccessButton, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString path = QInputDialog::getText(this, "添加快速访问路径", "请输入路径:", QLineEdit::Normal, "", &ok);
        if (ok && !path.isEmpty()) {
            addToFavorites(path);
        }
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

    EventHub::StartListening("extractArchiveStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("解压失败: " + data["msg"].toString());
            return;
        }
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

void RemoteFileExplorer::fetchDirectoryContents(const QModelIndex &index)
{
    bool isDir = index.data(Qt::UserRole + 2).toBool();
    QString targetPath = (isDir ? index : index.parent()).data(Qt::UserRole).toString();
    fetchDirectoryContents(targetPath);
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

    for (int i = 0; i < parentItem->rowCount(); i++) {
        auto childItem = parentItem->child(i);
        if (!childItem)
            continue;

        auto childPath = path + "/" + childItem->text();
        pathToItem.remove(childPath);
    }

    parentItem->removeRows(0, parentItem->rowCount());

    if (list.count() == 0) {
        setStatusMessage("目录为空: " + path);
        return;
    }

    for (const auto &value : list) {
        auto obj = value.toObject();
        auto name = obj["name"].toString();
        auto type = obj["type"].toString();

        auto isDirectory = type == "NSFileTypeDirectory" || type == "NSFileTypeSymbolicLink";

        auto symbolicLink = obj["symbolicLink"].toString();
        auto fullPath = path + '/' + name;
        if (path.endsWith('/')) fullPath = path + name;

        auto date = isDirectory ? "" : obj["date"].toString();
        auto size = isDirectory ? -1 : obj["size"].toInteger();

        addItemToTreeView(fullPath, type, date, size, symbolicLink);
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
        model->setData(model->index(row, 1), date);
        model->setData(model->index(row, 2), Tools::formatByteSize(size));
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
    auto sizeItem = new QStandardItem(Tools::formatByteSize(size));
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    parentItem->appendRow({item, dateItem, sizeItem});
}

void RemoteFileExplorer::onDirectoryExpanded(const QModelIndex &index)
{
    QString path = index.data(Qt::UserRole).toString();
    setStatusMessage("展开目录: " + path);
    fetchDirectoryContents(path);
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

        if (transferred == total) {
            transferTable->item(row, 1)->setText(type == 1 ? "接收完成" : "发送完成");
        }
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
    QMenu contextMenu(this);

    QModelIndexList selectedIndexes = treeView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        qDebugEx() << "没有选中任何项";
        return;
    }

    QStringList paths;

    for (const QModelIndex &index : selectedIndexes) {
        if (index.column() == 0)
        {
            QString targetPath = index.data(Qt::UserRole).toString();
            paths.append(targetPath);
        }
    }

    qDebugEx() << paths;

    int selectedCount = paths.count();

    QModelIndex index = treeView->selectionModel()->currentIndex();

    auto targetPath = index.data(Qt::UserRole).toString();
    if (targetPath == "")
    {
        new ToastWidget("路径错误", this);
        return;
    }

    bool isDir = index.data(Qt::UserRole + 2).toBool();

    if (isDir)
    {
        QAction *favoriteAction = nullptr;
        if (favorites.contains(targetPath)) {
            favoriteAction = new QAction("从快速访问移除", &contextMenu);
            connect(favoriteAction, &QAction::triggered, this, [=]() {
                removeFromFavorites(targetPath);
            });
        } else {
            favoriteAction = new QAction("添加到快速访问", &contextMenu);
            connect(favoriteAction, &QAction::triggered, this, [=]() {
                addToFavorites(targetPath);
            });
        }
        contextMenu.addAction(favoriteAction);

        QAction *compressAction = new QAction("压缩", &contextMenu);
        contextMenu.addAction(compressAction);
        connect(compressAction, &QAction::triggered, this, [=]() {
            connection->send("compressArchive", targetPath);
        });

        compressAction->setEnabled(selectedCount == 1);

        QAction *createAction = new QAction("新建文件夹", &contextMenu);
        contextMenu.addAction(createAction);
        connect(createAction, &QAction::triggered, this, [=]() {
            bool ok;
            auto name = QInputDialog::getText(this, "新建文件夹", "请输入名称:", QLineEdit::Normal, "", &ok);
            
            if (!ok || name.isEmpty())
                return;

            setStatusMessage("新建文件夹: " + name);

            connection->send("createDirectory", targetPath + "/" + name);
        });
        createAction->setEnabled(selectedCount == 1);
    }
    else
    {
        auto localPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + connection->deviceInfo->uniqueName() + "/" + QFileInfo(targetPath).fileName();

        QString dirPath = QFileInfo(localPath).absolutePath();

        QDir dir;
        if (!dir.exists(dirPath))
            dir.mkpath(dirPath);

        QAction *viewAction = new QAction("查看", &contextMenu);
        contextMenu.addAction(viewAction);
        connect(viewAction, &QAction::triggered, this, [=]() {
            if (!QFile::exists(localPath))
            {
                new ToastWidget("文件不存在，请先下载", this);
                return;
            }

            new FileViewer(localPath, this);
        });
        viewAction->setEnabled(selectedCount == 1);

        QAction *downloadAction = new QAction("下载", &contextMenu);
        contextMenu.addAction(downloadAction);
        connect(downloadAction, &QAction::triggered, this, [=]() {
            qDebugEx() << localPath << "<=" << targetPath;

            startFileTransfer(1, localPath, targetPath, 0);
        });
        downloadAction->setEnabled(selectedCount == 1);

        if (targetPath.endsWith(".zip") || targetPath.endsWith(".rar")) {
            QAction *extractAction = new QAction("解压", &contextMenu);
            contextMenu.addAction(extractAction);
            connect(extractAction, &QAction::triggered, this, [=]() {
                setStatusMessage("解压: " + targetPath);

                connection->send("extractArchive", targetPath);
            });
            extractAction->setEnabled(selectedCount == 1);
        }
    }

    QAction *renameAction = new QAction("重命名", &contextMenu);
    contextMenu.addAction(renameAction);
    connect(renameAction, &QAction::triggered, this, [=]() {
        bool ok;
        auto name = QInputDialog::getText(this, "重命名", "请输入名称:", QLineEdit::Normal, "", &ok);
        
        if (!ok || name.isEmpty())
            return;

        setStatusMessage("重命名: " + name);

        QJsonObject dataObject;
        dataObject["atPath"] = targetPath;
        dataObject["toPath"] = name;

        connection->send("renameItem", dataObject);
        fetchDirectoryContents(index.parent());
    });
    renameAction->setEnabled(selectedCount == 1);

    QAction *deleteAction = new QAction("删除", &contextMenu);
    contextMenu.addAction(deleteAction);
    connect(deleteAction, &QAction::triggered, this, [=]() {
        auto description = paths.count() > 1 ? QString("%1项").arg(paths.count()) : QFileInfo(targetPath).fileName();
        auto reply = QMessageBox::question(this, "确认删除", "你确定要删除【" + description + "】吗？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        setStatusMessage("删除: " + paths.join(", "));
        
        for (const QString& path : paths)
        {
            connection->send("removeItem", path);
            fetchDirectoryContents(index.parent());
        }
    });

    QAction *copyPathAction = new QAction("复制路径", &contextMenu);
    contextMenu.addAction(copyPathAction);
    connect(copyPathAction, &QAction::triggered, this, [=]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(targetPath);
    });

    contextMenu.exec(treeView->viewport()->mapToGlobal(pos));
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

    QAction *openFolderAction = menu.addAction("打开所在文件夹");
    connect(openFolderAction, &QAction::triggered, [=]() {
#if defined(Q_OS_WIN)
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(localPath);
        QProcess::startDetached("explorer.exe", args);
#elif defined(Q_OS_MAC)
        QString escapedFilePath = localPath;
        escapedFilePath.replace(" ", "\\ ");  // Escape spaces in path
        
        QStringList scriptArgs;
        scriptArgs << "-e"
                   << QString("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(escapedFilePath);
        QProcess::execute("/usr/bin/osascript", scriptArgs);
        QProcess::execute("/usr/bin/osascript", QStringList() << "-e" << "tell application \"Finder\" to activate");
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
#endif
    });

    QAction *copyLocalPathAction = menu.addAction("复制本地路径");
    connect(copyLocalPathAction, &QAction::triggered, [=]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(localPath);
    });

    QAction *copyRemotePathAction = menu.addAction("复制远程路径");
    connect(copyRemotePathAction, &QAction::triggered, [=]() {
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
