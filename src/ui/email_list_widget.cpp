#include "email_list_widget.h"
#include "../core/mime_decoder.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

#include <functional>
#include <set>

namespace {

constexpr int kEmailRole = Qt::UserRole;
constexpr int kRowRole = Qt::UserRole + 1;
constexpr int kCheckedRole = Qt::UserRole + 2;  // 自定义复选框状态

QString replace_regex(QString input,
                      const QRegularExpression& re,
                      const std::function<QString(const QRegularExpressionMatch&)>& replacer) {
    QString result;
    int offset = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(input);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result += input.mid(offset, match.capturedStart() - offset);
        result += replacer(match);
        offset = match.capturedEnd();
    }
    result += input.mid(offset);
    return result;
}

QString decode_display_entities(QString text) {
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));

    QRegularExpression entity_re(QStringLiteral("&#(x[0-9A-Fa-f]+|[0-9]+);"));
    return replace_regex(text, entity_re, [](const QRegularExpressionMatch& match) {
        QString raw = match.captured(1);
        bool ok = false;
        uint codepoint = 0;
        if (raw.startsWith(QLatin1Char('x'), Qt::CaseInsensitive)) {
            codepoint = raw.mid(1).toUInt(&ok, 16);
        } else {
            codepoint = raw.toUInt(&ok, 10);
        }
        if (!ok || codepoint > 0x10FFFF) {
            return match.captured(0);
        }
        char32_t cp = static_cast<char32_t>(codepoint);
        return QString::fromUcs4(&cp, 1);
    });
}

QString display_sender(const Email& mail) {
    if (!mail.sender_name.empty()) {
        MimeDecoder decoder;
        return decode_display_entities(
            QString::fromStdString(decoder.decode_rfc2047(mail.sender_name)));
    }
    if (!mail.sender_addr.empty()) {
        return QString::fromStdString(mail.sender_addr);
    }
    return QStringLiteral("(未知发件人)");
}

QString display_subject(const Email& mail) {
    MimeDecoder decoder;
    QString subject = mail.subject.empty()
        ? QStringLiteral("(无主题)")
        : decode_display_entities(
              QString::fromStdString(decoder.decode_rfc2047(mail.subject)));
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
        return QSize(0, 76);
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

        QRect outer = option.rect.adjusted(12, 5, -12, -5);
        QColor background("#FFFFFF");
        QColor border("#EDF2F7");
        if (checked) {
            background = QColor("#F0F8FF");
            border = QColor("#B9DDF8");
        } else if (selected) {
            background = QColor("#F6FBFF");
            border = QColor("#CFE7FA");
        } else if (hovered) {
            background = QColor("#F8FBFD");
            border = QColor("#DDEAF3");
        }
        painter->setPen(QPen(border, 1));
        painter->setBrush(background);
        painter->drawRoundedRect(outer, 8, 8);

        // 多选模式下绘制复选框
        int checkbox_width = 0;
        if (show_checkbox) {
            checkbox_width = 40;
            QRectF box(outer.left() + 14, outer.top() + (outer.height() - 18) / 2.0, 18, 18);
            painter->setPen(QPen(checked ? QColor("#2F80ED") : QColor("#B8C4D2"), 1.4));
            painter->setBrush(checked ? QColor("#2F80ED")
                                      : (hovered ? QColor("#F1F6FB") : QColor("#FFFFFF")));
            painter->drawRoundedRect(box, 5, 5);
            if (checked) {
                QPainterPath check;
                check.moveTo(box.left() + 4.2, box.top() + 9.4);
                check.lineTo(box.left() + 7.5, box.top() + 12.4);
                check.lineTo(box.left() + 14.0, box.top() + 5.6);
                painter->setPen(QPen(QColor("#FFFFFF"), 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->drawPath(check);
            }
        }

        QRect content = outer.adjusted(22 + checkbox_width, 10, -18, -10);
        QRect dot_rect(outer.left() + 12 + checkbox_width, outer.top() + (outer.height() - 8) / 2, 7, 7);
        if (unread) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#21A8A3"));
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
        painter->setPen(unread ? QColor("#102033") : QColor("#344256"));
        painter->drawText(sender_rect, Qt::AlignLeft | Qt::AlignVCenter,
                          sender_metrics.elidedText(display_sender(mail), Qt::ElideRight, sender_rect.width()));

        painter->setFont(time_font);
        painter->setPen(unread ? QColor("#2F80ED") : QColor("#8A97A8"));
        painter->drawText(time_rect, Qt::AlignRight | Qt::AlignVCenter,
                          time_metrics.elidedText(time, Qt::ElideRight, time_rect.width()));

        painter->setFont(subject_font);
        painter->setPen(unread ? QColor("#66758A") : QColor("#8A97A8"));
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
    list_->setStyleSheet(
        "QListWidget#emailListView { background: #FAFCFE; padding: 8px 0; }"
        "QListWidget#emailListView::item { border: none; background: transparent; }"
        "QListWidget#emailListView::item:selected { background: transparent; }");
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
        item->setSizeHint(QSize(0, 76));
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
        item->setSizeHint(QSize(0, 76));
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

void EmailListWidget::mark_emails_read(const std::vector<int>& email_ids) {
    std::set<int> ids(email_ids.begin(), email_ids.end());
    for (auto& email : emails_) {
        if (ids.count(email.id)) {
            email.is_read = true;
        }
    }
    list_->viewport()->update();
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

bool EmailListWidget::all_selected() const {
    const int count = list_->count();
    return count > 0 && selected_count() == count;
}

void EmailListWidget::set_all_checked(bool checked) {
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        if (item) {
            item->setData(kCheckedRole, checked);
        }
    }
    list_->clearSelection();
    list_->viewport()->update();
    emit selection_changed();
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
        item->setSizeHint(QSize(0, 76));
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
        list_->clearSelection();
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
