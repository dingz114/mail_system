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
#include <QStyleFactory>

// ========== 全局样式表 ==========
static const char* GLOBAL_STYLE = R"(
    /* --- 全局基础 --- */
    QMainWindow, QDialog {
        background-color: #F0F2F5;
    }
    QWidget {
        font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
        font-size: 13px;
        color: #333333;
    }

    /* --- 菜单栏 --- */
    QMenuBar {
        background-color: #FFFFFF;
        border-bottom: 1px solid #E4E7ED;
        padding: 2px 8px;
        font-size: 13px;
    }
    QMenuBar::item {
        padding: 6px 12px;
        border-radius: 4px;
    }
    QMenuBar::item:selected {
        background-color: #ECF5FF;
        color: #1890FF;
    }
    QMenu {
        background-color: #FFFFFF;
        border: 1px solid #E4E7ED;
        border-radius: 6px;
        padding: 4px;
    }
    QMenu::item {
        padding: 8px 32px 8px 16px;
        border-radius: 4px;
    }
    QMenu::item:selected {
        background-color: #ECF5FF;
        color: #1890FF;
    }
    QMenu::separator {
        height: 1px;
        background: #E4E7ED;
        margin: 4px 12px;
    }

    /* --- 工具栏 --- */
    QToolBar {
        background-color: #FFFFFF;
        border-bottom: 1px solid #E4E7ED;
        padding: 6px 12px;
        spacing: 8px;
    }

    /* --- 主按钮 (蓝底) --- */
    QPushButton#primaryBtn {
        background-color: #1890FF;
        color: #FFFFFF;
        border: none;
        border-radius: 6px;
        padding: 8px 20px;
        font-size: 13px;
        font-weight: 500;
    }
    QPushButton#primaryBtn:hover {
        background-color: #40A9FF;
    }
    QPushButton#primaryBtn:pressed {
        background-color: #096DD9;
    }

    /* --- 普通按钮 --- */
    QPushButton {
        background-color: #FFFFFF;
        color: #333333;
        border: 1px solid #D9D9D9;
        border-radius: 6px;
        padding: 7px 18px;
        font-size: 13px;
    }
    QPushButton:hover {
        color: #1890FF;
        border-color: #1890FF;
    }
    QPushButton:pressed {
        color: #096DD9;
        border-color: #096DD9;
    }

    /* --- 危险按钮 --- */
    QPushButton#dangerBtn {
        background-color: #FFFFFF;
        color: #FF4D4F;
        border: 1px solid #FF4D4F;
        border-radius: 6px;
        padding: 7px 18px;
    }
    QPushButton#dangerBtn:hover {
        background-color: #FFF2F0;
    }

    /* --- 下拉框 --- */
    QComboBox {
        background-color: #FFFFFF;
        border: 1px solid #D9D9D9;
        border-radius: 6px;
        padding: 6px 12px;
        font-size: 13px;
        min-height: 20px;
    }
    QComboBox:hover {
        border-color: #1890FF;
    }
    QComboBox::drop-down {
        border: none;
        width: 24px;
    }
    QComboBox QAbstractItemView {
        border: 1px solid #E4E7ED;
        border-radius: 4px;
        padding: 4px;
        selection-background-color: #ECF5FF;
        selection-color: #1890FF;
    }

    /* --- 输入框 --- */
    QLineEdit, QTextEdit, QPlainTextEdit {
        border: 1px solid #D9D9D9;
        border-radius: 6px;
        padding: 8px 12px;
        background-color: #FFFFFF;
        font-size: 13px;
    }
    QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
        border-color: #1890FF;
        outline: none;
    }

    /* --- 树形控件 (文件夹) --- */
    QTreeWidget {
        background-color: #FFFFFF;
        border: 1px solid #E4E7ED;
        border-radius: 8px;
        padding: 8px;
        font-size: 14px;
    }
    QTreeWidget::item {
        padding: 8px 12px;
        border-radius: 6px;
        margin: 2px 0;
    }
    QTreeWidget::item:selected {
        background-color: #E6F7FF;
        color: #1890FF;
    }
    QTreeWidget::item:hover {
        background-color: #F5F5F5;
    }

    /* --- 表格 --- */
    QTableWidget {
        background-color: #FFFFFF;
        border: 1px solid #E4E7ED;
        border-radius: 8px;
        gridline-color: #F0F0F0;
        font-size: 13px;
    }
    QTableWidget::item {
        padding: 10px 12px;
        border-bottom: 1px solid #F5F5F5;
    }
    QTableWidget::item:selected {
        background-color: #E6F7FF;
        color: #333333;
    }
    QHeaderView::section {
        background-color: #FAFAFA;
        border: none;
        border-bottom: 2px solid #E4E7ED;
        padding: 10px 12px;
        font-weight: 600;
        font-size: 12px;
        color: #888888;
        text-transform: uppercase;
    }

    /* --- 分割器 --- */
    QSplitter::handle {
        background-color: #E4E7ED;
    }
    QSplitter::handle:horizontal { width: 1px; }
    QSplitter::handle:vertical   { height: 1px; }

    /* --- 状态栏 --- */
    QStatusBar {
        background-color: #FFFFFF;
        border-top: 1px solid #E4E7ED;
        padding: 4px 12px;
        font-size: 12px;
        color: #888888;
    }

    /* --- 分组框 --- */
    QGroupBox {
        font-weight: 600;
        border: 1px solid #E4E7ED;
        border-radius: 8px;
        margin-top: 12px;
        padding: 16px 12px 12px 12px;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        left: 16px;
        padding: 0 8px;
        color: #333333;
    }

    /* --- 复选框 --- */
    QCheckBox {
        spacing: 8px;
    }
    QCheckBox::indicator {
        width: 16px;
        height: 16px;
        border: 2px solid #D9D9D9;
        border-radius: 3px;
    }
    QCheckBox::indicator:checked {
        background-color: #1890FF;
        border-color: #1890FF;
    }

    /* --- 滚动条 --- */
    QScrollBar:vertical {
        background: transparent;
        width: 6px;
        margin: 0;
    }
    QScrollBar::handle:vertical {
        background: #D9D9D9;
        border-radius: 3px;
        min-height: 40px;
    }
    QScrollBar::handle:vertical:hover {
        background: #BFBFBF;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0;
    }

    /* --- 提示框 --- */
    QMessageBox {
        background-color: #FFFFFF;
    }
    QMessageBox QPushButton {
        min-width: 80px;
    }
)";

