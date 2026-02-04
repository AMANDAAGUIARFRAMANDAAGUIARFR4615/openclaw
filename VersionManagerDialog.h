#pragma once

#include "global.h"
#include "ZipUtils.h"
#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QFrame>
#include <QStyleOption>
#include <QPainter>
#include <QMessageBox>
#include <QProgressDialog>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

/**
 * @brief 版本卡片控件
 * 显示单个版本的详细信息
 */
class VersionCard : public QFrame {
    Q_OBJECT
public:
    // 增加 isLatest 参数，用于区分最新版样式
    explicit VersionCard(const QJsonObject &data, bool isLatest = false, QWidget *parent = nullptr)
        : QFrame(parent), m_isLatest(isLatest)
    {
        // 设置属性以便 QSS 根据 isLatest 状态应用不同样式
        setProperty("isLatest", isLatest);
        setupData(data);
        setupUI();
    }

signals:
    void startDownload(const QString &url, const QString &tagName);

private:
    bool m_isLatest;
    QString m_tagName;
    QString m_releaseUrl;
    QString m_downloadUrl;
    QString m_dateStr;
    QString m_body;

    void setupData(const QJsonObject &data) {
        m_tagName = data["tag_name"].toString();
        m_releaseUrl = data["html_url"].toString(); // 发布页链接

        // 解析时间 (ISO 8601)
        QString rawDate = data["created_at"].toString();
        QDateTime dt = QDateTime::fromString(rawDate, Qt::ISODate);
        m_dateStr = dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm") : rawDate;

        // 简略说明 (截取前100个字符)
        m_body = data["body"].toString().simplified();
        if (m_body.length() > 80) m_body = m_body.left(80) + "...";

        // 尝试获取第一个资源文件的下载链接
        QJsonArray assets = data["assets"].toArray();
        if (!assets.isEmpty()) {
            m_downloadUrl = assets.first().toObject()["browser_download_url"].toString();
        } else {
            m_downloadUrl = "";
        }
    }

    void setupUI() {
        this->setObjectName("VersionCard");

        QHBoxLayout *layout = new QHBoxLayout(this);
        // 如果是最新版，内边距稍微大一点
        int margin = m_isLatest ? 25 : 20;
        layout->setContentsMargins(margin, margin, margin, margin);
        layout->setSpacing(20);

        // --- 左侧区域 ---
        QVBoxLayout *leftLayout = new QVBoxLayout();

        // 1. 标签行 (包含 Tag 和可能的 "最新" 徽章)
        QHBoxLayout *tagLine = new QHBoxLayout();
        tagLine->setSpacing(10);

        QLabel *lblTag = new QLabel(m_tagName, this);
        lblTag->setObjectName("LabelTag");
        tagLine->addWidget(lblTag);

        if (m_isLatest) {
            QLabel *badge = new QLabel("最新推荐", this);
            badge->setObjectName("BadgeLatest");
            badge->setAlignment(Qt::AlignCenter);
            tagLine->addWidget(badge);
        }
        tagLine->addStretch(); // 左对齐

        // 2. 日期
        QLabel *lblDate = new QLabel(m_dateStr, this);
        lblDate->setObjectName("LabelDate");

        leftLayout->addLayout(tagLine);
        leftLayout->addWidget(lblDate);
        leftLayout->addStretch();

        // --- 中间区域：描述信息 ---
        QLabel *lblBody = new QLabel(m_body, this);
        lblBody->setObjectName("LabelBody");
        lblBody->setWordWrap(true);
        lblBody->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // --- 右侧区域：按钮 ---
        QPushButton *btnDownload = new QPushButton(this);
        btnDownload->setCursor(Qt::PointingHandCursor);
        // 最新版按钮稍微大一点
        btnDownload->setFixedWidth(m_isLatest ? 120 : 100);
        btnDownload->setFixedHeight(m_isLatest ? 36 : 32);

        if (!m_downloadUrl.isEmpty()) {
            btnDownload->setText(m_isLatest ? "立即更新" : "下载");
            btnDownload->setObjectName("BtnDownload");
            connect(btnDownload, &QPushButton::clicked, this, [this](){
                emit startDownload(m_downloadUrl, m_tagName);
            });
        }

        layout->addLayout(leftLayout, 2); // 调整权重
        layout->addWidget(lblBody, 3);
        layout->addWidget(btnDownload, 0);
    }

