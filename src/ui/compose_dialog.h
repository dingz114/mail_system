#ifndef COMPOSE_DIALOG_H
#define COMPOSE_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include "../core/email.h"
#include "../database/db_manager.h"

class ComposeDialog : public QDialog {
    Q_OBJECT

public:
    explicit ComposeDialog(DbManager* db_mgr, int account_id, QWidget* parent = nullptr);

    // Pre-populate fields (for reply/forward)
    void set_reply_mode(const Email& original_email, bool reply_all = false);
    void set_forward_mode(const Email& original_email);
    void load_draft(const Email& draft);

    // Get the composed email
    Email get_email() const;

private slots:
    void on_send();
    void on_save_draft();
    void on_attach();
    void on_remove_attachment();

private:
    DbManager* db_mgr_;
    int account_id_;

    QComboBox*  from_combo_;
    QLineEdit*  to_edit_;
    QLineEdit*  cc_edit_;
    QLineEdit*  bcc_edit_;
    QLineEdit*  subject_edit_;
    QTextEdit*  body_edit_;
    QListWidget* attach_list_;
    QPushButton* send_btn_;
    QPushButton* draft_btn_;
    QPushButton* attach_btn_;

    std::vector<Attachment> attachments_;
    int draft_id_;  // -1 if not a draft
};

#endif // COMPOSE_DIALOG_H