MainWindow::MainWindow(DbManager* db_mgr, QWidget* parent)
    : QMainWindow(parent), db_mgr_(db_mgr), current_account_id_(-1), current_folder_("inbox") {
    // 应用全局样式
    qApp->setStyleSheet(QLatin1String(GLOBAL_STYLE));
    setup_ui();
    setup_toolbar();
    setup_menu();
    load_accounts();

    setWindowTitle(QStringLiteral("邮件系统 v1.0"));
    resize(1200, 750);

    // 居中显示
    QRect screen = QApplication::primaryScreen()->availableGeometry();
    move((screen.width() - width()) / 2, (screen.height() - height()) / 2);

    status_label_->setText(QStringLiteral("就绪"));
}

MainWindow::~MainWindow() {}

void MainWindow::setup_ui() {
    // Central widget
    QWidget* central = new QWidget(this);
    central->setStyleSheet("background-color: #F0F2F5;");
    setCentralWidget(central);
    QHBoxLayout* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(12);

    // --- 左侧：文件夹面板 ---
    QWidget* left_panel = new QWidget(this);
    left_panel->setFixedWidth(200);
    left_panel->setStyleSheet("background-color: #FFFFFF; border-radius: 10px;");
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(4, 8, 4, 8);

    QLabel* folder_title = new QLabel(QStringLiteral("📬 邮件夹"), this);
    folder_title->setStyleSheet("font-size: 15px; font-weight: 700; color: #333; padding: 8px 12px 4px 12px;");
    left_layout->addWidget(folder_title);

    folder_tree_ = new QTreeWidget(this);
    folder_tree_->setHeaderHidden(true);
    folder_tree_->setRootIsDecorated(false);
    folder_tree_->setIndentation(0);

    inbox_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("📥 收件箱")));
    inbox_item_->setIcon(0, QIcon());
    sent_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("📤 已发送")));
    drafts_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("📝 草稿箱")));
    trash_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("🗑️ 废纸篓")));
    folder_tree_->expandAll();

    left_layout->addWidget(folder_tree_);
    left_layout->addStretch();

    // 账号切换标签
    QLabel* acct_title = new QLabel(QStringLiteral("📧 账号"), this);
    acct_title->setStyleSheet("font-size: 15px; font-weight: 700; color: #333; padding: 12px 12px 4px 12px;");
    left_layout->addWidget(acct_title);

    account_combo_ = new QComboBox(this);
    account_combo_->setMinimumWidth(160);
    left_layout->addWidget(account_combo_);

    main_layout->addWidget(left_panel);

    // --- 右侧：邮件列表 + 阅读视图 ---
    email_list_ = new EmailListWidget(this);
    email_view_ = new EmailViewWidget(this);

    right_splitter_ = new QSplitter(Qt::Vertical, this);
    right_splitter_->setHandleWidth(8);
    right_splitter_->addWidget(email_list_);
    right_splitter_->addWidget(email_view_);
    right_splitter_->setStretchFactor(0, 3);
    right_splitter_->setStretchFactor(1, 4);
    right_splitter_->setStyleSheet("QSplitter::handle { background: #F0F2F5; }");

    main_layout->addWidget(right_splitter_, 1);

    connect(folder_tree_, &QTreeWidget::itemClicked, this, &MainWindow::on_folder_changed);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_account_changed);
    connect(email_list_, &EmailListWidget::email_selected, this, &MainWindow::on_email_selected);

    // Status bar
    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("color: #999; font-size: 12px; padding: 2px 8px;");
    statusBar()->addWidget(status_label_);
    statusBar()->setStyleSheet("QStatusBar { background: #FFFFFF; border-top: 1px solid #E4E7ED; }");
}