    void paintEvent(QPaintEvent *event) override {
        QStyleOption opt;
        opt.initFrom(this);
        QPainter p(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    }
};

/**
 * @brief 主对话框
 */
class VersionManagerDialog : public QDialog {
    Q_OBJECT
public:
    VersionManagerDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle(QString("软件更新"));
        resize(800, 720);

        setupUI();
        fetchVersions();
    }

private:
    QVBoxLayout *m_listLayout;
    QWidget *m_scrollContent;
    QWidget *m_loadingWidget;
    QLabel *m_statusLabel;

    void setupUI() {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 1. 顶部标题栏
        QWidget *header = new QWidget(this);
        header->setObjectName("Header");
        QHBoxLayout *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(25, 20, 25, 20);

        QLabel *title = new QLabel("版本列表", header);
        title->setObjectName("Title");

        headerLayout->addWidget(title);
        // 移除了右上角的 Subtitle

        // 2. 滚动区域
        QScrollArea *scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        m_scrollContent = new QWidget();
        m_scrollContent->setObjectName("ScrollContent");
        m_listLayout = new QVBoxLayout(m_scrollContent);
        m_listLayout->setContentsMargins(20, 20, 20, 20);
        m_listLayout->setSpacing(12);

        // 注意：这里不再预先添加 Stretch，而是在数据加载后动态处理

        scrollArea->setWidget(m_scrollContent);

        // 3. 底部状态/加载栏
        m_loadingWidget = new QWidget(this);
        QHBoxLayout *loadingLayout = new QHBoxLayout(m_loadingWidget);
        loadingLayout->setContentsMargins(0, 10, 0, 10);
        m_statusLabel = new QLabel("正在获取版本信息...", m_loadingWidget);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        loadingLayout->addWidget(m_statusLabel);

        mainLayout->addWidget(header);
        mainLayout->addWidget(scrollArea);
        mainLayout->addWidget(m_loadingWidget);

        applyStyles();
    }

