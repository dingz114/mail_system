#include "main_window.h"
#include "email_list_widget.h"
#include "email_view_widget.h"
#include "compose_dialog.h"
#include "account_dialog.h"
#include "../core/mime_encoder.h"
#include "../core/mime_decoder.h"

#include <ctime>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QThread>
#include <QScreen>
#include <QFrame>

// ============================================================
//  QQ 邮箱风格全局样式表
// ============================================================
static const char* GLOBAL_STYLE = R"(
    QMainWindow {
        background-color: #F2F4F7;
    }
    QWidget {
        font-family: "Microsoft YaHei", "PingFang SC", "Microsoft YaHei UI", sans-serif;
        font-size: 14px;
        color: #333333;
    }

    /* --- 菜单 --- */
    QMenuBar {
        background: #FFFFFF;
        border-bottom: 1px solid #E8EAED;
        padding: 2px 12px;
        font-size: 13px;
    }
    QMenuBar::item { padding: 6px 10px; border-radius: 2px; }
    QMenuBar::item:selected { background: #F0F2F5; }
    QMenu {
        background: #FFFFFF;
        border: 1px solid #DADCE0;
        border-radius: 4px;
        padding: 4px 0;
    }
    QMenu::item {
        padding: 8px 36px 8px 16px;
    }
    QMenu::item:selected { background: #F0F2F5; }
    QMenu::separator { height: 1px; background: #E8EAED; margin: 4px 0; }

    /* --- 顶部工具栏 — QQ邮箱蓝条 --- */
    QToolBar {
        background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
            stop:0 #1A8CF0, stop:1 #1890FF);
        border: none;
        padding: 0 16px;
        spacing: 12px;
        min-height: 50px;
    }

    /* 工具栏按钮 */
    QPushButton {
        background: transparent;
        color: #555555;
        border: 1px solid #D0D5DD;
        border-radius: 4px;
        padding: 7px 18px;
        font-size: 13px;
    }
    QPushButton:hover {
        background: #F5F7FA;
        border-color: #B0B8C1;
    }

    /* 蓝底主按钮 (写信) */
    QPushButton#primaryBtn {
        background: #1890FF;
        color: #FFFFFF;
        border: none;
        border-radius: 4px;
        padding: 8px 24px;
        font-size: 14px;
        font-weight: 500;
    }
    QPushButton#primaryBtn:hover { background: #40A9FF; }
    QPushButton#primaryBtn:pressed { background: #096DD9; }

    /* 危险按钮 */
    QPushButton#dangerBtn {
        background: transparent;
        color: #E54545;
        border: 1px solid #E54545;
        border-radius: 4px;
        padding: 7px 18px;
    }
    QPushButton#dangerBtn:hover { background: #FFF0F0; }

    /* 顶部栏按钮（白色透明） */
    QPushButton#toolBtn {
        background: rgba(255,255,255,0.15);
        color: #FFFFFF;
        border: none;
        border-radius: 3px;
        padding: 6px 16px;
        font-size: 13px;
    }
    QPushButton#toolBtn:hover { background: rgba(255,255,255,0.3); }

    /* --- 下拉框 --- */
    QComboBox {
        background: #FFFFFF;
        border: 1px solid #D0D5DD;
        border-radius: 4px;
        padding: 5px 30px 5px 10px;
        font-size: 13px;
        min-width: 120px;
    }
    QComboBox:hover { border-color: #1890FF; }
    QComboBox::drop-down { border: none; width: 22px; }
    QComboBox QAbstractItemView {
        border: 1px solid #DADCE0;
        border-radius: 4px;
        padding: 2px;
        selection-background-color: #E8F0FE;
        selection-color: #1A73E8;
    }

    /* --- 输入框 --- */
    QLineEdit, QTextEdit, QPlainTextEdit {
        border: 1px solid #D0D5DD;
        border-radius: 4px;
        padding: 7px 10px;
        background: #FFFFFF;
        font-size: 14px;
    }
    QLineEdit:focus, QTextEdit:focus {
        border-color: #1890FF;
        outline: none;
    }

    /* --- 左侧文件夹树 --- */
    QTreeWidget {
        background: #F5F6FA;
        border: none;
        font-size: 14px;
        outline: none;
    }
    QTreeWidget::item {
        padding: 10px 20px;
        border-radius: 0;
        color: #444444;
    }
    QTreeWidget::item:selected {
        background: #D6E9FF;
        color: #1A73E8;
        font-weight: 600;
    }
    QTreeWidget::item:hover:!selected {
        background: #EAEDF2;
    }

    /* --- 邮件列表表格 --- */
    QTableWidget {
        background: #FFFFFF;
        border: none;
        gridline-color: #F2F2F2;
        font-size: 14px;
        outline: none;
    }
    QTableWidget::item {
        padding: 12px 16px;
        border-bottom: 1px solid #F0F1F3;
    }
    QTableWidget::item:selected {
        background: #E8F0FE;
        color: #333333;
    }
    QHeaderView::section {
        background: #F8F9FB;
        border: none;
        border-bottom: 2px solid #E4E7ED;
        padding: 10px 16px;
        font-weight: 600;
        font-size: 12px;
        color: #8B8E94;
    }

    /* --- 分割器 --- */
    QSplitter::handle {
        background: #E4E7ED;
    }
    QSplitter::handle:horizontal { width: 1px; }
    QSplitter::handle:vertical { height: 1px; }

    /* --- 状态栏 --- */
    QStatusBar {
        background: #FFFFFF;
        border-top: 1px solid #E8EAED;
        padding: 3px 16px;
        font-size: 12px;
        color: #999999;
    }

    /* --- 分组框 --- */
    QGroupBox {
        font-weight: 600;
        border: 1px solid #E4E7ED;
        border-radius: 6px;
        margin-top: 12px;
        padding: 16px 14px 12px 14px;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        left: 16px;
        padding: 0 8px;
        color: #333333;
    }

    /* --- 复选框 --- */
    QCheckBox { spacing: 8px; color: #333; }
    QCheckBox::indicator {
        width: 16px; height: 16px;
        border: 2px solid #C0C4CC;
        border-radius: 2px;
    }
    QCheckBox::indicator:checked {
        background: #1890FF;
        border-color: #1890FF;
    }

    /* --- 滚动条 --- */
    QScrollBar:vertical {
        background: transparent;
        width: 8px;
        margin: 0;
    }
    QScrollBar::handle:vertical {
        background: #C0C4CC;
        border-radius: 4px;
        min-height: 36px;
    }
    QScrollBar::handle:vertical:hover { background: #A0A4AC; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

    /* --- 对话框 --- */
    QDialog {
        background: #F8F9FB;
    }
)";

MainWindow::MainWindow(DbManager* db_mgr, QWidget* parent)
    : QMainWindow(parent), db_mgr_(db_mgr), current_account_id_(-1), current_folder_("inbox") {
    qApp->setStyleSheet(QLatin1String(GLOBAL_STYLE));
    setup_ui();
    setup_toolbar();
    setup_menu();
    load_accounts();

    setWindowTitle(QStringLiteral("QQ邮箱风格邮件系统"));
    resize(1260, 780);
    QRect screen = QApplication::primaryScreen()->availableGeometry();
    move((screen.width() - width()) / 2, (screen.height() - height()) / 2);
    status_label_->setText(QStringLiteral("就绪"));
}

MainWindow::~MainWindow() {}

void MainWindow::setup_ui() {
    QWidget* central = new QWidget(this);
    central->setStyleSheet("background-color: #F2F4F7;");
    setCentralWidget(central);
    QHBoxLayout* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ========== QQ邮箱风格左侧导航栏 ==========
    QWidget* left_panel = new QWidget(this);
    left_panel->setFixedWidth(200);
    left_panel->setStyleSheet("background: #F5F6FA; border-right: 1px solid #E0E2E6;");
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(0);

    // 写信按钮
    QWidget* compose_area = new QWidget(left_panel);
    compose_area->setStyleSheet("padding: 20px 16px 12px 16px;");
    QHBoxLayout* cl = new QHBoxLayout(compose_area);
    cl->setContentsMargins(0, 0, 0, 0);
    QPushButton* compose_btn = new QPushButton(QStringLiteral("✏️  写 信"), left_panel);
    compose_btn->setStyleSheet(
        "QPushButton { background: #1890FF; color: #FFFFFF; border: none; border-radius: 4px; "
        "padding: 10px 24px; font-size: 15px; font-weight: 600; }"
        "QPushButton:hover { background: #40A9FF; }"
        "QPushButton:pressed { background: #096DD9; }");
    compose_btn->setMinimumHeight(40);
    compose_btn->setCursor(Qt::PointingHandCursor);
    connect(compose_btn, &QPushButton::clicked, this, &MainWindow::on_compose);
    cl->addWidget(compose_btn, 1);
    left_layout->addWidget(compose_area);

    // 分隔线
    QFrame* sep = new QFrame(left_panel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #E0E2E6; margin: 0 16px;");
    left_layout->addWidget(sep);

    // 文件夹
    folder_tree_ = new QTreeWidget(left_panel);
    folder_tree_->setHeaderHidden(true);
    folder_tree_->setRootIsDecorated(false);
    folder_tree_->setIndentation(0);
    folder_tree_->setCursor(Qt::PointingHandCursor);

    inbox_item_   = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("📥  收件箱")));
    sent_item_    = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("📤  已发送")));
    drafts_item_  = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("📝  草稿箱")));
    trash_item_   = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("🗑  废纸篓")));

    // 图标列宽
    for (auto* item : {inbox_item_, sent_item_, drafts_item_, trash_item_})
        item->setSizeHint(0, QSize(0, 42));

    folder_tree_->expandAll();
    folder_tree_->setCurrentItem(inbox_item_);
    left_layout->addWidget(folder_tree_, 1);

    // 底部账号区
    QWidget* acct_bottom = new QWidget(left_panel);
    acct_bottom->setStyleSheet("border-top: 1px solid #E0E2E6; padding: 12px 16px;");
    QVBoxLayout* ab = new QVBoxLayout(acct_bottom);
    ab->setContentsMargins(0, 0, 0, 0);
    account_combo_ = new QComboBox(left_panel);
    account_combo_->setMinimumWidth(150);
    ab->addWidget(account_combo_);
    left_layout->addWidget(acct_bottom);

    main_layout->addWidget(left_panel);

    // ========== 右侧：邮件列表 + 阅读视图 ==========
    email_list_ = new EmailListWidget(this);
    email_view_ = new EmailViewWidget(this);

    right_splitter_ = new QSplitter(Qt::Vertical, this);
    right_splitter_->setHandleWidth(4);
    right_splitter_->addWidget(email_list_);
    right_splitter_->addWidget(email_view_);
    right_splitter_->setStretchFactor(0, 3);
    right_splitter_->setStretchFactor(1, 4);
    right_splitter_->setStyleSheet("QSplitter::handle { background: #F2F4F7; }");

    main_layout->addWidget(right_splitter_, 1);

    // 信号
    connect(folder_tree_, &QTreeWidget::itemClicked, this, &MainWindow::on_folder_changed);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_account_changed);
    connect(email_list_, &EmailListWidget::email_selected, this, &MainWindow::on_email_selected);

    // 状态栏
    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("color: #999; font-size: 12px; padding: 2px 8px;");
    statusBar()->addWidget(status_label_);
    statusBar()->setStyleSheet("QStatusBar { background: #FFFFFF; border-top: 1px solid #E8EAED; }");
}