void MainWindow::setup_toolbar() {
    toolbar_ = addToolBar(QStringLiteral("工具栏"));
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);

    // 品牌标题
    QLabel* brand = new QLabel(QStringLiteral("  ✉ 邮件系统 "), this);
    brand->setStyleSheet("font-size: 16px; font-weight: 700; color: #1890FF; padding: 0 8px; margin-right: 12px;");
    toolbar_->addWidget(brand);
    toolbar_->addSeparator();

    btn_receive_ = new QPushButton(QStringLiteral("📥 接收"), this);
    btn_receive_->setObjectName("primaryBtn");
    toolbar_->addWidget(btn_receive_);
    connect(btn_receive_, &QPushButton::clicked, this, &MainWindow::on_receive);

    btn_compose_ = new QPushButton(QStringLiteral("✏️ 写信"), this);
    btn_compose_->setObjectName("primaryBtn");
    toolbar_->addWidget(btn_compose_);
    connect(btn_compose_, &QPushButton::clicked, this, &MainWindow::on_compose);

    btn_reply_ = new QPushButton(QStringLiteral("↩ 回复"), this);
    toolbar_->addWidget(btn_reply_);
    connect(btn_reply_, &QPushButton::clicked, this, &MainWindow::on_reply);

    btn_delete_ = new QPushButton(QStringLiteral("🗑 删除"), this);
    btn_delete_->setObjectName("dangerBtn");
    toolbar_->addWidget(btn_delete_);
    connect(btn_delete_, &QPushButton::clicked, this, &MainWindow::on_delete_mail);

    btn_refresh_ = new QPushButton(QStringLiteral("🔄 刷新"), this);
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

    file_menu->addSeparator();

    QAction* exit_action = file_menu->addAction(QStringLiteral("退出(&X)"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, qApp, &QApplication::quit);

    QMenu* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    QAction* about_action = help_menu->addAction(QStringLiteral("关于"));
    connect(about_action, &QAction::triggered, [this]() {
        QMessageBox::about(this, QStringLiteral("关于 邮件系统"),
                           QStringLiteral("<h3>邮件系统 v1.0</h3>"
                                          "<p>SMTP & POP3 邮件客户端</p>"
                                          "<p>计算机网络实践课程项目</p>"
                                          "<hr>"
                                          "<p style='color:#888;'>技术栈：C++17 / Qt 6 / MySQL / OpenSSL</p>"));
    });
}

void MainWindow::load_accounts() {
    accounts_ = db_mgr_->get_all_accounts();
    account_combo_->blockSignals(true);
    account_combo_->clear();
    for (const auto& acc : accounts_) {
        account_combo_->addItem(QString::fromStdString(acc.account_name + " (" + acc.email_address + ")"));
    }
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

    if (item == inbox_item_) current_folder_ = "inbox";
    else if (item == sent_item_) current_folder_ = "sent";
    else if (item == drafts_item_) current_folder_ = "drafts";
    else if (item == trash_item_) current_folder_ = "trash";

    load_emails(current_folder_);
}

void MainWindow::load_emails(const std::string& folder) {
    if (current_account_id_ < 0) {
        email_list_->clear();
        email_view_->clear();
        return;
    }

    auto emails = db_mgr_->get_emails(current_account_id_, folder);
    email_list_->set_emails(emails);
    email_view_->clear();

    QString folder_name;
    if (folder == "inbox") folder_name = QStringLiteral("收件箱");
    else if (folder == "sent") folder_name = QStringLiteral("已发送");
    else if (folder == "drafts") folder_name = QStringLiteral("草稿箱");
    else if (folder == "trash") folder_name = QStringLiteral("废纸篓");

    status_label_->setText(QStringLiteral("%1 — 共 %2 封邮件").arg(folder_name).arg(emails.size()));
}

