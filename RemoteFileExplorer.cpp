#include "RemoteFileExplorer.h"
#include "Logger.h"
#include "EventHub.h"
#include "Tools.h"
#include "FileTransfer.h"
#include "ToastWidget.h"
#include "FileViewer.h"
#include "MainWindow.h"
#include <QVBoxLayout>
#include <QNetworkReply>
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
#include <QSortFilterProxyModel>
#include <QCollator>

class FileSystemSortProxyModel : public QSortFilterProxyModel
{
    QCollator collator;
    QList<int> sortHistory; // 用于记录用户点击排序的列的历史顺序

public:
    explicit FileSystemSortProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) 
    {
        collator.setNumericMode(true);
        sortHistory.append(0); // 初始化把“名称列”（第0列）放入兜底
    }

    // 重写 sort 方法，每次用户点击表头排序时都会触发
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override
    {
        // 把最新点击的列移到历史记录的最前面 (优先级最高)
        sortHistory.removeAll(column);
        sortHistory.prepend(column); 
        
        QSortFilterProxyModel::sort(column, order);
    }

protected:
    // 单一列的比较逻辑，返回 -1(小于), 0(等于), 1(大于)
    int compareColumn(const QModelIndex &left, const QModelIndex &right, int column) const
    {
        if (column == 0 || column == 2) { // 按名称或类型排序
            return collator.compare(left.data(Qt::DisplayRole).toString(), 
                                    right.data(Qt::DisplayRole).toString());
        } 
        else if (column == 3) { // 按大小排序
            qint64 leftSize = left.data(Qt::UserRole + 1).toLongLong();
            qint64 rightSize = right.data(Qt::UserRole + 1).toLongLong();
            
            if (leftSize < rightSize) return -1;
            if (leftSize > rightSize) return 1;
            return 0;
        }
        
        // === 其他未定义的列，利用 Qt 基类的默认比较 ===
        if (QSortFilterProxyModel::lessThan(left, right)) return -1;
        if (QSortFilterProxyModel::lessThan(right, left)) return 1;
        return 0;
    }

    bool lessThan(const QModelIndex &leftIndex, const QModelIndex &rightIndex) const override
    {
        // 按照用户点击历史的优先级，依次进行比较
        for (int column : sortHistory) {
            // 获取对应历史列的数据节点
            QModelIndex leftSibling = leftIndex.siblingAtColumn(column);
            QModelIndex rightSibling = rightIndex.siblingAtColumn(column);

            int result = compareColumn(leftSibling, rightSibling, column);
            
            // 只要在这一列比较出了大小，直接返回结果
            if (result != 0)
                return result < 0;
        }
        
        return false;
    }
};

