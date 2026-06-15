#include "email_list_widget.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

namespace {

constexpr int kEmailRole = Qt::UserRole;
constexpr int kRowRole = Qt::UserRole + 1;

QString display_sender(const Email& mail) {
    if (!mail.sender_name.empty()) {
        return QString::fromStdString(mail.sender_name);
    }
    if (!mail.sender_addr.empty()) {
        return QString::fromStdString(mail.sender_addr);
    }
    return QStringLiteral("(未知发件人)");
}

QString display_subject(const Email& mail) {
    QString subject = QString::fromStdString(mail.subject.empty() ? "(无主题)" : mail.subject);
    QString preview = QString::fromStdString(mail.body_plain).simplified();

    if (!preview.isEmpty()) {
        subject += QStringLiteral("  -  ") + preview;
    }
    if (mail.has_attachments) {
        subject += QStringLiteral("  [附件]");
    }
    return subject;
}

QString display_time(const Email& mail) {
    QString raw = QString::fromStdString(mail.received_date).trimmed();
    if (raw.isEmpty()) {
        return QString();
    }

    QDateTime dt = QDateTime::fromString(raw, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(raw, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    if (!dt.isValid()) {
        return raw;
    }

    const QDate today = QDate::currentDate();
    if (dt.date() == today) {
        return dt.toString(QStringLiteral("HH:mm"));
    }
    if (dt.date().year() == today.year()) {
        return dt.toString(QStringLiteral("MM-dd"));
    }
    return dt.toString(QStringLiteral("yyyy-MM-dd"));
}

class EmailItemDelegate : public QStyledItemDelegate {
public:
    explicit EmailItemDelegate(const std::vector<Email>* emails, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), emails_(emails) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(0, 72);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!emails_) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const int row = index.data(kRowRole).toInt();
        if (row < 0 || row >= static_cast<int>(emails_->size())) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const Email& mail = emails_->at(row);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool unread = !mail.is_read;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QRect outer = option.rect.adjusted(8, 3, -8, -3);
        QColor background = Qt::transparent;
        if (selected) {
            background = QColor("#EAF3FF");
        } else if (hovered) {
            background = QColor("#F6F9FC");
        }
        if (background.isValid() && background.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(background);
            painter->drawRoundedRect(outer, 8, 8);
        }

        if (!selected) {
            painter->setPen(QColor("#EEF2F6"));
            painter->drawLine(option.rect.left() + 20, option.rect.bottom(), option.rect.right() - 16, option.rect.bottom());
        }

        QRect content = outer.adjusted(30, 10, -16, -10);
        QRect dot_rect(outer.left() + 14, outer.top() + (outer.height() - 8) / 2, 8, 8);
        if (unread) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#1677FF"));
            painter->drawEllipse(dot_rect);
        }

        QFont sender_font = option.font;
        sender_font.setBold(unread);
        sender_font.setPointSize(10);
        QFont time_font = option.font;
        time_font.setPointSize(9);
        QFont subject_font = option.font;
        subject_font.setPointSize(9);

        QFontMetrics sender_metrics(sender_font);
        QFontMetrics time_metrics(time_font);
        QFontMetrics subject_metrics(subject_font);

        const QString time = display_time(mail);
        const int time_width = qMin(time_metrics.horizontalAdvance(time) + 12, 112);
        QRect time_rect(content.right() - time_width, content.top() + 1, time_width, 20);
        QRect sender_rect(content.left(), content.top(), content.width() - time_width - 12, 22);
        QRect subject_rect(content.left(), content.top() + 28, content.width(), 22);

        painter->setFont(sender_font);
        painter->setPen(unread ? QColor("#17212F") : QColor("#2F3D4C"));
        painter->drawText(sender_rect, Qt::AlignLeft | Qt::AlignVCenter,
                          sender_metrics.elidedText(display_sender(mail), Qt::ElideRight, sender_rect.width()));

        painter->setFont(time_font);
        painter->setPen(unread ? QColor("#1677FF") : QColor("#8A94A6"));
        painter->drawText(time_rect, Qt::AlignRight | Qt::AlignVCenter,
                          time_metrics.elidedText(time, Qt::ElideRight, time_rect.width()));

        painter->setFont(subject_font);
        painter->setPen(QColor("#8A94A6"));
        painter->drawText(subject_rect, Qt::AlignLeft | Qt::AlignVCenter,
                          subject_metrics.elidedText(display_subject(mail), Qt::ElideRight, subject_rect.width()));

        painter->restore();
    }

private:
    const std::vector<Email>* emails_;
};

} // namespace

EmailListWidget::EmailListWidget(QWidget* parent) : QWidget(parent), selected_email_id_(-1) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("emailListView"));
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setMouseTracking(true);
    list_->setUniformItemSizes(false);
    list_->setItemDelegate(new EmailItemDelegate(&emails_, list_));

    layout->addWidget(list_);
    connect(list_, &QListWidget::itemClicked, this, &EmailListWidget::on_item_clicked);
}

void EmailListWidget::set_emails(const std::vector<Email>& emails) {
    emails_ = emails;
    selected_email_id_ = -1;
    list_->clear();

    for (int i = 0; i < static_cast<int>(emails_.size()); ++i) {
        const Email& mail = emails_[i];
        auto* item = new QListWidgetItem(list_);
        item->setData(kEmailRole, mail.id);
        item->setData(kRowRole, i);
        item->setSizeHint(QSize(0, 72));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
}

int EmailListWidget::current_email_id() const {
    return selected_email_id_;
}

void EmailListWidget::clear() {
    emails_.clear();
    selected_email_id_ = -1;
    list_->clear();
}

void EmailListWidget::on_item_clicked(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    selected_email_id_ = item->data(kEmailRole).toInt();
    emit email_selected(selected_email_id_);
}