void MainWindow::setup_toolbar() {
    toolbar_ = addToolBar(QStringLiteral("工具栏"));
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);

    // 品牌
    QLabel* brand = new QLabel(QStringLiteral("  ✉  邮件系统"), this);
    brand->setStyleSheet("font-size: 18px; font-weight: 700; color: #FFFFFF; padding: 0 12px;");
    toolbar_->addWidget(brand);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addWidget(spacer);

    btn_receive_ = new QPushButton(QStringLiteral("📥 收信"), this);
    btn_receive_->setObjectName("toolBtn");
    btn_receive_->setCursor(Qt::PointingHandCursor);
    toolbar_->addWidget(btn_receive_);
    connect(btn_receive_, &QPushButton::clicked, this, &MainWindow::on_receive);

    btn_compose_ = new QPushButton(QStringLiteral("✏️ 写信"), this);
    btn_compose_->setObjectName("toolBtn");
    btn_compose_->setCursor(Qt::PointingHandCursor);
    toolbar_->addWidget(btn_compose_);
    connect(btn_compose_, &QPushButton::clicked, this, &MainWindow::on_compose);

    btn_reply_ = new QPushButton(QStringLiteral("↩ 回复"), this);
    btn_reply_->setObjectName("toolBtn");
    btn_reply_->setCursor(Qt::PointingHandCursor);
    toolbar_->addWidget(btn_reply_);
    connect(btn_reply_, &QPushButton::clicked, this, &MainWindow::on_reply);

    btn_delete_ = new QPushButton(QStringLiteral("🗑 删除"), this);
    btn_delete_->setObjectName("toolBtn");
    btn_delete_->setCursor(Qt::PointingHandCursor);
    toolbar_->addWidget(btn_delete_);
    connect(btn_delete_, &QPushButton::clicked, this, &MainWindow::on_delete_mail);

    btn_restore_ = new QPushButton(QStringLiteral("↩ 恢复"), this);
    btn_restore_->setObjectName("toolBtn");
    btn_restore_->setCursor(Qt::PointingHandCursor);
    btn_restore_->setVisible(true);
    toolbar_->addWidget(btn_restore_);
    connect(btn_restore_, &QPushButton::clicked, this, &MainWindow::on_restore_mail);

    btn_refresh_ = new QPushButton(QStringLiteral("↻ 刷新"), this);
    btn_refresh_->setObjectName("toolBtn");
    btn_refresh_->setCursor(Qt::PointingHandCursor);
    toolbar_->addWidget(btn_refresh_);
    connect(btn_refresh_, &QPushButton::clicked, this, &MainWindow::on_refresh);
}

