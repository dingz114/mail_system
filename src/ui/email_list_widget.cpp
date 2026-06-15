#include "email_list_widget.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionButton>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

#include <set>

namespace {

constexpr int kEmailRole = Qt::UserRole;
constexpr int kRowRole = Qt::UserRole + 1;
constexpr int kCheckedRole = Qt::UserRole + 2;  // 自定义复选框状态

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
    explicit EmailItemDelegate(const std::vector<Email>* emails, const bool* multi_select, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), emails_(emails), multi_select_(multi_select) {}

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
        const bool show_checkbox = multi_select_ && *multi_select_;
        const bool checked = index.data(kCheckedRole).toBool();

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
            painter->setPen(QColor("#D5DCE4"));
            painter->drawLine(option.rect.left() + 20, option.rect.bottom(), option.rect.right() - 16, option.rect.bottom());
        }

        // 多选模式下绘制复选框
        int checkbox_width = 0;
        if (show_checkbox) {
            checkbox_width = 36;
            QStyleOptionButton cb_opt;
            cb_opt.rect = QRect(outer.left() + 8, outer.top() + (outer.height() - 20) / 2, 20, 20);
            cb_opt.state = QStyle::State_Enabled | QStyle::State_Active;
            if (checked) {
                cb_opt.state |= QStyle::State_On;
            } else {
                cb_opt.state |= QStyle::State_Off;
            }
            // 悬停时高亮复选框
            if (hovered) {
                cb_opt.state |= QStyle::State_MouseOver;
            }
            QApplication::style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &cb_opt, painter, nullptr);
        }

        QRect content = outer.adjusted(30 + checkbox_width, 10, -16, -10);
        QRect dot_rect(outer.left() + 14 + checkbox_width, outer.top() + (outer.height() - 8) / 2, 8, 8);
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
    const bool* multi_select_;
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
    list_->setItemDelegate(new EmailItemDelegate(&emails_, &multi_select_, list_));

    // 监听滚动条，接近底部时触发加载更多
    connect(list_->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &EmailListWidget::on_scroll_value_changed);

    layout->addWidget(list_);
    connect(list_, &QListWidget::itemClicked, this, &EmailListWidget::on_item_clicked);
    connect(list_, &QListWidget::itemChanged, this, &EmailListWidget::on_item_changed);
}

void EmailListWidget::set_emails(const std::vector<Email>& emails) {
    loading_more_ = false;
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
        item->setData(kCheckedRole, false);
    }
}

void EmailListWidget::append_emails(const std::vector<Email>& emails) {
    if (emails.empty()) {
        loading_more_ = false;
        return;
    }

    int base_index = static_cast<int>(emails_.size());
    for (int i = 0; i < static_cast<int>(emails.size()); ++i) {
        emails_.push_back(emails[i]);
        int row = base_index + i;
        auto* item = new QListWidgetItem(list_);
        item->setData(kEmailRole, emails[i].id);
        item->setData(kRowRole, row);
        item->setSizeHint(QSize(0, 72));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setData(kCheckedRole, false);
    }
    loading_more_ = false;
}

int EmailListWidget::email_count() const {
    return static_cast<int>(emails_.size());
}

int EmailListWidget::current_email_id() const {
    return selected_email_id_;
}

void EmailListWidget::clear() {
    emails_.clear();
    selected_email_id_ = -1;
    list_->clear();
}

void EmailListWidget::set_multi_select_mode(bool enabled) {
    if (multi_select_ == enabled) return;
    multi_select_ = enabled;

    list_->setSelectionMode(enabled
        ? QAbstractItemView::MultiSelection
        : QAbstractItemView::SingleSelection);

    // 切换模式时需要重建列表项以更新 flags 和复选框
    if (!emails_.empty()) {
        rebuild_items();
    }
}

bool EmailListWidget::is_multi_select_mode() const {
    return multi_select_;
}

std::vector<int> EmailListWidget::selected_email_ids() const {
    std::vector<int> ids;
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        if (item && item->data(kCheckedRole).toBool()) {
            ids.push_back(item->data(kEmailRole).toInt());
        }
    }
    return ids;
}

void EmailListWidget::mark_email_read(int email_id) {
    for (auto& email : emails_) {
        if (email.id == email_id) {
            email.is_read = true;
            list_->viewport()->update();
            return;
        }
    }
}

int EmailListWidget::selected_count() const {
    int count = 0;
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        if (item && item->data(kCheckedRole).toBool()) {
            ++count;
        }
    }
    return count;
}

void EmailListWidget::rebuild_items() {
    // 保存当前选中的复选框状态
    std::set<int> checked_ids;
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        if (item && item->data(kCheckedRole).toBool()) {
            checked_ids.insert(item->data(kEmailRole).toInt());
        }
    }

    selected_email_id_ = -1;
    list_->clear();

    for (int i = 0; i < static_cast<int>(emails_.size()); ++i) {
        const Email& mail = emails_[i];
        auto* item = new QListWidgetItem(list_);
        item->setData(kEmailRole, mail.id);
        item->setData(kRowRole, i);
        item->setSizeHint(QSize(0, 72));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setData(kCheckedRole, checked_ids.count(mail.id) ? true : false);
    }
}

void EmailListWidget::on_item_clicked(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    if (multi_select_) {
        // 多选模式：手动切换自定义复选框状态
        bool checked = item->data(kCheckedRole).toBool();
        item->setData(kCheckedRole, !checked);
        // 触发重绘以更新复选框外观
        list_->update(list_->indexFromItem(item));
        emit selection_changed();
        return;
    }
    selected_email_id_ = item->data(kEmailRole).toInt();
    emit email_selected(selected_email_id_);
}

void EmailListWidget::on_item_changed(QListWidgetItem* item) {
    Q_UNUSED(item)
    // 仅在多选模式下响应（重建列表时也会触发，但 selection_changed 信号无害）
    if (multi_select_) {
        emit selection_changed();
    }
}

void EmailListWidget::on_scroll_value_changed(int value) {
    if (loading_more_ || emails_.empty()) return;

    QScrollBar* sb = list_->verticalScrollBar();
    if (!sb || sb->maximum() == 0) return;

    // 当滚动条距离底部不到 2 个屏幕高度时触发加载更多
    int threshold = list_->viewport()->height() * 2;
    if (sb->maximum() - value <= threshold) {
        loading_more_ = true;
        emit load_more_requested();
    }
}
