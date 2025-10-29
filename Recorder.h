#pragma once

#include "DeviceConnection.h"
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
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>

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
    Recorder(DeviceConnection* connection, QWidget *parent = nullptr)
        : connection(connection), QWidget(parent), isRecording(false), isPaused(false) {
        QString recorderPath = QDir::currentPath() + "/recorder";

        QDir dir;
        if (!dir.exists(recorderPath))
            dir.mkpath(recorderPath);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QHBoxLayout *buttonLayout = new QHBoxLayout();

        btnStart = new QPushButton("开始录制", this);
        btnStop  = new QPushButton("停止录制", this);
        btnPause = new QPushButton("暂停录制", this);
        btnResume= new QPushButton("恢复录制", this);

        buttonLayout->addWidget(btnStart);
        buttonLayout->addWidget(btnStop);
        buttonLayout->addWidget(btnPause);
        buttonLayout->addWidget(btnResume);
        buttonLayout->addStretch();

        fileSystemModel = new QFileSystemModel();
        fileSystemModel->setRootPath(recorderPath);

        filterModel = new FileFilterProxyModel();
        filterModel->setSourceModel(fileSystemModel);

        treeView = new QTreeView(this);
        treeView->setModel(filterModel);
        QModelIndex rootIndex = fileSystemModel->index(recorderPath);
        treeView->setRootIndex(filterModel->mapFromSource(rootIndex));
        treeView->setColumnHidden(2, true);  // 隐藏“类型”列

        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(treeView);
        setLayout(mainLayout);

        treeView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(treeView, &QTreeView::customContextMenuRequested, this, &Recorder::showContextMenu);

        treeView->setDragEnabled(true);
        treeView->setAcceptDrops(true);
        treeView->setDropIndicatorShown(true);

        connect(btnStart,  &QPushButton::clicked, this, &Recorder::onStart);
        connect(btnStop,   &QPushButton::clicked, this, &Recorder::onStop);
        connect(btnPause,  &QPushButton::clicked, this, &Recorder::onPause);
        connect(btnResume, &QPushButton::clicked, this, &Recorder::onResume);

        updateButtonStates();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override {
        event->accept();
    }

    void dropEvent(QDropEvent *event) override {
        QModelIndex index = treeView->indexAt(event->pos());
        if (!index.isValid()) return;

        // 注意：这里使用 proxy model，需要映射到 source model
        QSortFilterProxyModel *proxy = static_cast<QSortFilterProxyModel *>(treeView->model());
        QModelIndex srcIndex = proxy->mapToSource(index);
        QFileInfo targetDir = fileSystemModel->fileInfo(srcIndex);
        QString targetPath = targetDir.absoluteFilePath();

        if (targetDir.isDir()) {
            QList<QUrl> urls = event->mimeData()->urls();
            for (const QUrl &url : urls) {
                QString sourcePath = url.toLocalFile();
                if (QFile::exists(sourcePath)) {
                    QFileInfo sourceFile(sourcePath);
                    QString destinationPath = targetPath + QDir::separator() + sourceFile.fileName();

                    if (QFile::rename(sourcePath, destinationPath)) {
                        QMessageBox::information(this, "成功",
                                                 QString("文件已移动到 %1").arg(targetPath));
                    } else {
                        QMessageBox::warning(this, "错误", "无法移动文件！");
                    }
                }
            }
        }
        QWidget::dropEvent(event);
    }

    void showContextMenu(const QPoint &pos) {
        QModelIndex index = treeView->indexAt(pos);
        if (!index.isValid()) return;

        QModelIndex srcIndex = filterModel->mapToSource(index);
        QFileInfo fileInfo = fileSystemModel->fileInfo(srcIndex);
        QString path = fileInfo.absoluteFilePath();

        QMenu menu;
        QAction *newFolderAction = menu.addAction("新建文件夹");
        QAction *renameAction    = menu.addAction("重命名");
        QAction *deleteAction    = menu.addAction("删除");

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
                                      QString("确定删除 “%1” 吗？").arg(fileInfo.fileName()))
                == QMessageBox::Yes) {
                if (fileInfo.isDir())
                    QDir(path).removeRecursively();
                else
                    QFile::remove(path);
            }
        }
    }

private slots:
    void onStart() {
        isRecording = true;
        isPaused = false;
        updateButtonStates();
        
        connection->send("recorder", "start");
    }

    void onStop() {
        isRecording = false;
        isPaused = false;
        updateButtonStates();

        connection->send("recorder", "stop");
    }

    void onPause() {
        isRecording = false;
        isPaused = true;
        updateButtonStates();

        connection->send("recorder", "pause");
    }

    void onResume() {
        isRecording = false;
        isPaused = false;
        updateButtonStates();

        connection->send("recorder", "resume");
    }

private:
    void updateButtonStates() {
        btnStart->setEnabled(!isRecording);
        btnStop->setEnabled(isRecording);
        btnPause->setEnabled(isRecording && !isPaused);
        btnResume->setEnabled(isRecording && isPaused);
    }

private:
    DeviceConnection* connection;

    QPushButton *btnStart;
    QPushButton *btnStop;
    QPushButton *btnPause;
    QPushButton *btnResume;

    bool isRecording;
    bool isPaused;

    QFileSystemModel *fileSystemModel;
    FileFilterProxyModel *filterModel;
    QTreeView *treeView;
};