void MainWindow::update_folder_counts() {
    if (current_account_id_ < 0) return;

    int unread = db_mgr_->get_unread_count(current_account_id_, "inbox");
    inbox_item_->setText(0, unread > 0
        ? QStringLiteral("📥 收件箱 (%1)").arg(unread)
        : QStringLiteral("📥 收件箱"));

    int sent = db_mgr_->get_total_count(current_account_id_, "sent");
    sent_item_->setText(0, sent > 0
        ? QStringLiteral("📤 已发送 (%1)").arg(sent)
        : QStringLiteral("📤 已发送"));

    int drafts = db_mgr_->get_total_count(current_account_id_, "drafts");
    drafts_item_->setText(0, drafts > 0
        ? QStringLiteral("📝 草稿箱 (%1)").arg(drafts)
        : QStringLiteral("📝 草稿箱"));
}

void MainWindow::on_email_selected(int email_id) {
    if (email_id < 0) return;
    Email email = db_mgr_->get_email(email_id);

    // 草稿箱中点击 → 打开编辑对话框继续编辑
    if (current_folder_ == "drafts") {
        ComposeDialog dlg(db_mgr_, current_account_id_, this);
        dlg.load_draft(email);
        if (dlg.exec() == QDialog::Accepted) {
            // 用户点了发送
            on_send_draft(email_id, dlg.get_email());
        }
        // 用户取消或关闭对话框 → 草稿保持原样
        return;
    }

    email_view_->show_email(email);
    if (!email.is_read) {
        db_mgr_->mark_read(email_id);
        update_folder_counts();
    }
}

void MainWindow::on_send_draft(int draft_id, const Email& updated) {
    Account acc = db_mgr_->get_account(current_account_id_);
    status_label_->setText(QStringLiteral("正在发送草稿..."));

    typedef std::pair<bool, std::string> SendResult;
    auto* watcher = new QFutureWatcher<SendResult>(this);
    connect(watcher, &QFutureWatcher<SendResult>::finished,
            [this, watcher, draft_id, updated, acc]() {
        SendResult r = watcher->result();
        if (r.first) {
            db_mgr_->mark_deleted(draft_id);
            Email sent = updated;
            sent.account_id = acc.id;
            sent.folder = "sent";
            auto now = std::time(nullptr);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            sent.received_date = buf;
            db_mgr_->save_email(sent);
            status_label_->setText(QStringLiteral("草稿已发送！"));
        } else {
            QMessageBox::critical(this, QStringLiteral("发送失败"),
                                  QString::fromStdString(r.second));
            status_label_->setText(QStringLiteral("草稿发送失败"));
        }
        load_emails(current_folder_);
        update_folder_counts();
        watcher->deleteLater();
    });

    QFuture<SendResult> future = QtConcurrent::run([updated, acc]() -> SendResult {
        MimeEncoder encoder;
        std::string mime = encoder.encode(updated);
        SmtpClient smtp;
        Email enc = updated;
        enc.body_plain = mime;
        bool ok = smtp.send_email(enc,
            acc.smtp_server, acc.smtp_port, acc.smtp_ssl,
            acc.username, acc.password);
        return {ok, ok ? "" : smtp.get_last_error()};
    });
    watcher->setFuture(future);
}