void MainWindow::setup_menu() {
    QMenu* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

    QAction* add_account_action = file_menu->addAction(QStringLiteral("添加账号..."));
    connect(add_account_action, &QAction::triggered, [this]() {
        AccountDialog dlg(db_mgr_, this);
        if (dlg.exec() == QDialog::Accepted) {
            Account acc = dlg.get_account();
            db_mgr_->add_account(acc);
            load_accounts();
        }
    });

    QAction* edit_account_action = file_menu->addAction(QStringLiteral("编辑账号..."));
    connect(edit_account_action, &QAction::triggered, [this]() {
        if (current_account_id_ < 0) return;
        Account acc = db_mgr_->get_account(current_account_id_);
        AccountDialog dlg(db_mgr_, this);
        dlg.set_account(acc);
        if (dlg.exec() == QDialog::Accepted) {
            Account new_acc = dlg.get_account();
            new_acc.id = current_account_id_;
            db_mgr_->update_account(new_acc);
            load_accounts();
        }
    });

    QAction* delete_account_action = file_menu->addAction(QStringLiteral("删除账号"));
    connect(delete_account_action, &QAction::triggered, [this]() {
        if (current_account_id_ < 0) return;
        if (QMessageBox::question(this, QStringLiteral("确认"),
              QStringLiteral("确定要删除当前账号吗？")) == QMessageBox::Yes) {
            db_mgr_->delete_account(current_account_id_);
            current_account_id_ = -1;
            load_accounts();
        }
    });

    file_menu->addSeparator();

    QAction* exit_action = file_menu->addAction(QStringLiteral("退出(&X)"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, qApp, &QApplication::quit);

    QMenu* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    help_menu->addAction(QStringLiteral("关于"))->connect(help_menu, &QMenu::triggered, [this]() {
        QMessageBox::about(this, QStringLiteral("关于"),
            QStringLiteral("<h3>邮件系统 v1.0</h3>"
                           "<p>计算机网络实践课程项目</p>"
                           "<p style='color:#888;'>C++17 / Qt 6 / MySQL / OpenSSL</p>"));
    });
}

// ---------- 以下方法（load_accounts / on_account_changed / on_folder_changed /
//           load_emails / update_folder_counts / on_email_selected /
//           on_receive / on_compose / on_send_draft / on_reply /
//           on_delete_mail / on_refresh）与之前相同 ----------

void MainWindow::load_accounts() {
    accounts_ = db_mgr_->get_all_accounts();
    account_combo_->blockSignals(true);
    account_combo_->clear();
    for (const auto& acc : accounts_)
        account_combo_->addItem(QString::fromStdString(acc.account_name + " (" + acc.email_address + ")"));
    account_combo_->blockSignals(false);

    if (!accounts_.empty() && current_account_id_ < 0) {
        current_account_id_ = accounts_[0].id;
        account_combo_->setCurrentIndex(0);
        load_emails(current_folder_);
        update_folder_counts();
    }
}

void MainWindow::on_account_changed(int index) {
    if (index >= 0 && index < (int)accounts_.size()) {
        current_account_id_ = accounts_[index].id;
        load_emails(current_folder_);
        update_folder_counts();
    }
}

void MainWindow::on_folder_changed() {
    QTreeWidgetItem* item = folder_tree_->currentItem();
    if (!item) return;
    if (item == inbox_item_)      current_folder_ = "inbox";
    else if (item == sent_item_)  current_folder_ = "sent";
    else if (item == drafts_item_) current_folder_ = "drafts";
    else if (item == trash_item_) current_folder_ = "trash";
    load_emails(current_folder_);
}

void MainWindow::load_emails(const std::string& folder) {
    if (current_account_id_ < 0) { email_list_->clear(); email_view_->clear(); return; }
    // 废纸篓特殊处理：显示所有 is_deleted=1 的邮件
    std::vector<Email> emails;
    if (folder == "trash") {
        emails = db_mgr_->get_deleted_emails(current_account_id_);
    } else {
        emails = db_mgr_->get_emails(current_account_id_, folder);
    }
    email_list_->set_emails(emails);
    email_view_->clear();

    QString name;
    if (folder == "inbox") name = QStringLiteral("收件箱");
    else if (folder == "sent") name = QStringLiteral("已发送");
    else if (folder == "drafts") name = QStringLiteral("草稿箱");
    else name = QStringLiteral("废纸篓");
    status_label_->setText(QStringLiteral("%1 — %2 封邮件").arg(name).arg(emails.size()));
}

void MainWindow::update_folder_counts() {
    if (current_account_id_ < 0) return;
    int unread = db_mgr_->get_unread_count(current_account_id_, "inbox");
    inbox_item_->setText(0, unread > 0
        ? QStringLiteral("📥  收件箱 (%1)").arg(unread) : QStringLiteral("📥  收件箱"));
    int sent = db_mgr_->get_total_count(current_account_id_, "sent");
    sent_item_->setText(0, sent > 0 ? QStringLiteral("📤  已发送 (%1)").arg(sent) : QStringLiteral("📤  已发送"));
    int drafts = db_mgr_->get_total_count(current_account_id_, "drafts");
    drafts_item_->setText(0, drafts > 0 ? QStringLiteral("📝  草稿箱 (%1)").arg(drafts) : QStringLiteral("📝  草稿箱"));
}

void MainWindow::on_email_selected(int email_id) {
    if (email_id < 0) return;
    Email email = db_mgr_->get_email(email_id);

    if (current_folder_ == "drafts") {
        ComposeDialog dlg(db_mgr_, current_account_id_, this);
        dlg.load_draft(email);
        if (dlg.exec() == QDialog::Accepted)
            on_send_draft(email_id, dlg.get_email());
        return;
    }

    email_view_->show_email(email);
    if (!email.is_read) { db_mgr_->mark_read(email_id); update_folder_counts(); }
}

void MainWindow::on_send_draft(int draft_id, const Email& updated) {
    Account acc = db_mgr_->get_account(current_account_id_);
    status_label_->setText(QStringLiteral("发送草稿中..."));

    typedef std::pair<bool, std::string> SR;
    auto* w = new QFutureWatcher<SR>(this);
    connect(w, &QFutureWatcher<SR>::finished, [this, w, draft_id, updated, acc]() {
        SR r = w->result();
        if (r.first) {
            db_mgr_->mark_deleted(draft_id);
            Email sent = updated; sent.account_id = acc.id; sent.folder = "sent";
            auto now = std::time(nullptr); char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            sent.received_date = buf;
            int eid = db_mgr_->save_email(sent);
            for (const auto& att : sent.attachments) {
                db_mgr_->add_attachment(eid, att);
            }
            status_label_->setText(QStringLiteral("草稿已发送！"));
        } else {
            QMessageBox::critical(this, QStringLiteral("发送失败"), QString::fromStdString(r.second));
            status_label_->setText(QStringLiteral("发送失败"));
        }
        load_emails(current_folder_); update_folder_counts();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([updated, acc]() -> SR {
        MimeEncoder enc; SmtpClient smtp;
        Email e = updated; e.body_plain = enc.encode(updated);
        bool ok = smtp.send_email(e, acc.smtp_server, acc.smtp_port, acc.smtp_ssl, acc.username, acc.password);
        return {ok, ok ? "" : smtp.get_last_error()};
    }));
}

void MainWindow::on_receive() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加邮箱账号。")); return;
    }
    Account acc = db_mgr_->get_account(current_account_id_);
    btn_receive_->setEnabled(false);
    status_label_->setText(QStringLiteral("正在接收邮件..."));

    typedef std::pair<std::vector<Email>, std::string> RR;
    auto* w = new QFutureWatcher<RR>(this);
    connect(w, &QFutureWatcher<RR>::finished, [this, w, acc]() {
        RR r = w->result(); auto emails = r.first; std::string err = r.second;
        if (!err.empty()) {
            QMessageBox::warning(this, QStringLiteral("接收失败"), QString::fromStdString(err));
            status_label_->setText(QStringLiteral("接收失败 — %1").arg(QString::fromStdString(err)));
        } else {
            MimeDecoder dec; int new_count = 0;
            for (auto& email : emails) {
                if (!email.pop3_uid.empty() && db_mgr_->is_uid_downloaded(acc.id, email.pop3_uid)) continue;
                email = dec.decode(email.body_plain, acc.id);
                email.folder = "inbox";
                int eid = db_mgr_->save_email(email);
                if (eid > 0) {
                    if (!email.pop3_uid.empty()) db_mgr_->mark_uid_downloaded(acc.id, email.pop3_uid, eid);
                    new_count++;
                }
            }
            status_label_->setText(QStringLiteral("接收完成 — %1 封新邮件").arg(new_count));
            load_emails(current_folder_); update_folder_counts();
        }
        btn_receive_->setEnabled(true); w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([acc]() -> RR {
        Pop3Client pop3; int nc = 0;
        auto emails = pop3.receive_emails(acc.pop3_server, acc.pop3_port, acc.pop3_ssl, acc.username, acc.password, &nc);
        std::string err;
        if (emails.empty()) err = pop3.get_last_error();
        return {emails, err};
    }));
}

void MainWindow::on_compose() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加邮箱账号。")); return;
    }
    ComposeDialog dlg(db_mgr_, current_account_id_, this);
    if (dlg.exec() == QDialog::Accepted) {
        Email email = dlg.get_email();
        Account acc = db_mgr_->get_account(current_account_id_);
        status_label_->setText(QStringLiteral("发送中..."));

        typedef std::pair<bool, std::string> SR;
        auto* w = new QFutureWatcher<SR>(this);
        connect(w, &QFutureWatcher<SR>::finished, [this, w, email, acc]() {
            SR r = w->result();
            if (r.first) {
                Email sent = email; sent.account_id = acc.id; sent.folder = "sent";
                auto now = std::time(nullptr); char buf[64];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                sent.received_date = buf;
                int eid = db_mgr_->save_email(sent);
                // 保存附件
                for (const auto& att : sent.attachments) {
                    db_mgr_->add_attachment(eid, att);
                }
                status_label_->setText(QStringLiteral("发送成功！"));
            } else {
                QMessageBox::critical(this, QStringLiteral("发送失败"), QString::fromStdString(r.second));
                status_label_->setText(QStringLiteral("发送失败"));
            }
            load_emails(current_folder_); update_folder_counts(); w->deleteLater();
        });
        w->setFuture(QtConcurrent::run([email, acc]() -> SR {
            MimeEncoder enc; SmtpClient smtp;
            Email e = email; e.body_plain = enc.encode(email);
            bool ok = smtp.send_email(e, acc.smtp_server, acc.smtp_port, acc.smtp_ssl, acc.username, acc.password);
            return {ok, ok ? "" : smtp.get_last_error()};
        }));
    }
}

