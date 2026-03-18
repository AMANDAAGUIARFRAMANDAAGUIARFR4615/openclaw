#pragma once

#include "DeviceConnection.h"
#include "ToastWidget.h"
#include "EventHub.h"
#include "FileViewer.h"
#include "DeviceView.h"
#include "MainWindow.h"
#include <QTreeView>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QWidget>
#include <QStandardPaths>
#include <QCheckBox>
#include <QHash>
#include <QStatusBar>
#include <QHeaderView>

class FileFilterProxyModel : public QSortFilterProxyModel {
public:
    FileFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        QFileSystemModel *fileSystemModel = qobject_cast<QFileSystemModel *>(sourceModel());
        QFileInfo fileInfo = fileSystemModel->fileInfo(index);

        return fileInfo.isFile() ? fileInfo.suffix().compare("recordx", Qt::CaseInsensitive) == 0 : true;
    }

    Qt::DropActions supportedDropActions() const override {
        return Qt::CopyAction | Qt::MoveAction;
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
        QString text = value.toString();
        
        if (role == Qt::EditRole && !text.endsWith(".recordx", Qt::CaseInsensitive) && ((QFileSystemModel*)sourceModel())->fileInfo(mapToSource(index)).isFile()) {
            return QSortFilterProxyModel::setData(index, text + ".recordx", role);
        }
        
        return QSortFilterProxyModel::setData(index, value, role);
    }
};

