#ifndef EMAIL_LIST_WIDGET_H
#define EMAIL_LIST_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <vector>
#include "../core/email.h"

class EmailListWidget : public QWidget {
    Q_OBJECT

public:
    explicit EmailListWidget(QWidget* parent = nullptr);

    void set_emails(const std::vector<Email>& emails);
    int  current_email_id() const;
    void clear();

signals:
    void email_selected(int email_id);

private slots:
    void on_selection_changed();

private:
    QTableWidget* table_;
    std::vector<Email> emails_;
};

#endif // EMAIL_LIST_WIDGET_H
