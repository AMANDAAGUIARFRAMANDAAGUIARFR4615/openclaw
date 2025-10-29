#pragma once

#include "DeviceConnection.h"
#include "ToastWidget.h"
#include "EventHub.h"
#include "FileViewer.h"
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
#include <QLabel>
#include <QSpinBox>
#include <QWidget>
#include <QStandardPaths>

class FileFilterProxyModel : public QSortFilterProxyModel {
public:
    FileFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        QFileSystemModel *fileSystemModel = static_cast<QFileSystemModel *>(sourceModel());
        QFileInfo fileInfo = fileSystemModel->fileInfo(index);

        return fileInfo.isFile() ? fileInfo.suffix() == "recordx" : true;
    }
};

class Recorder : public QWidget {
    Q_OBJECT

public:
    Recorder(DeviceConnection* connection, QWidget *parent = nullptr) : connection(connection), QWidget(parent) {
        recorderPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recorder";

        QDir dir;
        if (!dir.exists(recorderPath))
            dir.mkpath(recorderPath);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QHBoxLayout *buttonLayout = new QHBoxLayout();

        startButton = new QPushButton("开始录制", this);
        stopButton = new QPushButton("停止录制", this);
        startPlaybackButton = new QPushButton("开始回放", this);
        stopPlaybackButton = new QPushButton("停止回放", this);

        buttonLayout->addWidget(startButton);
        buttonLayout->addWidget(stopButton);
        buttonLayout->addWidget(startPlaybackButton);
        buttonLayout->addWidget(stopPlaybackButton);

        buttonLayout->addWidget(new QLabel("回放次数:", this));
        playbackTimesSpinBox = new QSpinBox(this);
        playbackTimesSpinBox->setRange(1, 999);
        playbackTimesSpinBox->setValue(1);
        buttonLayout->addWidget(playbackTimesSpinBox);

        buttonLayout->addStretch();

        fileSystemModel = new QFileSystemModel();
        fileSystemModel->setRootPath(recorderPath);

        filterModel = new FileFilterProxyModel();
        filterModel->setSourceModel(fileSystemModel);

        treeView = new QTreeView(this);
        treeView->setModel(filterModel);
        QModelIndex rootIndex = fileSystemModel->index(recorderPath);
        treeView->setRootIndex(filterModel->mapFromSource(rootIndex));
        treeView->setColumnHidden(2, true);

        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(treeView);
        setLayout(mainLayout);

        treeView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(treeView, &QTreeView::customContextMenuRequested, this, &Recorder::showContextMenu);

        treeView->setDragEnabled(true);
        treeView->setAcceptDrops(true);
        treeView->setDropIndicatorShown(true);

        connect(startButton,  &QPushButton::clicked, [=]() {
            isRecording = true;
            updateButtonStates();

            connection->send("recorder", "start");
        });

        connect(stopButton,   &QPushButton::clicked, [=]() {
            isRecording = false;
            updateButtonStates();

            connection->send("recorder", "stop");
        });

        connect(startPlaybackButton,  &QPushButton::clicked, [=]() {
            auto currentIndex = treeView->currentIndex();
            if (!currentIndex.isValid()) {
                new ToastWidget("请先选择回放文件", this);
                return;
            }

            auto srcIndex = filterModel->mapToSource(currentIndex);
            auto fileInfo = fileSystemModel->fileInfo(srcIndex);
            auto path = fileInfo.absoluteFilePath();
            
            onStartPlayback(path);
        });

        connect(stopPlaybackButton,  &QPushButton::clicked, [this]() {
            onStopPlayback();
        });

        updateButtonStates();

        EventHub::StartListening("recorderReport", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            auto text = data.toString();
            if (text == "") {
                new ToastWidget("没有录制内容", this);
                return;
            }

            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");

            QFile file(QString("%1/%2_%3.recordx").arg(recorderPath).arg(connection->deviceInfo->deviceName).arg(timestamp));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                new ToastWidget(file.errorString(), this);
                return;
            }

            QTextStream out(&file);
            out << text;
            file.close();
        });
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
        QFileInfo fileInfo;
        QString path;

        if (index.isValid()) {
            QModelIndex srcIndex = filterModel->mapToSource(index);
            fileInfo = fileSystemModel->fileInfo(srcIndex);
            path = fileInfo.absoluteFilePath();
        } else {
            fileInfo = QFileInfo(recorderPath);
            path = recorderPath;
        }

        QMenu *menu = new QMenu(treeView);

        if (index.isValid()) {
            if (!fileInfo.isDir()) {
                QAction *editAction = menu->addAction("编辑");
                QObject::connect(editAction, &QAction::triggered, [=]() {
                    new FileViewer(path);
                });

                if (!isPlaying) {
                    QAction *playAction = menu->addAction("开始回放");
                    QObject::connect(playAction, &QAction::triggered, [=]() {
                        onStartPlayback(path);
                    });
                }
                else {
                    QAction *stopAction = menu->addAction("停止回放");
                    QObject::connect(stopAction, &QAction::triggered, [this]() {
                        onStopPlayback();
                    });
                }
            }

            QAction *renameAction = menu->addAction("重命名");
            QObject::connect(renameAction, &QAction::triggered, [=]() {
                bool ok;
                QString newName = QInputDialog::getText(nullptr, "重命名",
                                                        "新名称：", QLineEdit::Normal,
                                                        fileInfo.fileName(), &ok);
                if (ok && !newName.isEmpty()) {
                    QDir dir = fileInfo.dir();
                    QString newPath = dir.filePath(newName);
                    if (!QFile::rename(path, newPath))
                        new ToastWidget("无法重命名文件！", this);
                }
            });

            QAction *deleteAction = menu->addAction("删除");
            QObject::connect(deleteAction, &QAction::triggered, [=]() {
                if (QMessageBox::question(nullptr, "确认删除",
                                          QString("确定删除 “%1” 吗？").arg(fileInfo.fileName()))
                    == QMessageBox::Yes) {
                    if (fileInfo.isDir())
                        QDir(path).removeRecursively();
                    else
                        QFile::remove(path);
                }
            });
        }

        QAction *newFolderAction = menu->addAction("新建文件夹");
        QObject::connect(newFolderAction, &QAction::triggered, [=]() {
            bool ok;
            QString folderName = QInputDialog::getText(nullptr, "新建文件夹",
                                                       "文件夹名称：", QLineEdit::Normal,
                                                       "新建文件夹", &ok);
            if (ok && !folderName.isEmpty()) {
                QDir dir(fileInfo.isDir() ? fileInfo.dir() : path);
                if (!dir.mkdir(folderName))
                    new ToastWidget("无法创建文件夹！", this);
            }
        });

        menu->exec(treeView->viewport()->mapToGlobal(pos));
        delete menu;
    }

    void updateButtonStates() {
        startButton->setEnabled(!isRecording && !isPlaying);
        stopButton->setEnabled(isRecording && !isPlaying);
        startPlaybackButton->setEnabled(!isPlaying && !isRecording);
        stopPlaybackButton->setEnabled(isPlaying && !isRecording);
    }

    void onStartPlayback(QString path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "文件打开失败:" << file.errorString();
            return;
        }

        QJsonObject dataObject;
        dataObject["type"] = "start";
        dataObject["script"] = QString::fromUtf8(file.readAll());
        dataObject["repeat"] = playbackTimesSpinBox->value();

        connection->send("playback", dataObject);

        isPlaying = true;
        updateButtonStates();
    }

    void onStopPlayback() {
        QJsonObject dataObject;
        dataObject["type"] = "stop";
        connection->send("playback", dataObject);

        isPlaying = false;
        updateButtonStates();
    }

    DeviceConnection* connection;
    QString recorderPath;

    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *startPlaybackButton;
    QPushButton *stopPlaybackButton;
    QSpinBox *playbackTimesSpinBox;

    bool isRecording = false;
    bool isPlaying = false;

    QFileSystemModel *fileSystemModel;
    FileFilterProxyModel *filterModel;
    QTreeView *treeView;
};