void MainWindow::on_reply() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0 || current_account_id_ < 0) return;
    Email original = db_mgr_->get_email(email_id);
    ComposeDialog dlg(db_mgr_, current_account_id_, this);
    dlg.set_reply_mode(original, false);
    if (dlg.exec() == QDialog::Accepted) {
        Email email = dlg.get_email();
        Account acc = db_mgr_->get_account(current_account_id_);
        QtConcurrent::run([email, acc]() {
            MimeEncoder enc; SmtpClient smtp;
            Email e = email; e.body_plain = enc.encode(email);
            smtp.send_email(e, acc.smtp_server, acc.smtp_port, acc.smtp_ssl, acc.username, acc.password);
        });
        status_label_->setText(QStringLiteral("回复已发送"));
    }
}

void MainWindow::on_delete_mail() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0) {
        status_label_->setText(QStringLiteral("请先在邮件列表中选择一封邮件"));
        return;
    }
    if (current_folder_ == "trash") {
        // 废纸篓中彻底删除
        auto reply = QMessageBox::question(this, QStringLiteral("彻底删除"),
            QStringLiteral("确定要彻底删除这封邮件吗？\n此操作不可恢复！"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            db_mgr_->delete_permanently(email_id);
            load_emails(current_folder_);
            update_folder_counts();
            status_label_->setText(QStringLiteral("已彻底删除"));
        }
    } else {
        // 其他文件夹移入废纸篓
        auto reply = QMessageBox::question(this, QStringLiteral("确认删除"),
            QStringLiteral("确定删除这封邮件？\n（可在废纸篓中找回）"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            db_mgr_->mark_deleted(email_id);  // 只标记，不改 folder
            load_emails(current_folder_);
            update_folder_counts();
            status_label_->setText(QStringLiteral("已移入废纸篓"));
        }
    }
}

void MainWindow::on_restore_mail() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0) {
        status_label_->setText(QStringLiteral("请先在邮件列表中选择一封邮件"));
        return;
    }
    db_mgr_->mark_undeleted(email_id);  // 取消标记即可，folder 保持原值
    load_emails(current_folder_);
    update_folder_counts();
    status_label_->setText(QStringLiteral("已恢复"));
}

void MainWindow::on_refresh() {
    load_emails(current_folder_); update_folder_counts();
    status_label_->setText(QStringLiteral("已刷新"));
}