class Recorder : public QWidget {
    Q_OBJECT

public:
    static Recorder* open(DeviceConnection* connection, DeviceView* deviceView) {
        auto existing = instanceMap.value(connection);
        if (existing) {
            existing->setWindowState(existing->windowState() & ~Qt::WindowMinimized);
            existing->raise();
            existing->activateWindow();
            return existing;
        }

        auto recorder = new Recorder(connection, deviceView);
        recorder->setWindowTitle(connection->deviceInfo->deviceName + "[录制+回放]");
        recorder->resize(920, 400);
        recorder->show();
        return recorder;
    }

private:
    Recorder(DeviceConnection* connection, DeviceView* deviceView) : connection(connection), deviceView(deviceView), QWidget() {
        instanceMap.insert(connection, this);

        setAttribute(Qt::WA_DeleteOnClose);

        connect(deviceView, &QObject::destroyed, this, &QWidget::close);

        recorderPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/recorder";

        static QDir dir;
        if (!dir.exists(recorderPath))
            dir.mkpath(recorderPath);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QHBoxLayout *buttonLayout = new QHBoxLayout();

        startButton = new QPushButton("开始录制", this);
        stopButton = new QPushButton("停止录制", this);
        startPlaybackButton = new QPushButton("开始回放", this);
        pausePlaybackButton = new QPushButton("暂停回放", this);
        resumePlaybackButton = new QPushButton("恢复回放", this);
        stopPlaybackButton = new QPushButton("停止回放", this);

        buttonLayout->addWidget(startButton);
        buttonLayout->addWidget(stopButton);
        buttonLayout->addWidget(startPlaybackButton);
        buttonLayout->addWidget(pausePlaybackButton);
        buttonLayout->addWidget(resumePlaybackButton);
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
        fileSystemModel->setReadOnly(false);

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
        treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

        statusBar = new QStatusBar(this);

        mainLayout->addLayout(buttonLayout);
        mainLayout->addWidget(treeView);
        mainLayout->addWidget(statusBar);
        setLayout(mainLayout);

        treeView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(treeView, &QTreeView::customContextMenuRequested, this, &Recorder::showContextMenu);

        treeView->setDragEnabled(true);
        treeView->setDragDropMode(QAbstractItemView::DragDrop); // 允许拖和放
        treeView->setDefaultDropAction(Qt::MoveAction);         // 默认动作为移动
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
            QModelIndexList selectedRows = treeView->selectionModel()->selectedRows();
            int selectionCount = selectedRows.count();
            if (selectionCount == 0) {
                new ToastWidget("请先选择要回放的文件", this);
                return;
            }

            if (selectionCount > 1) {
                new ToastWidget("只能选择一个文件", this);
                return;
            }

            auto srcIndex = filterModel->mapToSource(selectedRows[0]);
            auto fileInfo = fileSystemModel->fileInfo(srcIndex);
            if (fileInfo.isDir()) {
                new ToastWidget("不能选择文件夹", this);
                return;
            }

            auto path = fileInfo.absoluteFilePath();
            
            onStartPlayback(path);
        });

        connect(pausePlaybackButton, &QPushButton::clicked, [this]() {
            onPausePlayback();
        });

        connect(resumePlaybackButton, &QPushButton::clicked, [this]() {
            onResumePlayback();
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
            
            if (!isPlaying)
                isPaused = false;
            
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

    void showContextMenu(const QPoint &pos) {
        QModelIndex index = treeView->indexAt(pos);

        QModelIndexList selectedRows = treeView->selectionModel()->selectedRows();
        int selectionCount = selectedRows.count();

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

        menu.addAction("编辑", [=]() {
            new FileViewer(path, this);
        })->setEnabled(selectionCount == 1 && !fileInfo.isDir());

        menu.addAction("重命名", [=]() {
            treeView->edit(index);
        })->setEnabled(selectionCount == 1);

        menu.addAction("删除", [=]() {
            if (QMessageBox::question(this, "确认删除", QString("确定删除选中的 %1 项内容吗？").arg(selectionCount)) == QMessageBox::Yes) {
                for (const auto &idx : selectedRows) {
                    QModelIndex srcIdx = filterModel->mapToSource(idx);
                    QFileInfo info = fileSystemModel->fileInfo(srcIdx);
                    if (info.isDir())
                        QDir(info.absoluteFilePath()).removeRecursively();
                    else
                        QFile::remove(info.absoluteFilePath());
                }
            }
        })->setEnabled(selectionCount > 0);

        menu.addAction("新建文件夹", [=]() {
            bool ok;
            QString folderName = QInputDialog::getText(this, "新建文件夹",
                                                        "文件夹名称：", QLineEdit::Normal,
                                                        "新建文件夹", &ok);
            if (ok && !folderName.isEmpty()) {
                QDir dir(fileInfo.isDir() ? path : fileInfo.dir());
                if (!dir.mkdir(folderName))
                    new ToastWidget("无法创建文件夹！", this);
            }
        })->setEnabled(selectionCount <= 1);

        menu.exec(treeView->viewport()->mapToGlobal(pos));
    }

    void updateButtonStates() {
        startButton->setEnabled(!isRecording && !isPlaying);
        stopButton->setEnabled(isRecording && !isPlaying);
        
        startPlaybackButton->setEnabled(!isPlaying && !isRecording);
        pausePlaybackButton->setEnabled(isPlaying && !isPaused && !isRecording);
        resumePlaybackButton->setEnabled(isPlaying && isPaused && !isRecording);
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

        for (const auto& connection : MainWindow::getInstance()->getDeviceConnections(deviceView)) {
            connection->send("playback", dataObject);
        }

        isPlaying = true;
        isPaused = false;
        updateButtonStates();
    }

    void onPausePlayback() {
        connection->send("playback", QJsonObject{{"type", "pause"}});

        isPaused = true;
        updateButtonStates();
    }

    void onResumePlayback() {
        connection->send("playback", QJsonObject{{"type", "resume"}});

        isPaused = false;
        updateButtonStates();
    }

    void onStopPlayback() {
        connection->send("playback", QJsonObject{{"type", "stop"}});

        isPlaying = false;
        isPaused = false;
        updateButtonStates();
    }

    void setStatusMessage(const QString &message)
    {
        auto timestamp = QTime::currentTime().toString("HH:mm:ss");
        statusBar->showMessage("[" + timestamp + "] " + message);
    }

    DeviceConnection* const connection;
    DeviceView* const deviceView;
    QString recorderPath;

    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *startPlaybackButton;
    QPushButton *pausePlaybackButton;
    QPushButton *resumePlaybackButton;
    QPushButton *stopPlaybackButton;
    QSpinBox *playbackTimesSpinBox;
    QCheckBox *infiniteCheckBox;

    bool isRecording = false;
    bool isPlaying = false;
    bool isPaused = false;

    QFileSystemModel *fileSystemModel;
    FileFilterProxyModel *filterModel;
    QTreeView *treeView;
    QStatusBar *statusBar;

    inline static QHash<DeviceConnection*, Recorder*> instanceMap;
};