void MainWindow::on_receive() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先添加邮箱账号。"));
        return;
    }

    Account acc = db_mgr_->get_account(current_account_id_);
    btn_receive_->setEnabled(false);
    status_label_->setText(QStringLiteral("正在连接 POP3 服务器..."));

    typedef std::pair<std::vector<Email>, std::string> ReceiveResult;
    auto* watcher = new QFutureWatcher<ReceiveResult>(this);
    connect(watcher, &QFutureWatcher<ReceiveResult>::finished, [this, watcher, acc]() {
        ReceiveResult r = watcher->result();
        auto emails = r.first;
        std::string err = r.second;

        if (!err.empty()) {
            QMessageBox::warning(this, QStringLiteral("接收失败"),
                                 QString::fromStdString(err));
            status_label_->setText(QStringLiteral("接收失败 — %1").arg(QString::fromStdString(err)));
        } else {
            MimeDecoder decoder;
            int new_count = 0;

            for (auto& email : emails) {
                if (!email.pop3_uid.empty() &&
                    db_mgr_->is_uid_downloaded(acc.id, email.pop3_uid)) {
                    continue;
                }

                email = decoder.decode(email.body_plain, acc.id);
                email.folder = "inbox";
                email.received_date = email.received_date.empty() ? "now" : email.received_date;

                int email_id = db_mgr_->save_email(email);
                if (email_id > 0) {
                    if (!email.pop3_uid.empty()) {
                        db_mgr_->mark_uid_downloaded(acc.id, email.pop3_uid, email_id);
                    }
                    new_count++;
                }
            }

            status_label_->setText(QStringLiteral("接收完成 — 共 %1 封新邮件").arg(new_count));
            load_emails(current_folder_);
            update_folder_counts();
        }

        btn_receive_->setEnabled(true);
        watcher->deleteLater();
    });

    QFuture<ReceiveResult> future = QtConcurrent::run([acc]() -> ReceiveResult {
        Pop3Client pop3;
        int new_count = 0;
        auto emails = pop3.receive_emails(
            acc.pop3_server, acc.pop3_port, acc.pop3_ssl,
            acc.username, acc.password, &new_count);
        std::string err = pop3.get_last_error();
        if (err.empty() && emails.empty()) {
            // Distinguish "0 new emails" from "error"
            // Leave err empty — 0 emails is normal
        }
        return {emails, err};
    });

    watcher->setFuture(future);
}

void MainWindow::on_compose() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先添加邮箱账号。"));
        return;
    }

    ComposeDialog dlg(db_mgr_, current_account_id_, this);
    if (dlg.exec() == QDialog::Accepted) {
        Email email = dlg.get_email();
        Account acc = db_mgr_->get_account(current_account_id_);

        status_label_->setText(QStringLiteral("正在发送邮件..."));

        // 返回 <成功, 错误信息>，空字符串 = 成功
        typedef std::pair<bool, std::string> SendResult;
        auto* watcher = new QFutureWatcher<SendResult>(this);
        connect(watcher, &QFutureWatcher<SendResult>::finished, [this, watcher, email, acc]() {
            SendResult r = watcher->result();
            if (r.first) {
                Email sent_email = email;
                sent_email.account_id = acc.id;
                sent_email.folder = "sent";
                auto now = std::time(nullptr);
                char buf[64];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                sent_email.received_date = buf;

                db_mgr_->save_email(sent_email);
                status_label_->setText(QStringLiteral("邮件发送成功！"));
                load_emails(current_folder_);
                update_folder_counts();
            } else {
                QString err = QString::fromStdString(r.second);
                if (err.isEmpty()) err = QStringLiteral("未知错误");
                QMessageBox::critical(this, QStringLiteral("发送失败"), err);
                status_label_->setText(QStringLiteral("发送失败 — %1").arg(err));
            }
            watcher->deleteLater();
        });

        QFuture<SendResult> future = QtConcurrent::run([email, acc]() -> SendResult {
            MimeEncoder encoder;
            std::string mime = encoder.encode(email);
            SmtpClient smtp;
            Email enc_email = email;
            enc_email.body_plain = mime;

            bool ok = smtp.send_email(enc_email,
                                      acc.smtp_server, acc.smtp_port, acc.smtp_ssl,
                                      acc.username, acc.password);
            std::string err;
            if (!ok) {
                err = smtp.get_last_error();
                if (err.empty()) err = smtp.get_last_response();
            }
            return {ok, err};
        });

        watcher->setFuture(future);
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

        MimeEncoder encoder;
        std::string mime = encoder.encode(email);

        QtConcurrent::run([email, acc, mime]() {
            SmtpClient smtp;
            Email enc_email = email;
            enc_email.body_plain = mime;
            smtp.send_email(enc_email,
                           acc.smtp_server, acc.smtp_port, acc.smtp_ssl,
                           acc.username, acc.password);
        });

        status_label_->setText(QStringLiteral("回复已发送"));
    }
}

void MainWindow::on_delete_mail() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0) return;

    auto reply = QMessageBox::question(this, QStringLiteral("确认删除"),
                                        QStringLiteral("确定要删除这封邮件吗？"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        db_mgr_->mark_deleted(email_id);
        load_emails(current_folder_);
        update_folder_counts();
        status_label_->setText(QStringLiteral("邮件已删除"));
    }
}

void MainWindow::on_refresh() {
    load_emails(current_folder_);
    update_folder_counts();
    status_label_->setText(QStringLiteral("已刷新"));
}
