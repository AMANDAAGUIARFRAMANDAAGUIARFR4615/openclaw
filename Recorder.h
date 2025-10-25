#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QVBoxLayout>

class FileFilterProxyModel : public QSortFilterProxyModel {
public:
    FileFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        QFileSystemModel *fileSystemModel = static_cast<QFileSystemModel *>(sourceModel());
        QFileInfo fileInfo = fileSystemModel->fileInfo(index);

        if (fileInfo.isFile()) {
            QStringList allowedExtensions = {"txt", "cpp", "h"};
            return allowedExtensions.contains(fileInfo.suffix(), Qt::CaseInsensitive);
        }

        return true;
    }
};

class Recorder : public QWidget {
    Q_OBJECT

public:
    Recorder(QWidget *parent = nullptr) : QWidget(parent) {
        // 设置主界面布局
        QVBoxLayout *layout = new QVBoxLayout(this);

        // 创建文件系统模型
        fileSystemModel = new QFileSystemModel();
        fileSystemModel->setRootPath(QDir::currentPath());

        // 创建过滤模型
        filterModel = new FileFilterProxyModel();
        filterModel->setSourceModel(fileSystemModel);

        // 创建树形视图
        treeView = new QTreeView(this);
        treeView->setModel(filterModel);
        QModelIndex rootIndex = fileSystemModel->index(QDir::currentPath());
        treeView->setRootIndex(filterModel->mapFromSource(rootIndex));
        treeView->setColumnHidden(2, true);

        layout->addWidget(treeView);
        setLayout(layout);
        resize(800, 600);
        setWindowTitle("文件浏览器");

        treeView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(treeView, &QTreeView::customContextMenuRequested, this, &Recorder::showContextMenu);

        // 启用拖放功能
        treeView->setDragEnabled(true);
        treeView->setAcceptDrops(true);
        treeView->setDropIndicatorShown(true);
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override {
        event->accept();
    }

    void dropEvent(QDropEvent *event) override {
        QModelIndex index = treeView->indexAt(event->pos());
        if (!index.isValid()) return;

        QFileSystemModel *fileSystemModel = static_cast<QFileSystemModel *>(treeView->model());
        QModelIndex srcIndex = static_cast<FileFilterProxyModel *>(treeView->model())->mapToSource(index);
        QFileInfo targetDir = fileSystemModel->fileInfo(srcIndex);
        QString targetPath = targetDir.absoluteFilePath();

        if (targetDir.isDir()) {
            QList<QUrl> urls = event->mimeData()->urls();
            foreach (const QUrl &url, urls) {
                QString sourcePath = url.toLocalFile();
                if (QFile::exists(sourcePath)) {
                    QFileInfo sourceFile(sourcePath);
                    QString destinationPath = targetPath + QDir::separator() + sourceFile.fileName();
                    if (QFile::rename(sourcePath, destinationPath)) {
                        QMessageBox::information(this, "成功", QString("文件已移动到 %1").arg(targetPath));
                    } else {
                        QMessageBox::warning(this, "错误", "无法移动文件！");
                    }
                }
            }
        }

        QWidget::dropEvent(event);
    }

private slots:
    void showContextMenu(const QPoint &pos) {
        QModelIndex index = treeView->indexAt(pos);
        if (!index.isValid())
            return;

        QModelIndex srcIndex = filterModel->mapToSource(index);
        QFileInfo fileInfo = fileSystemModel->fileInfo(srcIndex);
        QString path = fileInfo.absoluteFilePath();

        QMenu menu;
        QAction *newFolderAction = menu.addAction("新建文件夹");
        QAction *renameAction = menu.addAction("重命名");
        QAction *deleteAction = menu.addAction("删除");

        QAction *selectedAction = menu.exec(treeView->viewport()->mapToGlobal(pos));
        if (!selectedAction) return;

        if (selectedAction == newFolderAction) {
            bool ok;
            QString folderName = QInputDialog::getText(nullptr, "新建文件夹",
                                                       "文件夹名称：", QLineEdit::Normal,
                                                       "新建文件夹", &ok);
            if (ok && !folderName.isEmpty()) {
                QDir dir(path);
                if (!fileInfo.isDir())
                    dir = fileInfo.dir();
                if (!dir.mkdir(folderName))
                    QMessageBox::warning(this, "错误", "无法创建文件夹！");
            }
        } else if (selectedAction == renameAction) {
            bool ok;
            QString newName = QInputDialog::getText(nullptr, "重命名",
                                                    "新名称：", QLineEdit::Normal,
                                                    fileInfo.fileName(), &ok);
            if (ok && !newName.isEmpty()) {
                QDir dir = fileInfo.dir();
                QString newPath = dir.filePath(newName);
                if (!QFile::rename(path, newPath))
                    QMessageBox::warning(this, "错误", "无法重命名文件！");
            }
        } else if (selectedAction == deleteAction) {
            if (QMessageBox::question(this, "确认删除",
                                      QString("确定删除 “%1” 吗？").arg(fileInfo.fileName())) == QMessageBox::Yes) {
                if (fileInfo.isDir())
                    QDir(path).removeRecursively();
                else
                    QFile::remove(path);
            }
        }
    }

private:
    QFileSystemModel *fileSystemModel;
    FileFilterProxyModel *filterModel;
    QTreeView *treeView;
};