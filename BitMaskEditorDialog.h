#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstdint>
#include <vector>
#include <QString>

class BitMaskEditorDialog : public QDialog
{
    Q_OBJECT
public:
    struct Item {
        int bit;        // 位索引 [0, 31]
        QString name;   // 显示名称
    };

    explicit BitMaskEditorDialog(const std::vector<Item>& items,
                                 uint32_t currentMask,
                                 QWidget* parent = nullptr)
        : QDialog(parent)
        , m_items(items)
        , m_currentMask(currentMask)
    {
        setWindowTitle(tr(""));
        setModal(true);
        setupUi();
    }

    uint32_t mask() const { return m_resultMask; }

private:
    void setupUi()
    {
        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->setSpacing(12);

        auto grid = new QGridLayout;
        grid->setHorizontalSpacing(20);
        grid->setVerticalSpacing(10);
        m_checkBoxes.reserve(m_items.size());

        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& item = m_items[i];
            if (item.bit < 0 || item.bit > 31) continue;

            auto cb = new QCheckBox(item.name, this);
            cb->setChecked(m_currentMask & (1U << item.bit));
            cb->setStyleSheet("QCheckBox { spacing: 8px; }");
            m_checkBoxes.push_back(cb);

            int col = static_cast<int>(i % 4);
            int row = static_cast<int>(i / 4);
            grid->addWidget(cb, row, col);
        }

        mainLayout->addLayout(grid);
        mainLayout->addStretch();

        auto buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确定"));
        buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));

        // auto okBtn = buttonBox->button(QDialogButtonBox::Ok);
        // okBtn->setStyleSheet(R"(
        //     QPushButton {
        //         background-color: #3498db;
        //         color: white;
        //         border: none;
        //         padding: 8px 20px;
        //         border-radius: 4px;
        //         font-weight: bold;
        //         min-width: 80px;
        //     }
        //     QPushButton:hover { background-color: #2980b9; }
        //     QPushButton:pressed { background-color: #1c6ba0; }
        // )");

        mainLayout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &BitMaskEditorDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &BitMaskEditorDialog::reject);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &BitMaskEditorDialog::collectMask);
    }

    void collectMask()
    {
        m_resultMask = m_currentMask;

        for (const auto& item : m_items) {
            if (item.bit >= 0 && item.bit <= 31)
                m_resultMask &= ~(1U << item.bit);
        }

        for (size_t i = 0; i < m_checkBoxes.size(); ++i) {
            if (m_checkBoxes[i]->isChecked()) {
                int bit = m_items[i].bit;
                if (bit >= 0 && bit <= 31)
                    m_resultMask |= (1U << bit);
            }
        }
    }

    const std::vector<Item> m_items;
    uint32_t m_currentMask = 0;
    uint32_t m_resultMask = 0;

    std::vector<QCheckBox*> m_checkBoxes;
};