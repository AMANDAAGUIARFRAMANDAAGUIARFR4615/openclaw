#pragma once

#include <QWidget>
#include <QFileDialog>
#include <QFile>
#include <QTextEdit>
#include <QLabel>
#include <QMessageBox>
#include <QImageReader>
#include <QVBoxLayout>
#include <QPixmap>
#include <QFileInfo>
#include <QKeyEvent>

class FileViewer : public QWidget
{
    Q_OBJECT

public:
    explicit FileViewer(const QString &filePath, QWidget *parent = nullptr) : QWidget(parent)
    {
        setWindowTitle("文件预览器");
        resize(parent ? parent->size() : QSize(800, 600));
        show();

        openFile(filePath);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

    void openFile(const QString &fileName)
    {
        QVBoxLayout *layout = new QVBoxLayout(this);

        if (isTextFile(fileName)) {
            QFile file(fileName);
            if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, "错误", "无法打开文本文件。");
                return;
            }

            QTextEdit *textEdit = new QTextEdit;
            textEdit->setPlainText(QString::fromUtf8(file.readAll()));
            layout->addWidget(textEdit);
        }
        else if (isImageFile(fileName)) {
            QPixmap pixmap(fileName);
            if (pixmap.isNull()) {
                QMessageBox::warning(this, "错误", "无法加载图片。");
                return;
            }

            QLabel *imageLabel = new QLabel;
            imageLabel->setAlignment(Qt::AlignCenter);
            imageLabel->setPixmap(pixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            layout->addWidget(imageLabel);
        }
        else {
            QMessageBox::information(this, "不支持的文件", "不支持该文件类型。");
            return;
        }

        setLayout(layout);
    }

    bool isTextFile(const QString &fileName)
    {
        static const QStringList textExtensions = {"txt", "cpp", "h", "json", "md", "ini", "log", "csv", "recordx"};
        return textExtensions.contains(QFileInfo(fileName).suffix().toLower());
    }

    bool isImageFile(const QString &fileName)
    {
        static const QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "webp"};
        return imageExtensions.contains(QFileInfo(fileName).suffix().toLower());
    }
};
