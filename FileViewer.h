#pragma once

#include <QWidget>
#include <QFileDialog>
#include <QFile>
#include <QPlainTextEdit>
#include <QLabel>
#include <QMessageBox>
#include <QImageReader>
#include <QVBoxLayout>
#include <QPixmap>
#include <QFileInfo>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QPainter>
#include <QTextBlock>

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr) : QPlainTextEdit(parent) {
        lineNumberArea = new LineNumberArea(this);

        // QFont font;
        // font.setFamily("Consolas"); // Windows常用等宽字体，也可选 Courier New
        // font.setStyleHint(QFont::Monospace);
        // font.setFixedPitch(true);
        // font.setPointSize(10);
        // setFont(font);

        connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
        connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
        connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

        updateLineNumberAreaWidth(0);
        highlightCurrentLine();
    }

    void lineNumberAreaPaintEvent(QPaintEvent *event) {
        QPainter painter(lineNumberArea);
        painter.fillRect(event->rect(), QColor(240, 240, 240));

        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
        int bottom = top + (int) blockBoundingRect(block).height();

        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                QString number = QString::number(blockNumber + 1);
                // 文字颜色：深灰色
                painter.setPen(QColor(120, 120, 120));
                // 绘制文字，右对齐
                painter.drawText(0, top, lineNumberArea->width() - 5, fontMetrics().height(),
                                 Qt::AlignRight, number);
            }

            block = block.next();
            top = bottom;
            bottom = top + (int) blockBoundingRect(block).height();
            ++blockNumber;
        }
        
        // 绘制右侧分割线
        painter.setPen(QColor(220, 220, 220));
        painter.drawLine(lineNumberArea->width() - 1, event->rect().top(), 
                         lineNumberArea->width() - 1, event->rect().bottom());
    }

    int lineNumberAreaWidth() {
        int digits = 1;
        int max = qMax(1, blockCount());
        while (max >= 10) {
            max /= 10;
            ++digits;
        }
        // 这里的 15 是左右内边距 padding
        int space = 15 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
        return space;
    }

protected:
    void resizeEvent(QResizeEvent *e) override {
        QPlainTextEdit::resizeEvent(e);
        QRect cr = contentsRect();
        lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    }

private slots:
    void updateLineNumberAreaWidth(int /* newBlockCount */) {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    }

    void updateLineNumberArea(const QRect &rect, int dy) {
        if (dy)
            lineNumberArea->scroll(0, dy);
        else
            lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

        if (rect.contains(viewport()->rect()))
            updateLineNumberAreaWidth(0);
    }

    void highlightCurrentLine() {
        QList<QTextEdit::ExtraSelection> extraSelections;

        if (!isReadOnly()) {
            QTextEdit::ExtraSelection selection;
            QColor lineColor = QColor(128, 128, 128).lighter(160);
            selection.format.setBackground(lineColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            extraSelections.append(selection);
        }

        setExtraSelections(extraSelections);
    }

private:
    QWidget *lineNumberArea;

    class LineNumberArea : public QWidget {
    public:
        LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}
        QSize sizeHint() const override { return QSize(codeEditor->lineNumberAreaWidth(), 0); }
    protected:
        void paintEvent(QPaintEvent *event) override { codeEditor->lineNumberAreaPaintEvent(event); }
    private:
        CodeEditor *codeEditor;
    };
};

class FileViewer : public QWidget
{
    Q_OBJECT

public:
    explicit FileViewer(const QString &filePath, QWidget *parent) : filePath(filePath)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        
        setWindowTitle(QFileInfo(filePath).fileName());
        show();

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        setLayout(layout);

        if (isTextFile()) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, "错误", "无法打开文本文件。");
                return;
            }

            editor = new CodeEditor(this);
            editor->setPlainText(QString::fromUtf8(file.readAll()));
            editor->document()->setModified(false);
            layout->addWidget(editor);
        }
        else if (isImageFile()) {
            QPixmap pixmap(filePath);
            if (pixmap.isNull()) {
                QMessageBox::warning(this, "错误", "无法加载图片。");
                return;
            }

            QLabel *imageLabel = new QLabel(this);
            imageLabel->setAlignment(Qt::AlignCenter);
           
            QSize maxSize = qApp->primaryScreen()->availableSize();
            if (pixmap.width() > maxSize.width() || pixmap.height() > maxSize.height())
                imageLabel->setPixmap(pixmap.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else
                imageLabel->setPixmap(pixmap);
    
            layout->addWidget(imageLabel);
            resize(pixmap.size());
        }
        else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            deleteLater();
            return;
        }

        QRect geometry = parent ? parent->window()->geometry() : qApp->primaryScreen()->availableGeometry();
        int x = (geometry.width() - width()) / 2;
        int y = (geometry.height() - height()) / 2;
        move(x, y);
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
        if (!editor || !editor->document()->isModified()) {
            event->accept();
            return;
        }

        auto button = QMessageBox::question(this, "保存", "文件已修改，是否保存？", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (button == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        
        if (button == QMessageBox::No) {
            event->accept();
            return;
        }

        saveFile();

        if (editor->document()->isModified())
            event->ignore();
        else
            event->accept();
    }

    void saveFile() {
        if (!editor) return;
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "错误", "无法保存文件。");
            return;
        }
        file.write(editor->toPlainText().toUtf8());
        file.close();
        editor->document()->setModified(false);
    }

    bool isTextFile()
    {
        static const QStringList textExtensions = {"txt", "cpp", "h", "hpp", "c", "json", "md", "ini", "log", "csv", "xml", "recordx"};
        return textExtensions.contains(QFileInfo(filePath).suffix().toLower());
    }

    bool isImageFile()
    {
        static const QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "webp"};
        return imageExtensions.contains(QFileInfo(filePath).suffix().toLower());
    }

    QString filePath;
    CodeEditor *editor = nullptr;
};
