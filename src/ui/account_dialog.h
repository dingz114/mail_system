#ifndef ACCOUNT_DIALOG_H
#define ACCOUNT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include "../core/email.h"
#include "../database/db_manager.h"

class AccountDialog : public QDialog {
    Q_OBJECT

public:
    explicit AccountDialog(DbManager* db_mgr, QWidget* parent = nullptr);

    // For editing an existing account
    void set_account(const Account& acc);

    // Get the account entered by the user
    Account get_account() const;

private slots:
    void on_provider_changed(int index);
    void on_test_connection();
    void on_save();

private:
    DbManager* db_mgr_;

    QLineEdit*  name_edit_;
    QLineEdit*  email_edit_;
    QLineEdit*  username_edit_;
    QLineEdit*  password_edit_;
    QComboBox*  provider_combo_;
    QLineEdit*  smtp_server_edit_;
    QSpinBox*   smtp_port_spin_;
    QCheckBox*  smtp_ssl_check_;
    QLineEdit*  pop3_server_edit_;
    QSpinBox*   pop3_port_spin_;
    QCheckBox*  pop3_ssl_check_;
    QCheckBox*  leave_on_server_check_;
    QPushButton* test_btn_;
    QPushButton* save_btn_;
    QPushButton* cancel_btn_;

    int edit_id_;  // -1 for new account
};

#endif // ACCOUNT_DIALOG_H
