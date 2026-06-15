#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>

#include "../core/email.h"
#include "../database/db_manager.h"
#include "../network/smtp_client.h"
#include "../network/pop3_client.h"

class EmailListWidget;
class EmailViewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(DbManager* db_mgr, QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void on_receive();
    void on_compose();
    void on_reply();
    void on_delete_mail();
    void on_restore_mail();
    void on_account_changed(int index);
    void on_folder_changed();
    void on_email_selected(int email_id);
    void on_send_draft(int draft_id, const Email& updated);
    void on_refresh();
    void on_toggle_multi_select();
    void on_batch_delete();
    void on_batch_restore();
    void on_selection_changed();
    void on_load_more();

private:
    void setup_ui();
    void setup_toolbar();
    void setup_menu();
    void load_accounts();
    void load_emails(const std::string& folder);
    void update_folder_counts();
    void update_batch_buttons();
    void show_email_list_page();

    DbManager* db_mgr_;

    // Toolbar
    QToolBar* toolbar_;
    QComboBox* account_combo_;
    QPushButton* btn_receive_;
    QPushButton* btn_compose_;
    QPushButton* btn_reply_;
    QPushButton* btn_delete_;
    QPushButton* btn_restore_;
    QPushButton* btn_refresh_;
    QPushButton* btn_multi_select_;

    // Batch action bar
    QWidget* batch_bar_;
    QLabel* batch_count_label_;
    QPushButton* btn_batch_delete_;
    QPushButton* btn_batch_restore_;
    QPushButton* btn_batch_cancel_;

    // Folder tree
    QTreeWidget* folder_tree_;
    QTreeWidgetItem* inbox_item_;
    QTreeWidgetItem* sent_item_;
    QTreeWidgetItem* drafts_item_;
    QTreeWidgetItem* trash_item_;

    // Email list & view
    EmailListWidget* email_list_;
    EmailViewWidget* email_view_;
    QStackedWidget* mail_stack_;
    QWidget* list_page_;
    QWidget* detail_page_;
    QPushButton* btn_back_to_list_;

    // Status
    QLabel* status_label_;

    // Current state
    std::vector<Account> accounts_;
    int current_account_id_;
    std::string current_folder_;

    // 分页加载
    static constexpr int kPageSize = 50;
    int current_offset_ = 0;
    int total_count_ = 0;
};

#endif // MAIN_WINDOW_H
