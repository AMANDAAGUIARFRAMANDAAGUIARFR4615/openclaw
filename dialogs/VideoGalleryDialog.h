#pragma once

#include "BaseDialog.h"
#include "Tools.h"
#include <algorithm>
#include <memory>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

/** 浏览本机保存的投屏录像（screenvid_*.h264），列表 + 内置预览。 */
class VideoGalleryDialog : public BaseDialog {
public:
    static void open(QWidget *parent, const QString &deviceId) {
        VideoGalleryDialog dialog(parent, deviceId);
        dialog.resize(920, 560);
        dialog.exec();
    }

private:
    explicit VideoGalleryDialog(QWidget *parent, const QString &deviceId)
        : BaseDialog(QStringLiteral("录像文件"), parent), galleryDir_(Tools::screenVideoSaveDirectory(deviceId)) {
        auto *root = contentLayout();
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(8);

        auto *top = new QHBoxLayout();
        countLabel_ = new QLabel(this);
        top->addWidget(countLabel_);
        top->addStretch();
        auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
        auto *folderBtn = new QPushButton(QStringLiteral("打开文件夹"), this);
        top->addWidget(refreshBtn);
        top->addWidget(folderBtn);
        root->addLayout(top);

        pathLabel_ = new QLabel(this);
        pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pathLabel_->setWordWrap(true);
        root->addWidget(pathLabel_);

        auto *row = new QHBoxLayout();
        list_ = new QListWidget(this);
        list_->setMinimumWidth(280);
        video_ = new QVideoWidget(this);
        video_->setMinimumSize(480, 360);
        row->addWidget(list_, 0);
        row->addWidget(video_, 1);
        root->addLayout(row, 1);

        metaLabel_ = new QLabel(this);
        metaLabel_->setWordWrap(true);
        root->addWidget(metaLabel_);

        player_ = new QMediaPlayer(this);
        player_->setVideoOutput(video_);
        player_->setAudioOutput(nullptr);

        connect(refreshBtn, &QPushButton::clicked, this, &VideoGalleryDialog::reload);
        connect(folderBtn, &QPushButton::clicked, this, [this]() {
            QDir().mkpath(galleryDir_);
            QDesktopServices::openUrl(QUrl::fromLocalFile(galleryDir_));
        });
        connect(list_, &QListWidget::currentItemChanged, this, &VideoGalleryDialog::onPick);
        connect(list_, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem *item) {
            if (!item)
                return;
            const QString p = item->data(Qt::UserRole).toString();
            if (!p.isEmpty())
                QDesktopServices::openUrl(QUrl::fromLocalFile(p));
        });
        connect(player_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &msg) {
            if (!msg.isEmpty())
                metaLabel_->setText(metaLabel_->text() + QStringLiteral("\n") + msg);
        });

        reload();
    }

    ~VideoGalleryDialog() override { detachPlaySource(); }

    void reload() {
        QDir().mkpath(galleryDir_);
        pathLabel_->setText(galleryDir_);

        QDir dir(galleryDir_);
        const auto files = dir.entryInfoList(
            QStringList{QStringLiteral("screenvid_*.h264"), QStringLiteral("screen_rec_*.h264"),
                        QStringLiteral("videocap_*.h264")},
            QDir::Files | QDir::Readable, QDir::NoSort);
        QFileInfoList sorted = files;
        std::sort(sorted.begin(), sorted.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.lastModified() > b.lastModified();
        });

        list_->clear();
        detachPlaySource();
        metaLabel_->clear();

        for (const QFileInfo &fi : sorted) {
            auto *it = new QListWidgetItem(
                QStringLiteral("%1\n%2")
                    .arg(fi.fileName(), fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
            it->setData(Qt::UserRole, fi.absoluteFilePath());
            list_->addItem(it);
        }

        countLabel_->setText(QStringLiteral("共 %1 个文件").arg(sorted.size()));
        if (list_->count() > 0)
            list_->setCurrentRow(0);
        else
            metaLabel_->setText(QStringLiteral(
                "暂无录像。请在投屏已连接时勾选「投屏录像」；文件保存在本页路径下的 screenvideos/<设备>。"));
    }

    void onPick(QListWidgetItem *cur, QListWidgetItem *) {
        if (!cur) {
            detachPlaySource();
            metaLabel_->clear();
            return;
        }
        const QString path = cur->data(Qt::UserRole).toString();
        QFileInfo fi(path);
        if (!fi.exists()) {
            metaLabel_->setText(QStringLiteral("文件不存在"));
            return;
        }
        detachPlaySource();
        playSourceFile_ = std::make_unique<QFile>(path);
        if (!playSourceFile_->open(QIODevice::ReadOnly)) {
            metaLabel_->setText(QStringLiteral("无法打开文件：%1").arg(playSourceFile_->errorString()));
            playSourceFile_.reset();
            return;
        }
        // 用 QIODevice 喂给解码器，避免精简版 FFmpeg 未注册 file:// 协议时报 Protocol not found
        player_->setSourceDevice(playSourceFile_.get(), QUrl::fromLocalFile(path));
        player_->play();
        metaLabel_->setText(QStringLiteral("%1\n%2 · %3 字节")
                                .arg(path, fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                     QString::number(fi.size())));
    }

    void detachPlaySource() {
        if (!player_)
            return;
        player_->stop();
        player_->setSource(QUrl());
        playSourceFile_.reset();
    }

    QString galleryDir_;
    QListWidget *list_{nullptr};
    QVideoWidget *video_{nullptr};
    QLabel *pathLabel_{nullptr};
    QLabel *countLabel_{nullptr};
    QLabel *metaLabel_{nullptr};
    QMediaPlayer *player_{nullptr};
    std::unique_ptr<QFile> playSourceFile_;
};