RemoteFileExplorer::RemoteFileExplorer(DeviceConnection* connection, const QString& openPath, DeviceView* deviceView) : connection(connection), openPath(openPath), deviceView(deviceView), QWidget()
{
    QString key = QString("%1:%2").arg(reinterpret_cast<quintptr>(connection), 0, 16).arg(openPath);
    instanceMap.insert(key, this);

    rootPath = openPath;
    
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);

    connect(deviceView, &QObject::destroyed, this, &QWidget::close);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0); // 可选：根据需要调整边距

    QLabel* quickAccessLabel = new QLabel("⭐ 快速访问", this);
    quickAccessLabel->setStyleSheet("font-weight: bold; padding: 4px;");
    headerLayout->addWidget(quickAccessLabel);

    QPushButton* addQuickAccessButton = new QPushButton("添加路径", this);
    connect(addQuickAccessButton, &QPushButton::clicked, [this]() {
        bool ok;
        QString path = QInputDialog::getText(this, "添加快速访问路径", "请输入路径:", QLineEdit::Normal, "", &ok);
        if (!ok)
            return;

        while (path.endsWith('/'))
            path.chop(1);
        
        if (!path.isEmpty())
            addToFavorites(path);
    });
    headerLayout->addWidget(addQuickAccessButton);

    headerLayout->addStretch();

    quickAccessList = new QListWidget(this);
    quickAccessList->setFixedHeight(120);
    quickAccessList->setSelectionMode(QAbstractItemView::SingleSelection);
    quickAccessList->setDragDropMode(QAbstractItemView::InternalMove);

    // 当顺序改变时，更新 favorites
    connect(quickAccessList->model(), &QAbstractItemModel::rowsMoved, [this](const QModelIndex &, int, int, const QModelIndex &, int){
        QStringList newOrder;
        for (int i = 1; i < quickAccessList->count(); i++) {
            newOrder.append(quickAccessList->item(i)->text());
        }
        favorites = newOrder;
        saveFavorites();
    });

    treeView = new QTreeView(this);
    treeView->setMinimumHeight(200);
    QFont font = treeView->font();
    font.setPointSize(16);
    treeView->setFont(font);
    treeView->setIconSize(QSize(24, 24));

    treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto layout = new QVBoxLayout(this);
    
    layout->addLayout(headerLayout);
    layout->addWidget(quickAccessList);
    layout->addWidget(treeView);

    setLayout(layout);

    model = new QStandardItemModel();
    model->setHorizontalHeaderLabels({"名称", "修改时间", "类型", "大小"});
    
    auto proxyModel = new FileSystemSortProxyModel();
    proxyModel->setSourceModel(model);
    treeView->setModel(proxyModel);
    treeView->setSortingEnabled(true);
    treeView->sortByColumn(0, Qt::AscendingOrder);

    treeView->setItemDelegate(new VirtualItemDelegate(treeView));

    connect(treeView, &QTreeView::expanded, this, &RemoteFileExplorer::onDirectoryExpanded);

    fetchDirectoryContents(rootPath);

    EventHub::on(this, "fileList", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto path = data["path"].toString();
        auto list = data["list"].toArray();
        updateDirectoryView(path, list);
    });

    EventHub::on(this, "transferStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        if (data["code"].toInt() != 0) {
            setStatusMessage("传输失败: " + data["msg"].toString());
            return;
        }

        const auto& result = data["result"];
        const auto& fullPath = result["path"].toString();
        const auto& date = result["date"].toString();
        const auto& size = result["size"].toInteger();

        addItemToTreeView(fullPath, "NSFileTypeRegular", date, size);
    });

    EventHub::on(this, "compressArchiveStatus", [this](QJsonValue data, DeviceConnection* connection) {
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

    EventHub::on(this, "extractArchiveStatus", [this](QJsonValue data, DeviceConnection* connection) {
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

    EventHub::on(this, "createDirectoryStatus", [this](QJsonValue data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto code = data["code"].toInt();
        if (code != 0) {
            setStatusMessage("新建文件夹失败: " + data["msg"].toString());
            return;
        }

        addItemToTreeView(data["path"].toString(), "NSFileTypeDirectory", "", -1);
    });

    EventHub::on(this, "renameItemStatus", [this](QJsonValue data, DeviceConnection* connection) {
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
        auto size = model->index(index.row(), 3, index.parent()).data().toString();

        bool isDir = index.data(Qt::UserRole + 2).toBool();

        removeItemPaths(item);
        addItemToTreeView(toPath, isDir ? "NSFileTypeDirectory" : "NSFileTypeRegular", date, Tools::parseByteSize(size));
    });

    EventHub::on(this, "removeItemStatus", [this](QJsonValue data, DeviceConnection* connection) {
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
    treeView->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    treeView->header()->setStretchLastSection(false);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeView, &QTreeView::customContextMenuRequested, this, &RemoteFileExplorer::showTreeContextMenu);

    loadFavorites();
    refreshQuickAccessList();

    connect(quickAccessList, &QListWidget::itemClicked, [this](QListWidgetItem* item) {
        QString path = item->text();
        setStatusMessage("打开收藏路径: " + path);
        rootPath = path;
        model->removeRows(0, model->rowCount());
        fetchDirectoryContents(path);
    });

    QLabel* transferLabel = new QLabel("📂 文件传输列表", this);
    transferLabel->setStyleSheet("font-weight: bold; padding: 4px;");

    transferTable = new QTableWidget(this);
    transferTable->setColumnCount(9);
    transferTable->setHorizontalHeaderLabels({ "开始时间", "名称", "状态", "进度", "大小", "本地路径", "远程路径", "平均速度", "用时" });
    transferTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    transferTable->setFixedHeight(180);
    transferTable->setAlternatingRowColors(true);
    transferTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(transferTable, &QTableWidget::customContextMenuRequested, this, &RemoteFileExplorer::showTableContextMenu);

    for (int i = 0; i < transferTable->columnCount(); i++) {
        if (i == 5 || i == 6)
            transferTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
        else
            transferTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    layout->addWidget(transferLabel);
    layout->addWidget(transferTable);

    statusBar = new QStatusBar(this);
    layout->addWidget(statusBar);

    setStatusMessage("就绪");

    QVariantList history = settings->value(connection->deviceInfo->deviceId + "/transferHistory").toList();
    for (const QVariant &v : history) {
        QJsonObject obj = v.toJsonObject();
        int row = 0;
        transferTable->insertRow(row);
        QStringList texts = {
            obj["startTime"].toString(),
            obj["name"].toString(),
            obj["type"].toInt() == 1 ? "接收完成" : "发送完成",
            "100%",
            obj["size"].toString(),
            obj["localPath"].toString(),
            obj["remotePath"].toString(),
            obj["speed"].toString(),
            obj["usedTime"].toString()
        };
        for (int col = 0; col < texts.size(); ++col) {
            auto item = new QTableWidgetItem(texts[col]);
            item->setTextAlignment(Qt::AlignCenter);
            if (col == 1 || col == 5 || col == 6)
                item->setToolTip(texts[col]);
            transferTable->setItem(row, col, item);
        }
    }
}

RemoteFileExplorer::~RemoteFileExplorer()
{
    EventHub::off(this, "fileList");
    EventHub::off(this, "transferStatus");
    EventHub::off(this, "compressArchiveStatus");
    EventHub::off(this, "extractArchiveStatus");
    EventHub::off(this, "createDirectoryStatus");
    EventHub::off(this, "renameItemStatus");
    EventHub::off(this, "removeItemStatus");

    QString key = QString("%1:%2").arg(reinterpret_cast<quintptr>(connection), 0, 16).arg(openPath);
    instanceMap.remove(key);

    delete ((QSortFilterProxyModel*)treeView->model())->sourceModel();
    delete treeView->model();
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

    QSet<QString> currentPaths;
    for (const auto &value : list) {
        auto obj = value.toObject();
        auto name = obj["name"].toString();
        currentPaths.insert(name);
    }

    qDebugEx() << "updateDirectoryView" << path << currentPaths.size();
    // qDebugEx() << "currentPaths" << currentPaths;

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

void RemoteFileExplorer::addItemToTreeView(const QString& fullPath, const QString& type, const QString& date, qint64 size, const QString& symbolicLink)
{
    auto parentPath = fullPath.left(fullPath.lastIndexOf('/'));
    auto parentItem = pathToItem[parentPath.isEmpty() ? "/" : parentPath];
    
    if (!parentItem) {
        qCriticalEx() << parentPath << "不存在";
        new ToastWidget("路径不存在", this);
        return;
    }

    auto name = QFileInfo(fullPath).fileName();
    auto& item = pathToItem[fullPath];
    auto isDirectory = type == "NSFileTypeDirectory" || type == "NSFileTypeSymbolicLink";

    QString typeDescription;
    if (isDirectory)
        typeDescription = "文件夹";
    else
        typeDescription = name.lastIndexOf('.') != -1 ? name.section('.', -1).toUpper() + " 文件" : "文件";

    if (item) {
        item->setData(size, Qt::UserRole + 1);
        
        int row = item->row();
        auto parent = item->index().parent();
        model->setData(model->index(row, 1, parent), date);

        // 更新类型列
        model->setData(model->index(row, 2, parent), typeDescription);

        // 更新大小列
        auto sizeIndex = model->index(row, 3, parent);
        model->setData(sizeIndex, Tools::formatByteSize(size));
        model->itemFromIndex(sizeIndex)->setData(size, Qt::UserRole + 1);
        return;
    }
    
    static QFileIconProvider iconProvider;

    item = new QStandardItem(name);
    item->setData(fullPath, Qt::UserRole);
    item->setData(isDirectory, Qt::UserRole + 2);
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

    if (isDirectory) item->setChild(0, nullptr);

    auto dateItem = new QStandardItem(date);
    auto typeItem = new QStandardItem(typeDescription);
    auto sizeItem = new QStandardItem(Tools::formatByteSize(isDirectory ? -1 : size));
    sizeItem->setData(size, Qt::UserRole + 1);
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    parentItem->appendRow({item, dateItem, typeItem, sizeItem});
}

void RemoteFileExplorer::onDirectoryExpanded(const QModelIndex &index)
{
    QString path = index.data(Qt::UserRole).toString();
    setStatusMessage("展开目录: " + path);
    fetchDirectoryContents(path);
}

QString RemoteFileExplorer::getLocalPath(const QString& remotePath) {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/" + connection->deviceInfo->deviceId + "/" + QFileInfo(remotePath).fileName();
}

void RemoteFileExplorer::startFileTransfer(int type, const QString &localPath, const QString &remotePath)
{
    const auto startTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const auto name = QFileInfo(localPath).fileName();

    if (type == 2 && MainWindow::getInstance()->multiControlSwitchButton->isChecked())
    {
        for(const auto& connection : MainWindow::getInstance()->getDeviceConnections()) {
            if (connection == this->connection)
                continue;

            auto transfer = new FileTransfer(connection, type, localPath, remotePath, this);

            connect(transfer, &FileTransfer::progressUpdated, this, [=](qint64 transferred, qint64 total) {
                if (transferred != total)
                    return;

                float elapsed = transfer->elapsedTime();

                QJsonObject obj;
                obj["startTime"] = startTime;
                obj["name"] = name;
                obj["type"] = type;
                obj["size"] = QString("%1/%2").arg(Tools::formatByteSize(transferred), Tools::formatByteSize(total));
                obj["localPath"] = localPath;
                obj["remotePath"] = remotePath;
                obj["speed"] = Tools::formatByteSize(transferred / elapsed) + "/s";
                obj["usedTime"] = QString::number(elapsed, 'f', 2) + " s";

                QString key = connection->deviceInfo->deviceId + "/transferHistory";
                QVariantList history = settings->value(key).toList();
                history.append(obj);
                settings->setValue(key, history);
            });
        }
    }

    auto transfer = new FileTransfer(connection, type, localPath, remotePath, this);

    int row = 0;
    transferTable->insertRow(row);

    QStringList texts = {
        startTime,
        name,
        "排队中",
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

        if (col == 1 || col == 5 || col == 6)
            item->setToolTip(texts[col]);

        transferTable->setItem(row, col, item);
    }

    const auto& statusItem = transferTable->item(row, 2);
    const auto& percentItem = transferTable->item(row, 3);
    const auto& sizeItem = transferTable->item(row, 4);
    const auto& speedItem = transferTable->item(row, 7);
    const auto& timeItem = transferTable->item(row, 8);

    connect(transfer, &FileTransfer::progressUpdated, transferTable, [=](qint64 transferred, qint64 total) {
        if (transferred < 0) {
            statusItem->setText("传输失败");
            return;
        }

        if (statusItem->text() == "排队中")
            statusItem->setText(type == 1 ? "接收中" : "发送中");

        float percent = transferred * 100.0 / total;
        
        percentItem->setText(QString::number(percent, 'f', 1) + "%");
        sizeItem->setText(QString("%1/%2").arg(Tools::formatByteSize(transferred), Tools::formatByteSize(total)));

        double elapsed = transfer->elapsedTime();
        speedItem->setText(Tools::formatByteSize(transferred / elapsed) + "/s");
        timeItem->setText(QString::number(elapsed, 'f', 2) + " s");

        if (transferred != total)
            return;

        statusItem->setText(type == 1 ? "接收完成" : "发送完成");

        QJsonObject obj;
        obj["startTime"] = startTime;
        obj["name"] = name;
        obj["type"] = type;
        obj["size"] = sizeItem->text();
        obj["localPath"] = localPath;
        obj["remotePath"] = remotePath;
        obj["speed"] = speedItem->text();
        obj["usedTime"] = timeItem->text();

        QString key = connection->deviceInfo->deviceId + "/transferHistory";
        QVariantList history = settings->value(key).toList();
        history.append(obj);
        settings->setValue(key, history);
    });
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
                new ToastWidget("根目录不能删除", this);
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
    QMenu menu;

    QModelIndex index = treeView->indexAt(pos);
    if (index.column() != 0)
        index = index.sibling(index.row(), 0);

    auto remotePath = index.isValid() ? index.data(Qt::UserRole).toString() : rootPath;

    QModelIndexList selectedIndexes = treeView->selectionModel()->selectedIndexes();

    QStringList paths;

    for (const QModelIndex &index : selectedIndexes) {
        if (index.column() == 0)
            paths.append(index.data(Qt::UserRole).toString());
    }

    qDebugEx() << paths;

    bool isDir = !selectedIndexes.empty() && std::ranges::all_of(selectedIndexes, [](const QModelIndex &index){ return (index.column() == 0 ? index : index.sibling(index.row(), 0)).data(Qt::UserRole + 2).toBool(); });

    auto send = [=](const DataGuard::StrObfuscator<>& event, const QJsonValue &jsonValue = QJsonValue()) {
        for (const auto& connection : MainWindow::getInstance()->getDeviceConnections((DeviceView*)parent())) {
            connection->send(event, jsonValue);
        }
    };

    int selectedCount = paths.count();

    auto localPath = getLocalPath(remotePath);

    menu.addAction("查看", [=]() {
        if (!QFile::exists(localPath))
        {
            new ToastWidget("文件不存在，请先下载", this);
            return;
        }

        new FileViewer(localPath, this);
    })->setEnabled(selectedCount == 1 && !isDir);

    menu.addAction("下载", [=]() {
        for (const QString& remotePath : paths) {
            startFileTransfer(1, getLocalPath(remotePath), remotePath);
        }
    })->setEnabled(selectedCount > 0 && !isDir);

    menu.addAction("解压", [=]() {
        for (const QString& remotePath : paths) {
            connection->send("extractArchive", remotePath);
        }
    })->setEnabled(selectedCount > 0 && std::ranges::all_of(paths, [](const QString &path){ return path.endsWith(".zip") || path.endsWith(".rar"); }) && !isDir);

    if (favorites.contains(remotePath)) {
        menu.addAction("从快速访问移除", [=]() {
            removeFromFavorites(remotePath);
        })->setEnabled(selectedCount == 1 && isDir);
    } else {
        menu.addAction("添加到快速访问", [=]() {
            addToFavorites(remotePath);
        })->setEnabled(selectedCount == 1 && isDir);
    }

    menu.addAction("压缩", [=]() {
        for (const QString& remotePath : paths) {
            send("compressArchive", remotePath);
        }
    })->setEnabled(isDir);

    menu.addAction("重命名", [=]() {
        bool ok;
        auto name = QInputDialog::getText(this, "重命名", "请输入名称:", QLineEdit::Normal, remotePath.section('/', -1), &ok);
        
        if (!ok || name.isEmpty())
            return;

        setStatusMessage("重命名: " + name);

        QJsonObject dataObject;
        dataObject["atPath"] = remotePath;
        dataObject["toPath"] = name;

        send("renameItem", dataObject);
    })->setEnabled(selectedCount == 1);

    menu.addAction("删除", [=]() {
        auto description = paths.count() > 1 ? QString("%1项").arg(paths.count()) : QFileInfo(remotePath).fileName();
        auto reply = QMessageBox::question(this, "确认删除", "你确定要删除【" + description + "】吗？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        qDebugEx() << "删除: " + paths.join(", ");
        
        for (const QString& remotePath : paths) {
            send("removeItem", remotePath);
        }
    });

    menu.addAction("在文件资源管理器中显示", [=]() {
        Tools::showInFileExplorer(localPath);
    })->setEnabled(selectedCount == 1);

    menu.addAction("复制本地路径", [=]() {
        QStringList list;
        for (const QString& remotePath : paths) {
            list << getLocalPath(remotePath);
        }

        qApp->clipboard()->setText(list.join("\n"));
    });

    menu.addAction("复制远程路径", [=]() {
        qApp->clipboard()->setText(paths.join("\n"));
    });

    menu.addAction("复制文件", [=]() {
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

        qApp->clipboard()->setMimeData(mimeData, QClipboard::Clipboard);

        if (pendingDownloadPaths.count() == 0)
            return;

        auto reply = QMessageBox::question(this, "下载提示", QString("有%1个文件还未下载不能复制，是否下载？").arg(pendingDownloadPaths.count()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        for (const QString &remotePath : pendingDownloadPaths) {
            startFileTransfer(1, getLocalPath(remotePath), remotePath);
        }
    });

    menu.addAction("新建文件夹", [=]() {
        bool ok;
        auto name = QInputDialog::getText(this, "新建文件夹", "请输入名称:", QLineEdit::Normal, "", &ok);
        
        if (!ok || name.isEmpty())
            return;

        setStatusMessage("新建文件夹: " + name);

        auto dir = isDir ? remotePath : remotePath.left(remotePath.lastIndexOf('/'));
        send("createDirectory", dir + "/" + name);
    })->setEnabled(selectedCount == 1 || (selectedCount == 0 && rootPath != "/"));

    menu.addAction("刷新文件夹", [=]() {
        auto dir = isDir ? remotePath : remotePath.left(remotePath.lastIndexOf('/'));
        fetchDirectoryContents(dir);
    })->setEnabled(selectedCount == 1);

    menu.exec(treeView->viewport()->mapToGlobal(pos));
}

void RemoteFileExplorer::showTableContextMenu(const QPoint &pos)
{
    QModelIndex index = transferTable->indexAt(pos);
    if (!index.isValid())
        return;

    QString localPath = transferTable->item(index.row(), 5)->text();
    QString remotePath = transferTable->item(index.row(), 6)->text();

    QMenu menu;

    menu.addAction("查看", [=]() {
        new FileViewer(localPath, this);
    });

    menu.addAction("在文件资源管理器中显示", [=]() {
        Tools::showInFileExplorer(localPath);
    });

    menu.addAction("复制本地路径", [=]() {
        qApp->clipboard()->setText(localPath);
    });

    menu.addAction("复制远程路径", [=]() {
        qApp->clipboard()->setText(remotePath);
    });

    menu.exec(transferTable->viewport()->mapToGlobal(pos));
}

void RemoteFileExplorer::dragEnterEvent(QDragEnterEvent *event)
{
    qDebugEx() << "dragEnterEvent";

    if (!event->mimeData()->hasUrls()) 
        event->ignore();

    for (const QUrl &url : event->mimeData()->urls()) {
        if (QFileInfo(url.toLocalFile()).isDir()) {
            event->ignore();
            return;
        }
    }
    
    event->accept();
}

void RemoteFileExplorer::dragMoveEvent(QDragMoveEvent *event)
{
    QPoint globalPos = mapToGlobal(event->position().toPoint());
    QPoint localPos = treeView->viewport()->mapFromGlobal(globalPos);
    
    treeView->selectionModel()->clearSelection();

    QModelIndex index = treeView->indexAt(localPos);
    if (index.isValid())
        treeView->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    event->accept();
}

void RemoteFileExplorer::dropEvent(QDropEvent *event)
{
    QPoint globalPos = mapToGlobal(event->position().toPoint());
    QPoint localPos = treeView->viewport()->mapFromGlobal(globalPos);
    
    QModelIndex index = treeView->indexAt(localPos);

    QString targetPath = rootPath;
    bool isDir = true;

    if (index.isValid()) {
        targetPath = index.data(Qt::UserRole).toString();
        isDir = index.data(Qt::UserRole + 2).toBool();
    }

    for (const QUrl &url : event->mimeData()->urls()) {
        auto localPath = url.toLocalFile();

        qDebugEx() << localPath << "=>" << targetPath;

        auto dir = isDir ? targetPath : targetPath.left(targetPath.lastIndexOf('/'));

        startFileTransfer(2, localPath, dir + QString("/") + QFileInfo(localPath).fileName());
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
