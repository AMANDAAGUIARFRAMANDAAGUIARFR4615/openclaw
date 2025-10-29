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
    explicit FileViewer(const QString &filePath, QWidget *parent = nullptr) : QWidget(parent), filePath(filePath)
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
        else if (event->matches(QKeySequence::Save))
            saveFile();
        else
            QWidget::keyPressEvent(event);
    }

    void closeEvent(QCloseEvent *event) override {
        if (!textEdit || !textEdit->document()->isModified()) {
            event->accept();
            return;
        }

        auto btn = QMessageBox::question(this, "保存", "文件已修改，是否保存？");
        
        if (btn == QMessageBox::No) {
            event->accept();
            return;
        }

        saveFile();

        if (textEdit->document()->isModified())
            event->ignore();
        else
            event->accept();
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

            textEdit = new QTextEdit;
            textEdit->setPlainText(QString::fromUtf8(file.readAll()));
            textEdit->document()->setModified(false);
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

    void saveFile() {
        if (!textEdit) return;
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "错误", "无法保存文件。");
            return;
        }
        file.write(textEdit->toPlainText().toUtf8());
        file.close();
        textEdit->document()->setModified(false);
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

    QString filePath;
    QTextEdit *textEdit = nullptr;
};