#ifndef EMAIL_LIST_WIDGET_H
#define EMAIL_LIST_WIDGET_H

#include <QWidget>
#include <QListWidget>
#include <vector>
#include "../core/email.h"

class EmailListWidget : public QWidget {
    Q_OBJECT

public:
    explicit EmailListWidget(QWidget* parent = nullptr);

    void set_emails(const std::vector<Email>& emails);
    void append_emails(const std::vector<Email>& emails);  // 增量追加
    int  current_email_id() const;
    int  email_count() const;
    void clear();

    // 批量选中
    void set_multi_select_mode(bool enabled);
    bool is_multi_select_mode() const;
    std::vector<int> selected_email_ids() const;
    int  selected_count() const;
    bool all_selected() const;
    void set_all_checked(bool checked);

    // 标记单封邮件为已读（更新内存中的状态）
    void mark_email_read(int email_id);
    void mark_emails_read(const std::vector<int>& email_ids);

signals:
    void email_selected(int email_id);
    void selection_changed();     // 多选状态变化
    void load_more_requested();   // 滚动到底部，请求加载更多

private slots:
    void on_item_clicked(QListWidgetItem* item);
    void on_item_changed(QListWidgetItem* item);
    void on_scroll_value_changed(int value);

private:
    void rebuild_items();  // 重建列表项（切换多选模式时）

    QListWidget* list_;
    std::vector<Email> emails_;
    int selected_email_id_ = -1;
    bool multi_select_ = false;
    bool loading_more_ = false;   // 防止重复触发
};

#endif // EMAIL_LIST_WIDGET_H
