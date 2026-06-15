#include "email_list_widget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QDateTime>
#include <QItemSelectionModel>

EmailListWidget::EmailListWidget(QWidget* parent) : QWidget(parent), selected_email_id_(-1) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({
        QStringLiteral(""),
        QString(),
        QStringLiteral("发件人"),
        QStringLiteral("主题"),
        QStringLiteral("时间")
    });

    table_->setColumnWidth(0, 36);
    table_->setColumnWidth(1, 28);
    table_->setColumnWidth(2, 160);
    table_->setColumnWidth(3, 460);
    table_->setColumnWidth(4, 140);

    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->verticalHeader()->setDefaultSectionSize(48);

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

        // 0 — 复选框
        QTableWidgetItem* cb = new QTableWidgetItem();
        cb->setFlags(cb->flags() | Qt::ItemIsUserCheckable);
        cb->setCheckState(Qt::Unchecked);
        table_->setItem(row, 0, cb);

        // 1 — 标记
        QTableWidgetItem* mark = new QTableWidgetItem();
        mark->setTextAlignment(Qt::AlignCenter);
        if (mail.is_flagged) {
            mark->setText(QStringLiteral("★"));
            mark->setForeground(QColor("#F5A623"));
        } else if (unread) {
            mark->setText(QStringLiteral("●"));
            mark->setForeground(QColor("#1890FF"));
            QFont f = mark->font(); f.setPointSize(8); mark->setFont(f);
        }
        table_->setItem(row, 1, mark);

        // 2 — 发件人
        QString from = mail.sender_name.empty()
            ? QString::fromStdString(mail.sender_addr)
            : QString::fromStdString(mail.sender_name);
        QTableWidgetItem* fi = new QTableWidgetItem(from);
        fi->setData(Qt::UserRole, mail.id);
        if (unread) { QFont f = fi->font(); f.setBold(true); fi->setFont(f); fi->setForeground(QColor("#1A1A1A")); }
        else { fi->setForeground(QColor("#666666")); }
        table_->setItem(row, 2, fi);

        // 3 — 主题 + 预览
        QString subj = QString::fromStdString(mail.subject.empty() ? "(无主题)" : mail.subject);
        if (!mail.body_plain.empty()) {
            QString preview = QString::fromStdString(mail.body_plain).left(60).replace('\n', ' ');
            subj += "  — " + preview;
        }
        if (mail.has_attachments) subj += QStringLiteral("  📎");
        QTableWidgetItem* si = new QTableWidgetItem(subj);
        if (unread) { QFont f = si->font(); f.setBold(true); si->setFont(f); si->setForeground(QColor("#1A1A1A")); }
        else { si->setForeground(QColor("#888888")); }
        table_->setItem(row, 3, si);

        // 4 — 时间
        QTableWidgetItem* di = new QTableWidgetItem(QString::fromStdString(mail.received_date));
        di->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        di->setForeground(QColor("#AAAAAA"));
        table_->setItem(row, 4, di);

        if (row % 2 == 1) {
            QColor alt("#F8F9FB");
            for (int c = 0; c < 5; ++c)
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
