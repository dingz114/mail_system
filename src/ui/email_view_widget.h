#ifndef EMAIL_VIEW_WIDGET_H
#define EMAIL_VIEW_WIDGET_H

#include <QWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QVBoxLayout>
#include "../core/email.h"

class EmailViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit EmailViewWidget(QWidget* parent = nullptr);

    void show_email(const Email& email);
    void clear();

private:
    QLabel*       header_label_;
    QTextBrowser* body_browser_;
    QWidget*      attachments_widget_;
};

#endif // EMAIL_VIEW_WIDGET_H