    void applyStyles() {
        QString qss = R"(
            QDialog {
                background-color: #f5f7fa;
            }
            QWidget#Header {
                background-color: #ffffff;
                border-bottom: 1px solid #ebeef5;
            }
            QLabel#Title {
                font-size: 20px;
                font-weight: bold;
                color: #303133;
            }
            QWidget#ScrollContent {
                background-color: #f5f7fa;
            }

            /* Section Titles */
            QLabel#SectionTitle {
                font-size: 14px;
                font-weight: bold;
                color: #909399;
                margin-top: 10px;
                margin-bottom: 5px;
            }

            /* --- Version Card Common --- */
            VersionCard {
                background-color: #ffffff;
                border-radius: 8px;
            }
            QLabel#LabelTag {
                font-weight: bold;
                color: #303133;
            }
            QLabel#LabelDate {
                font-size: 12px;
                color: #909399;
                margin-top: 4px;
            }
            QLabel#LabelBody {
                font-size: 13px;
                color: #606266;
                line-height: 1.5;
            }

            /* --- Normal Card Style --- */
            VersionCard[isLatest="false"] {
                border: 1px solid #ebeef5;
            }
            VersionCard[isLatest="false"]:hover {
                border: 1px solid #c6e2ff;
                background-color: #fcfdff;
            }
            VersionCard[isLatest="false"] QLabel#LabelTag {
                font-size: 16px;
            }

            /* --- Latest Card Style (Special Layout) --- */
            VersionCard[isLatest="true"] {
                border: 1px solid #b3d8ff;
                background-color: #ecf5ff; /* 浅蓝色背景强调 */
            }
            VersionCard[isLatest="true"]:hover {
                border: 1px solid #409eff;
            }
            VersionCard[isLatest="true"] QLabel#LabelTag {
                font-size: 22px; /* 更大的版本号 */
                color: #409eff;
            }
            VersionCard[isLatest="true"] QLabel#LabelBody {
                color: #303133; /* 更深的文字颜色 */
            }

            /* Latest Badge */
            QLabel#BadgeLatest {
                background-color: #f56c6c;
                color: white;
                border-radius: 4px;
                padding: 2px 6px;
                font-size: 12px;
                font-weight: bold;
            }

            /* Buttons */
            QPushButton#BtnDownload {
                background-color: #409eff;
                color: white;
                border: none;
                border-radius: 4px;
                font-weight: 500;
            }
            QPushButton#BtnDownload:hover {
                background-color: #66b1ff;
            }
            QPushButton#BtnDownload:pressed {
                background-color: #3a8ee6;
            }
            QPushButton#BtnLink {
                background-color: #f4f4f5;
                color: #909399;
                border: 1px solid #dcdfe6;
                border-radius: 4px;
            }
            QPushButton#BtnLink:hover {
                color: #409eff;
                border-color: #c6e2ff;
                background-color: #ecf5ff;
            }
        )";
        this->setStyleSheet(qss);
    }

    void fetchVersions() {
        QUrl url = QString("https://gitee.com/api/v5/repos/RemotePro/%2/releases?direction=desc&page=1&per_page=10").arg(QSysInfo::productType() == "windows" ? "RemotePro-windows" : "RemotePro-macos");

        QNetworkRequest request(url);

        auto reply = networkAccessManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply](){
            handleResponse(reply);
        });
    }

    void handleResponse(QNetworkReply *reply) {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText("连接失败: " + reply->errorString());
            m_statusLabel->setStyleSheet("color: #f56c6c;");
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        if (array.isEmpty()) {
            m_statusLabel->setText("未找到版本信息");
            return;
        }

        // --- 布局逻辑 ---

        // 1. 提取并处理最新版本 (第一个元素)
        QJsonValue latestVal = array.takeAt(0); // 取出第一个
        if (latestVal.isObject()) {
            // 添加标题 "最新版本"
            QLabel *titleLatest = new QLabel("最新版本", m_scrollContent);
            titleLatest->setObjectName("SectionTitle");
            m_listLayout->addWidget(titleLatest);

            // 添加特殊样式的最新版卡片 (isLatest = true)
            VersionCard *latestCard = new VersionCard(latestVal.toObject(), true);
            connect(latestCard, &VersionCard::startDownload, this, &VersionManagerDialog::downloadFile);
            m_listLayout->addWidget(latestCard);
        }

        // 2. 处理历史版本 (剩余元素)
        if (!array.isEmpty()) {
            m_listLayout->addSpacing(15); // 增加间距

            // 添加标题 "历史版本"
            QLabel *titleHistory = new QLabel("历史版本", m_scrollContent);
            titleHistory->setObjectName("SectionTitle");
            m_listLayout->addWidget(titleHistory);

            for (const QJsonValue &val : array) {
                if (val.isObject()) {
                    // 普通样式 (isLatest = false)
                    VersionCard *card = new VersionCard(val.toObject(), false);
                    connect(card, &VersionCard::startDownload, this, &VersionManagerDialog::downloadFile);
                    m_listLayout->addWidget(card);
                }
            }
        }

        // 最后添加伸缩占位，保证布局靠上
        m_listLayout->addStretch();

        m_loadingWidget->hide();
    }

    void downloadFile(const QString &url, const QString &tagName) {
        QUrl qurl(url);
        QString fileName = qurl.fileName();
        if (fileName.isEmpty()) fileName = QString("update_%1.zip").arg(tagName);

        QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QString filePath = QDir(downloadPath).filePath(fileName);

        auto file = new QFile(filePath);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, "错误", "无法写入文件: " + filePath);
            delete file;;
            return;
        }

        auto progressDialog = new QProgressDialog("正在下载 " + tagName + " ... ", "取消", 0, 100, this);
        progressDialog->setWindowTitle("下载中");
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->setMinimumDuration(0);

        QNetworkRequest request(qurl);

        auto reply = networkAccessManager->get(request);

        connect(progressDialog, &QProgressDialog::canceled, reply, [=](){
            reply->abort();
        });

        connect(reply, &QNetworkReply::downloadProgress, progressDialog,
                [=](qint64 bytesReceived, qint64 bytesTotal) {
                    if (bytesTotal > 0)
                        progressDialog->setValue((int)(bytesReceived * 100 / bytesTotal));
                });

        connect(reply, &QNetworkReply::readyRead, this, [=](){
            file->write(reply->readAll());
        });

        connect(reply, &QNetworkReply::finished, this, [=](){
            delete file;
            progressDialog->deleteLater();
            
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                QFile::remove(filePath);
            } else if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::warning(this, "下载失败", reply->errorString());
                QFile::remove(filePath);
            } else {
                if (!ZipUtils::extractSmart(filePath, ".")) {
                    new ToastWidget("解压失败，请重试");
                }
                else {
                    QFile::remove(filePath);
                    qApp->quit();
                    QProcess::startDetached(qApp->applicationFilePath());
                }
            }

            reply->deleteLater();
        });
    }
};
