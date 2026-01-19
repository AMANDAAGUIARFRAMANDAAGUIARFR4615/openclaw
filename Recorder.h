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
#include <QCheckBox>
#include <QMap>
#include <QStatusBar>
#include <QHeaderView>

class FileFilterProxyModel : public QSortFilterProxyModel {
public:
    FileFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        QFileSystemModel *fileSystemModel = qobject_cast<QFileSystemModel *>(sourceModel());
        QFileInfo fileInfo = fileSystemModel->fileInfo(index);

        return fileInfo.isFile() ? fileInfo.suffix() == "recordx" : true;
    }
};

class Recorder : public QWidget {
    Q_OBJECT

public:
    static Recorder* open(DeviceConnection* connection) {
        auto existing = instanceMap.value(connection);
        if (existing) {
            existing->setWindowState(existing->windowState() & ~Qt::WindowMinimized);
            existing->raise();
            existing->activateWindow();
            return existing;
        }

        auto recorder = new Recorder(connection);
        recorder->setWindowTitle(connection->displayName());
        recorder->resize(920, 400);
        recorder->show();
        return recorder;
    }

private:
    Recorder(DeviceConnection* connection) : connection(connection), QWidget() {
        instanceMap[connection] = this;

        setAttribute(Qt::WA_DeleteOnClose);

        recorderPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/recorder";

        static QDir dir;
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

        infiniteCheckBox = new QCheckBox("无限", this);
        buttonLayout->addWidget(infiniteCheckBox);

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
        treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        treeView->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        treeView->header()->setStretchLastSection(false);

        statusBar = new QStatusBar(this);

        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(treeView);
        mainLayout->addWidget(statusBar);
        setLayout(mainLayout);

        treeView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(treeView, &QTreeView::customContextMenuRequested, this, &Recorder::showContextMenu);

        treeView->setDragEnabled(true);
        treeView->setAcceptDrops(true);
        treeView->setDropIndicatorShown(true);

        connect(infiniteCheckBox, &QCheckBox::toggled, [=](bool checked) {
            playbackTimesSpinBox->setEnabled(!checked);
        });

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

        EventHub::on(this, "recorderReport", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            auto text = data.toString();
            if (text == "") {
                new ToastWidget("没有录制内容", this);
                return;
            }

            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");

            QFile file(QString("%1/%2_%3.recordx").arg(recorderPath, connection->deviceInfo->deviceName, timestamp));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                new ToastWidget(file.errorString(), this);
                return;
            }

            QTextStream out(&file);
            out << text;
            file.close();
        });

        EventHub::on(this, "playbackStatus", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            auto code = data["code"].toInt();
            setStatusMessage(data["msg"].toString());
            isPlaying = code != 5;
            updateButtonStates();
        });
    }

    ~Recorder() {
        EventHub::off(this, "recorderReport");
        EventHub::off(this, "playbackStatus");

        instanceMap.remove(connection);

        delete ((QSortFilterProxyModel*)treeView->model())->sourceModel();
        delete treeView->model();
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
        QModelIndex index = treeView->indexAt(event->position().toPoint());
        if (!index.isValid()) return;

        QSortFilterProxyModel *proxy = qobject_cast<QSortFilterProxyModel *>(treeView->model());
        QModelIndex srcIndex = proxy->mapToSource(index);
        QFileInfo targetDir = fileSystemModel->fileInfo(srcIndex);
        QString targetPath = targetDir.absoluteFilePath();

        if (targetDir.isDir()) {
            for (const QUrl &url : event->mimeData()->urls()) {
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

        QMenu menu;

        if (index.isValid()) {
            if (!fileInfo.isDir()) {
                menu.addAction("编辑", [=]() {
                    new FileViewer(path, this);
                });

                if (!isPlaying) {
                    menu.addAction("开始回放", [=]() {
                        onStartPlayback(path);
                    });
                }
                else {
                    menu.addAction("停止回放", [this]() {
                        onStopPlayback();
                    });
                }
            }

            menu.addAction("重命名", [=]() {
                bool ok;
                QString newName = QInputDialog::getText(this, "重命名",
                                                        "新名称：", QLineEdit::Normal,
                                                        fileInfo.fileName(), &ok);
                if (ok && !newName.isEmpty()) {
                    QDir dir = fileInfo.dir();
                    QString newPath = dir.filePath(newName);
                    if (!QFile::rename(path, newPath))
                        new ToastWidget("无法重命名文件！", this);
                }
            });

            menu.addAction("删除", [=]() {
                if (QMessageBox::question(this, "确认删除",
                                          QString("确定删除 “%1” 吗？").arg(fileInfo.fileName()))
                    == QMessageBox::Yes) {
                    if (fileInfo.isDir())
                        QDir(path).removeRecursively();
                    else
                        QFile::remove(path);
                }
            });
        }

        menu.addAction("新建文件夹", [=]() {
            bool ok;
            QString folderName = QInputDialog::getText(this, "新建文件夹",
                                                       "文件夹名称：", QLineEdit::Normal,
                                                       "新建文件夹", &ok);
            if (ok && !folderName.isEmpty()) {
                QDir dir(fileInfo.isDir() ? fileInfo.dir() : path);
                if (!dir.mkdir(folderName))
                    new ToastWidget("无法创建文件夹！", this);
            }
        });

        menu.exec(treeView->viewport()->mapToGlobal(pos));
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
            qCriticalEx() << "文件打开失败:" << file.errorString();
            return;
        }

        QJsonObject dataObject;
        dataObject["type"] = "start";
        dataObject["script"] = QString::fromUtf8(file.readAll());
        dataObject["repeat"] = infiniteCheckBox->isChecked() ? -1 : playbackTimesSpinBox->value();

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

    void setStatusMessage(const QString &message)
    {
        auto timestamp = QTime::currentTime().toString("HH:mm:ss");
        statusBar->showMessage("[" + timestamp + "] " + message);
    }

    DeviceConnection* connection;
    QString recorderPath;

    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *startPlaybackButton;
    QPushButton *stopPlaybackButton;
    QSpinBox *playbackTimesSpinBox;
    QCheckBox *infiniteCheckBox;

    bool isRecording = false;
    bool isPlaying = false;

    QFileSystemModel *fileSystemModel;
    FileFilterProxyModel *filterModel;
    QTreeView *treeView;
    QStatusBar *statusBar;

    inline static QMap<DeviceConnection*, Recorder*> instanceMap;
};
