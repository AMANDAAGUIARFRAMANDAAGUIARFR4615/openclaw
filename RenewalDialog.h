#ifndef RENEWALDIALOG_H
#define RENEWALDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QDateTime>
#include <QList>
#include <QString>

struct DeviceInfo {
    QString id;
    QString name;
    QDateTime expireTime;
};

class RenewalDialog : public QDialog {
    Q_OBJECT

public:
    explicit RenewalDialog(const QList<DeviceInfo>& devices, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        // 1. 窗口基础设置
        setWindowTitle(tr("设备续费列表"));
        resize(650, 500);
        
        // 设置窗口背景色，避免默认灰色
        setStyleSheet("QDialog { background-color: #ffffff; }");

        // 2. 布局初始化 (增加边距)
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20); // 上下左右留白
        layout->setSpacing(15);                     // 控件间距

        m_tableWidget = new QTableWidget(this);
        
        // 3. 美化表格属性
        m_tableWidget->setColumnCount(3);
        m_tableWidget->setHorizontalHeaderLabels({tr(""), tr("设备名称"), tr("到期时间")});
        
        m_tableWidget->verticalHeader()->setVisible(false);       // 隐藏行号
        m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows); // 选中整行
        m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 禁止编辑
        m_tableWidget->setAlternatingRowColors(true);             // 开启斑马纹(隔行变色)
        m_tableWidget->setShowGrid(false);                        // 隐藏网格线(用样式表控制更美观)
        m_tableWidget->setFocusPolicy(Qt::NoFocus);               // 去除点击时的虚线框
        m_tableWidget->setFrameShape(QFrame::NoFrame);            // 去除表格周围的凹陷边框

        // 4. 应用 CSS 样式表 (核心美化部分)
        QString tableStyle = R"(
            QTableWidget {
                background-color: #ffffff;
                alternate-background-color: #f9f9f9; /* 偶数行浅灰 */
                border: 1px solid #e0e0e0;
                font-size: 13px;
            }
            QTableWidget::item {
                padding-left: 5px;
                border-bottom: 1px solid #f0f0f0; /* 每行下面加一条细线 */
            }
            QTableWidget::item:selected {
                background-color: #e6f7ff; /* 选中时淡蓝色背景 */
                color: #000000;            /* 选中时文字颜色 */
            }
            QHeaderView::section {
                background-color: #f5f5f5; /* 表头浅灰背景 */
                border: none;
                border-bottom: 1px solid #d0d0d0;
                padding: 8px;
                font-weight: bold;
                color: #555555;
            }
            QCheckBox { 
                spacing: 5px; 
            }
        )";
        m_tableWidget->setStyleSheet(tableStyle);

        // 5. 列宽优化
        QHeaderView *header = m_tableWidget->horizontalHeader();
        header->setSectionResizeMode(0, QHeaderView::Fixed); // 复选框列固定宽度
        header->resizeSection(0, 40);                        // 宽度设为40
        header->setSectionResizeMode(1, QHeaderView::Stretch); // 设备名拉伸
        header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // 时间列自适应
        header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 表头左对齐

        layout->addWidget(m_tableWidget);

        // 6. 按钮组
        m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        // 美化按钮
        m_buttonBox->setStyleSheet(R"(
            QPushButton {
                background-color: #f0f0f0; 
                border: 1px solid #d0d0d0; 
                border-radius: 4px; 
                padding: 6px 16px;
                min-width: 60px;
            }
            QPushButton:hover { background-color: #e0e0e0; }
            QPushButton[text="OK"] { /* 针对OK按钮特殊处理(如果不生效可能需要用objectName) */
                background-color: #1890ff; 
                color: white; 
                border: 1px solid #1890ff;
            }
            QPushButton[text="OK"]:hover { background-color: #40a9ff; }
        )");
        
        layout->addWidget(m_buttonBox);

        // 7. 填充数据
        m_tableWidget->setSortingEnabled(false);
        m_tableWidget->setRowCount(devices.size());

        for (int i = 0; i < devices.size(); ++i) {
            const DeviceInfo &info = devices[i];

            // 复选框列
            QTableWidgetItem *checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Checked);
            checkItem->setData(Qt::UserRole, info.id);
            checkItem->setTextAlignment(Qt::AlignCenter); // 复选框居中
            m_tableWidget->setItem(i, 0, checkItem);

            // 设备名
            QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);
            m_tableWidget->setItem(i, 1, nameItem);

            // 时间列
            QString timeStr = info.expireTime.toString("yyyy-MM-dd HH:mm:ss");
            QTableWidgetItem *timeItem = new QTableWidgetItem(timeStr);
            m_tableWidget->setItem(i, 2, timeItem);
            
            // 设置每一行的高度，避免太挤
            m_tableWidget->setRowHeight(i, 38); 
        }

        m_tableWidget->setSortingEnabled(true);
        m_tableWidget->sortItems(2, Qt::AscendingOrder);

        connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    QList<QString> getSelectedDeviceIds() const {
        QList<QString> result;
        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
            if (m_tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                result.append(m_tableWidget->item(i, 0)->data(Qt::UserRole).toString());
            }
        }
        return result;
    }

private:
    QTableWidget *m_tableWidget;
    QDialogButtonBox *m_buttonBox;
};

#endif // RENEWALDIALOG_H