#include "email_list_widget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QDateTime>
#include <QItemSelectionModel>
#include <QBrush>

EmailListWidget::EmailListWidget(QWidget* parent) : QWidget(parent), selected_email_id_(-1) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("状态"),
        QStringLiteral("发件人"),
        QStringLiteral("主题"),
        QStringLiteral("时间")
    });

    table_->setColumnWidth(0, 64);
    table_->setColumnWidth(1, 190);
    table_->setColumnWidth(2, 560);
    table_->setColumnWidth(3, 150);

    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->verticalHeader()->setDefaultSectionSize(56);
    table_->setAlternatingRowColors(false);
    table_->setMouseTracking(true);

    layout->addWidget(table_);
    connect(table_, &QTableWidget::cellClicked,
            this, &EmailListWidget::on_cell_clicked);
}

void EmailListWidget::set_emails(const std::vector<Email>& emails) {
    emails_ = emails;
    selected_email_id_ = -1;
    table_->setRowCount(0);

    for (size_t i = 0; i < emails_.size(); ++i) {
        const Email& mail = emails_[i];
        int row = table_->rowCount();
        table_->insertRow(row);
        bool unread = !mail.is_read;

        // 0 — 状态
        QString state_text;
        if (mail.is_flagged) {
            state_text = QStringLiteral("重要");
        } else if (unread) {
            state_text = QStringLiteral("未读");
        } else {
            state_text = QStringLiteral("已读");
        }
        QTableWidgetItem* state = new QTableWidgetItem(state_text);
        state->setTextAlignment(Qt::AlignCenter);
        state->setData(Qt::UserRole, mail.id);
        QFont sf = state->font();
        sf.setPointSize(10);
        sf.setBold(unread || mail.is_flagged);
        state->setFont(sf);
        state->setForeground(mail.is_flagged ? QColor("#B45309") :
                             unread ? QColor("#2563EB") : QColor("#9CA3AF"));
        table_->setItem(row, 0, state);

        // 1 — 发件人
        QString from = mail.sender_name.empty()
            ? QString::fromStdString(mail.sender_addr)
            : QString::fromStdString(mail.sender_name);
        QTableWidgetItem* fi = new QTableWidgetItem(from);
        fi->setData(Qt::UserRole, mail.id);
        if (unread) { QFont f = fi->font(); f.setBold(true); fi->setFont(f); fi->setForeground(QColor("#111827")); }
        else { fi->setForeground(QColor("#4B5563")); }
        table_->setItem(row, 1, fi);

        // 2 — 主题 + 预览
        QString subj = QString::fromStdString(mail.subject.empty() ? "(无主题)" : mail.subject);
        if (!mail.body_plain.empty()) {
            QString preview = QString::fromStdString(mail.body_plain).simplified().left(78);
            subj += "  — " + preview;
        }
        if (mail.has_attachments) subj += QStringLiteral("  [附件]");
        QTableWidgetItem* si = new QTableWidgetItem(subj);
        if (unread) { QFont f = si->font(); f.setBold(true); si->setFont(f); si->setForeground(QColor("#111827")); }
        else { si->setForeground(QColor("#6B7280")); }
        table_->setItem(row, 2, si);

        // 3 — 时间
        QTableWidgetItem* di = new QTableWidgetItem(QString::fromStdString(mail.received_date));
        di->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        di->setForeground(QColor("#9CA3AF"));
        table_->setItem(row, 3, di);

        if (row % 2 == 1) {
            QColor alt("#FBFCFE");
            for (int c = 0; c < 4; ++c)
                if (table_->item(row, c)) table_->item(row, c)->setBackground(alt);
        }
    }
}

int EmailListWidget::current_email_id() const {
    return selected_email_id_;
}

void EmailListWidget::clear() {
    emails_.clear();
    selected_email_id_ = -1;
    table_->setRowCount(0);
}

void EmailListWidget::on_cell_clicked(int row, int /*column*/) {
    if (row >= 0 && row < (int)emails_.size()) {
        selected_email_id_ = emails_[row].id;
        emit email_selected(selected_email_id_);
    }
}
