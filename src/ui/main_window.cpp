#include "main_window.h"
#include "email_list_widget.h"
#include "email_view_widget.h"
#include "compose_dialog.h"
#include "account_dialog.h"
#include "app_style.h"
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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QDateTime>
#include <algorithm>
#include <cctype>

namespace {

QIcon toolbarIcon(const QString& name, const QColor& color) {
    QPixmap pix(22, 22);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (name == QStringLiteral("receive")) {
        p.drawLine(QPointF(11, 4), QPointF(11, 13));
        p.drawLine(QPointF(7.5, 9.5), QPointF(11, 13));
        p.drawLine(QPointF(14.5, 9.5), QPointF(11, 13));
        p.drawLine(QPointF(5, 15), QPointF(5, 18));
        p.drawLine(QPointF(5, 18), QPointF(17, 18));
        p.drawLine(QPointF(17, 18), QPointF(17, 15));
    } else if (name == QStringLiteral("compose")) {
        p.drawRoundedRect(QRectF(4, 5, 11, 13), 2, 2);
        p.drawLine(QPointF(7, 9), QPointF(12, 9));
        p.drawLine(QPointF(7, 12), QPointF(11, 12));
        p.drawLine(QPointF(13.5, 15.5), QPointF(18, 11));
        p.drawLine(QPointF(16.5, 9.5), QPointF(18.5, 11.5));
    } else if (name == QStringLiteral("reply")) {
        p.drawLine(QPointF(14, 6), QPointF(7, 11));
        p.drawLine(QPointF(7, 11), QPointF(14, 16));
        p.drawLine(QPointF(8, 11), QPointF(18, 11));
    } else if (name == QStringLiteral("delete")) {
        p.drawLine(QPointF(7, 7), QPointF(15, 7));
        p.drawLine(QPointF(9, 5), QPointF(13, 5));
        p.drawRoundedRect(QRectF(7.5, 8, 7, 10), 1.5, 1.5);
        p.drawLine(QPointF(10, 10.5), QPointF(10, 15.5));
        p.drawLine(QPointF(12, 10.5), QPointF(12, 15.5));
    } else if (name == QStringLiteral("restore")) {
        p.drawLine(QPointF(11, 18), QPointF(11, 9));
        p.drawLine(QPointF(7.5, 12.5), QPointF(11, 9));
        p.drawLine(QPointF(14.5, 12.5), QPointF(11, 9));
        p.drawLine(QPointF(5, 7), QPointF(5, 4));
        p.drawLine(QPointF(5, 4), QPointF(17, 4));
        p.drawLine(QPointF(17, 4), QPointF(17, 7));
    } else if (name == QStringLiteral("refresh")) {
        p.drawArc(QRectF(5, 5, 12, 12), 35 * 16, 250 * 16);
        p.drawLine(QPointF(16.5, 5.5), QPointF(17.5, 10));
        p.drawLine(QPointF(16.5, 5.5), QPointF(12.2, 5.8));
    } else if (name == QStringLiteral("multi")) {
        p.drawRoundedRect(QRectF(4, 5, 5, 5), 1.2, 1.2);
        p.drawRoundedRect(QRectF(4, 13, 5, 5), 1.2, 1.2);
        p.drawLine(QPointF(12, 7.5), QPointF(18, 7.5));
        p.drawLine(QPointF(12, 15.5), QPointF(18, 15.5));
    } else if (name == QStringLiteral("done")) {
        p.drawRoundedRect(QRectF(4, 4, 14, 14), 3, 3);
        p.drawLine(QPointF(7.5, 11.2), QPointF(10.2, 14));
        p.drawLine(QPointF(10.2, 14), QPointF(15, 8.5));
    } else if (name == QStringLiteral("back")) {
        p.drawLine(QPointF(14, 6), QPointF(8, 11));
        p.drawLine(QPointF(8, 11), QPointF(14, 16));
        p.drawLine(QPointF(8.5, 11), QPointF(18, 11));
    }

    return QIcon(pix);
}

void applyToolbarIcon(QPushButton* button, const QString& name, const QColor& color) {
    button->setIcon(toolbarIcon(name, color));
    button->setIconSize(QSize(18, 18));
}

std::string to_utf8(const QString& text) {
    QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString from_utf8(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
}

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string smtp_endpoint_label(const std::string& server, int port, bool ssl) {
    return server + ":" + std::to_string(port) + (ssl ? " SSL/TLS" : " STARTTLS");
}

bool is_retryable_smtp_failure(const std::string& error) {
    const std::string lower = lower_ascii(error);
    return lower.find("connect failed") != std::string::npos ||
           lower.find("ssl connect failed") != std::string::npos ||
           lower.find("tcp connect failed") != std::string::npos ||
           lower.find("starttls failed") != std::string::npos ||
           lower.find("does not advertise starttls") != std::string::npos ||
           lower.find("no smtp greeting") != std::string::npos ||
           lower.find("unexpected greeting") != std::string::npos;
}

bool validate_attachments(const Email& email, std::string& error) {
    for (const auto& att : email.attachments) {
        QString path = from_utf8(att.file_path);
        QFileInfo fi(path);
        if (path.isEmpty() || !fi.exists() || !fi.isFile() || !fi.isReadable()) {
            error = to_utf8(QStringLiteral("附件文件不存在或无法读取：%1").arg(path));
            return false;
        }
    }
    return true;
}

std::pair<bool, std::string> send_email_with_progress(
    const Email& email,
    const Account& acc,
    SmtpClient::ProgressCallback progress) {
    std::string attachment_error;
    if (!validate_attachments(email, attachment_error)) {
        return {false, attachment_error};
    }

    MimeEncoder enc;
    Email e = email;
    e.body_plain = enc.encode(email);

    struct Candidate {
        std::string server;
        int port;
        bool ssl;
    };

    std::vector<Candidate> candidates;
    auto add_candidate = [&candidates](const std::string& server, int port, bool ssl) {
        if (server.empty() || port <= 0) return;
        for (const auto& existing : candidates) {
            if (lower_ascii(existing.server) == lower_ascii(server) &&
                existing.port == port &&
                existing.ssl == ssl) {
                return;
            }
        }
        candidates.push_back({server, port, ssl});
    };

    add_candidate(acc.smtp_server, acc.smtp_port, acc.smtp_ssl);

    const std::string server_lower = lower_ascii(acc.smtp_server);
    const std::string email_lower = lower_ascii(acc.email_address);
    if (server_lower.find("gmail.com") != std::string::npos ||
        email_lower.find("@gmail.com") != std::string::npos) {
        add_candidate("smtp.gmail.com", 465, true);
        add_candidate("smtp.gmail.com", 587, false);
    } else if (server_lower.find("outlook") != std::string::npos ||
               server_lower.find("office365") != std::string::npos ||
               email_lower.find("@outlook.") != std::string::npos ||
               email_lower.find("@hotmail.") != std::string::npos ||
               email_lower.find("@live.") != std::string::npos) {
        add_candidate("smtp-mail.outlook.com", 587, false);
        add_candidate("smtp.office365.com", 587, false);
    } else {
        add_candidate(acc.smtp_server, 465, true);
        add_candidate(acc.smtp_server, 587, false);
    }

    std::string failures;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        SmtpClient smtp;
        bool ok = smtp.send_email(e, c.server, c.port, c.ssl,
                                  acc.username, acc.password, progress);
        if (ok) return {true, ""};

        std::string err = smtp.get_last_error();
        if (err.empty()) err = smtp.get_last_response();
        if (err.empty()) err = "No response from SMTP server";

        failures += smtp_endpoint_label(c.server, c.port, c.ssl) + ": " + err;
        if (i + 1 < candidates.size()) failures += "\n";

        if (!is_retryable_smtp_failure(err)) {
            break;
        }
    }

    return {false, failures};
}

Email copy_attachments_for_sent(Email email) {
    QString base_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       + "/sent_attachments";
    QDir dir;
    dir.mkpath(base_dir);

    for (size_t i = 0; i < email.attachments.size(); ++i) {
        Attachment& att = email.attachments[i];
        QString src = from_utf8(att.file_path);
        if (src.isEmpty() || !QFile::exists(src)) continue;

        QFileInfo fi(src);
        QString safe_name = fi.fileName();
        safe_name.replace('/', '_');
        safe_name.replace('\\', '_');
        safe_name.replace(':', '_');
        QString dest = QDir(base_dir).filePath(
            QStringLiteral("%1_%2_%3")
                .arg(QDateTime::currentMSecsSinceEpoch())
                .arg(static_cast<int>(i))
                .arg(safe_name));

        if (QFile::copy(src, dest)) {
            att.file_path = to_utf8(dest);
        }
    }
    return email;
}

} // namespace

MainWindow::MainWindow(DbManager* db_mgr, QWidget* parent)
    : QMainWindow(parent), db_mgr_(db_mgr), current_account_id_(-1), current_folder_("inbox") {
    qApp->setStyleSheet(UiStyle::globalStyle());
    setup_ui();
    setup_toolbar();
    setup_menu();
    load_accounts();

    setWindowTitle(QStringLiteral("邮件系统"));
    resize(1180, 760);
    QRect screen = QApplication::primaryScreen()->availableGeometry();
    move(screen.x() + (screen.width() - width()) / 2,
         screen.y() + (screen.height() - height()) / 2);
    status_label_->setText(QStringLiteral("就绪"));
}

MainWindow::~MainWindow() {}

void MainWindow::setup_ui() {
    QWidget* central = new QWidget(this);
    central->setObjectName("mainSurface");
    setCentralWidget(central);

    QHBoxLayout* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 12, 12);
    main_layout->setSpacing(0);

    QWidget* left_panel = new QWidget(this);
    left_panel->setObjectName("sidebarPanel");
    left_panel->setFixedWidth(230);
    left_panel->setStyleSheet(
        "QWidget#sidebarPanel { background: #F6FAFE; border-right: 1px solid #E6EEF6; }");

    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(0);

    QWidget* compose_area = new QWidget(left_panel);
    QHBoxLayout* compose_layout = new QHBoxLayout(compose_area);
    compose_layout->setContentsMargins(18, 20, 18, 16);

    QPushButton* compose_btn = new QPushButton(QStringLiteral("  写信"), left_panel);
    compose_btn->setObjectName("composeSidebarButton");
    applyToolbarIcon(compose_btn, QStringLiteral("compose"), QColor("#FFFFFF"));
    compose_btn->setIconSize(QSize(18, 18));
    compose_btn->setMinimumHeight(48);
    compose_btn->setCursor(Qt::PointingHandCursor);
    auto* compose_shadow = new QGraphicsDropShadowEffect(compose_btn);
    compose_shadow->setBlurRadius(22);
    compose_shadow->setOffset(0, 6);
    compose_shadow->setColor(QColor(47, 128, 237, 54));
    compose_btn->setGraphicsEffect(compose_shadow);
    connect(compose_btn, &QPushButton::clicked, this, &MainWindow::on_compose);
    compose_layout->addWidget(compose_btn);
    left_layout->addWidget(compose_area);

    QFrame* sidebar_separator = new QFrame(left_panel);
    sidebar_separator->setFrameShape(QFrame::HLine);
    sidebar_separator->setStyleSheet("color: #E2ECF6; margin: 0 18px;");
    left_layout->addWidget(sidebar_separator);

    folder_tree_ = new QTreeWidget(left_panel);
    folder_tree_->setHeaderHidden(true);
    folder_tree_->setRootIsDecorated(false);
    folder_tree_->setIndentation(0);
    folder_tree_->setCursor(Qt::PointingHandCursor);

    inbox_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("收件箱")));
    sent_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("已发送")));
    drafts_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("草稿箱")));
    trash_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("废纸篓")));

    for (auto* item : {inbox_item_, sent_item_, drafts_item_, trash_item_}) {
        item->setSizeHint(0, QSize(0, 44));
    }

    folder_tree_->expandAll();
    folder_tree_->setCurrentItem(inbox_item_);
    left_layout->addWidget(folder_tree_, 1);

    QWidget* account_panel = new QWidget(left_panel);
    account_panel->setObjectName("accountBottom");
    account_panel->setStyleSheet(
        "QWidget#accountBottom { border-top: 1px solid #E2ECF6; }"
        "QLabel { color: #6F7F91; }"
        "QComboBox { background: #FFFFFF; color: #304154; font-size: 12px;"
        "border: 1px solid #D7E3EF; border-radius: 6px; padding: 7px 28px 7px 10px; }"
        "QComboBox:hover { border-color: #AFCBE8; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { font-size: 12px; }");

    QVBoxLayout* account_layout = new QVBoxLayout(account_panel);
    account_layout->setContentsMargins(18, 15, 18, 17);
    account_layout->setSpacing(8);

    QLabel* account_label = new QLabel(QStringLiteral("当前账号"), account_panel);
    account_label->setStyleSheet("font-size: 12px; color: #718196;");
    account_layout->addWidget(account_label);

    account_combo_ = new QComboBox(account_panel);
    account_combo_->setMinimumWidth(180);
    account_layout->addWidget(account_combo_);
    left_layout->addWidget(account_panel);

    main_layout->addWidget(left_panel);

    QFrame* content_surface = new QFrame(this);
    content_surface->setObjectName("contentSurface");
    auto* surface_shadow = new QGraphicsDropShadowEffect(content_surface);
    surface_shadow->setBlurRadius(24);
    surface_shadow->setOffset(0, 6);
    surface_shadow->setColor(QColor(31, 45, 61, 22));
    content_surface->setGraphicsEffect(surface_shadow);

    QVBoxLayout* surface_layout = new QVBoxLayout(content_surface);
    surface_layout->setContentsMargins(0, 0, 0, 0);
    surface_layout->setSpacing(0);

    mail_stack_ = new QStackedWidget(content_surface);
    mail_stack_->setObjectName("mailStack");

    list_page_ = new QWidget(mail_stack_);
    auto* list_layout = new QVBoxLayout(list_page_);
    list_layout->setContentsMargins(0, 0, 0, 0);
    list_layout->setSpacing(0);

    // 批量操作栏（默认隐藏）
    batch_bar_ = new QWidget(list_page_);
    batch_bar_->setObjectName("batchBar");
    batch_bar_->setFixedHeight(56);
    batch_bar_->setStyleSheet(
        "QWidget#batchBar { background: #F7FBFF; border-bottom: 1px solid #DDECF8; }");
    batch_bar_->setVisible(false);
    auto* batch_layout = new QHBoxLayout(batch_bar_);
    batch_layout->setContentsMargins(20, 0, 12, 0);
    batch_layout->setSpacing(10);

    batch_count_label_ = new QLabel(QStringLiteral("已选择 0 封邮件"), batch_bar_);
    batch_count_label_->setStyleSheet("color: #2F80ED; font-weight: 600; font-size: 13px;");
    batch_layout->addWidget(batch_count_label_);

    btn_batch_select_all_ = new QPushButton(QStringLiteral("全选"), batch_bar_);
    UiStyle::applyGhostButton(btn_batch_select_all_, 64);
    batch_layout->addWidget(btn_batch_select_all_);

    batch_layout->addStretch();

    btn_batch_mark_read_ = new QPushButton(QStringLiteral("标为已读"), batch_bar_);
    UiStyle::applySecondaryButton(btn_batch_mark_read_, 88);
    batch_layout->addWidget(btn_batch_mark_read_);

    btn_batch_restore_ = new QPushButton(QStringLiteral("批量恢复"), batch_bar_);
    UiStyle::applySecondaryButton(btn_batch_restore_, 88);
    btn_batch_restore_->setVisible(false);
    batch_layout->addWidget(btn_batch_restore_);

    btn_batch_delete_ = new QPushButton(QStringLiteral("批量删除"), batch_bar_);
    UiStyle::applyDangerButton(btn_batch_delete_, 88);
    batch_layout->addWidget(btn_batch_delete_);

    btn_batch_cancel_ = new QPushButton(QStringLiteral("取消"), batch_bar_);
    UiStyle::applyGhostButton(btn_batch_cancel_, 64);
    batch_layout->addWidget(btn_batch_cancel_);

    list_layout->addWidget(batch_bar_);

    email_list_ = new EmailListWidget(list_page_);
    list_layout->addWidget(email_list_);

    detail_page_ = new QWidget(mail_stack_);
    auto* detail_layout = new QVBoxLayout(detail_page_);
    detail_layout->setContentsMargins(0, 0, 0, 0);
    detail_layout->setSpacing(0);

    QWidget* detail_toolbar = new QWidget(detail_page_);
    detail_toolbar->setObjectName("detailToolbar");
    auto* detail_toolbar_layout = new QHBoxLayout(detail_toolbar);
    detail_toolbar_layout->setContentsMargins(18, 12, 18, 12);
    detail_toolbar_layout->setSpacing(8);

    btn_back_to_list_ = new QPushButton(QStringLiteral("返回列表"), detail_toolbar);
    applyToolbarIcon(btn_back_to_list_, QStringLiteral("back"), QColor("#53657A"));
    UiStyle::applyGhostButton(btn_back_to_list_, 96);
    detail_toolbar_layout->addWidget(btn_back_to_list_);
    detail_toolbar_layout->addStretch();
    detail_layout->addWidget(detail_toolbar);

    email_view_ = new EmailViewWidget(detail_page_);
    detail_layout->addWidget(email_view_, 1);

    mail_stack_->addWidget(list_page_);
    mail_stack_->addWidget(detail_page_);
    mail_stack_->setCurrentWidget(list_page_);
    surface_layout->addWidget(mail_stack_);

    main_layout->addWidget(content_surface, 1);

    connect(folder_tree_, &QTreeWidget::itemClicked, this, &MainWindow::on_folder_changed);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_account_changed);
    connect(email_list_, &EmailListWidget::email_selected, this, &MainWindow::on_email_selected);
    connect(email_list_, &EmailListWidget::load_more_requested, this, &MainWindow::on_load_more);
    connect(btn_back_to_list_, &QPushButton::clicked, this, &MainWindow::show_email_list_page);

    // 批量操作栏
    connect(btn_batch_select_all_, &QPushButton::clicked, this, &MainWindow::on_batch_select_all);
    connect(btn_batch_delete_, &QPushButton::clicked, this, &MainWindow::on_batch_delete);
    connect(btn_batch_restore_, &QPushButton::clicked, this, &MainWindow::on_batch_restore);
    connect(btn_batch_mark_read_, &QPushButton::clicked, this, &MainWindow::on_batch_mark_read);
    connect(btn_batch_cancel_, &QPushButton::clicked, this, &MainWindow::on_toggle_multi_select);
    connect(email_list_, &EmailListWidget::selection_changed, this, &MainWindow::on_selection_changed);

    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("color: #6B7280; font-size: 12px; padding: 2px 8px;");
    statusBar()->addWidget(status_label_);
    send_progress_bar_ = new QProgressBar(this);
    send_progress_bar_->setFixedWidth(190);
    send_progress_bar_->setRange(0, 100);
    send_progress_bar_->setValue(0);
    send_progress_bar_->setTextVisible(true);
    send_progress_bar_->setVisible(false);
    send_progress_bar_->setStyleSheet(
        "QProgressBar { border: 1px solid #D7E3EF; border-radius: 5px; "
        "height: 12px; text-align: center; color: #53657A; background: #F7FBFF; }"
        "QProgressBar::chunk { background: #2F80ED; border-radius: 4px; }");
    statusBar()->addPermanentWidget(send_progress_bar_);
    statusBar()->setStyleSheet("QStatusBar { background: #FFFFFF; border-top: 1px solid #E7EBF0; }");

    status_timer_ = new QTimer(this);
    status_timer_->setInterval(450);
    connect(status_timer_, &QTimer::timeout, this, &MainWindow::update_busy_status);
}

void MainWindow::start_busy_status(const QString& text) {
    status_busy_text_ = text;
    status_busy_step_ = 0;
    status_label_->setStyleSheet("color: #2F80ED; font-size: 12px; font-weight: 600; padding: 2px 8px;");
    update_busy_status();
    status_timer_->start();
}

void MainWindow::update_receive_progress(int current, int total) {
    if (total <= 0) {
        status_busy_text_ = QStringLiteral("正在获取邮件");
    } else {
        status_busy_text_ = QStringLiteral("正在获取第 %1/%2 封").arg(current).arg(total);
    }
    status_busy_step_ = 0;
    update_busy_status();
}

void MainWindow::begin_send_progress(const QString& text) {
    start_busy_status(text);
    if (!send_progress_bar_) return;
    send_progress_bar_->setRange(0, 0);
    send_progress_bar_->setFormat(QStringLiteral("准备中"));
    send_progress_bar_->setVisible(true);
}

void MainWindow::update_send_progress(size_t sent, size_t total) {
    if (!send_progress_bar_) return;
    if (total == 0) {
        send_progress_bar_->setRange(0, 0);
        send_progress_bar_->setFormat(QStringLiteral("准备中"));
        send_progress_bar_->setVisible(true);
        return;
    }
    send_progress_bar_->setRange(0, 100);
    const int value = qBound(0, static_cast<int>((sent * 100) / total), 100);
    send_progress_bar_->setValue(value);
    send_progress_bar_->setFormat(QStringLiteral("%1%").arg(value));
    send_progress_bar_->setVisible(true);
}

void MainWindow::finish_send_progress() {
    if (!send_progress_bar_) return;
    send_progress_bar_->setVisible(false);
    send_progress_bar_->setValue(0);
    send_progress_bar_->setFormat(QStringLiteral("%p%"));
}

void MainWindow::update_busy_status() {
    const QString dots(status_busy_step_ + 1, QLatin1Char('.'));
    status_label_->setText(status_busy_text_ + dots);
    status_busy_step_ = (status_busy_step_ + 1) % 3;
}

void MainWindow::set_status_neutral(const QString& text) {
    if (status_timer_) status_timer_->stop();
    finish_send_progress();
    status_label_->setStyleSheet("color: #6B7280; font-size: 12px; padding: 2px 8px;");
    status_label_->setText(text);
}

void MainWindow::set_status_success(const QString& text) {
    if (status_timer_) status_timer_->stop();
    finish_send_progress();
    status_label_->setStyleSheet("color: #15803D; font-size: 12px; font-weight: 600; padding: 2px 8px;");
    status_label_->setText(text);
}

void MainWindow::set_status_error(const QString& text) {
    if (status_timer_) status_timer_->stop();
    finish_send_progress();
    status_label_->setStyleSheet("color: #DC2626; font-size: 12px; font-weight: 600; padding: 2px 8px;");
    status_label_->setText(text);
}

void MainWindow::setup_toolbar() {
    toolbar_ = addToolBar(QStringLiteral("工具栏"));
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);

    QLabel* brand = new QLabel(QStringLiteral("邮件系统"), this);
    brand->setStyleSheet("font-size: 18px; font-weight: 700; color: #111827; padding: 0 10px 0 2px;");
    toolbar_->addWidget(brand);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addWidget(spacer);

    btn_receive_ = new QPushButton(QStringLiteral("刷新"), this);
    applyToolbarIcon(btn_receive_, QStringLiteral("refresh"), QColor("#2F80ED"));
    UiStyle::applyGhostButton(btn_receive_, 82);
    toolbar_->addWidget(btn_receive_);
    connect(btn_receive_, &QPushButton::clicked, this, &MainWindow::on_receive);

    btn_compose_ = new QPushButton(QStringLiteral("写信"), this);
    applyToolbarIcon(btn_compose_, QStringLiteral("compose"), QColor("#2F80ED"));
    UiStyle::applyGhostButton(btn_compose_, 82);
    toolbar_->addWidget(btn_compose_);
    connect(btn_compose_, &QPushButton::clicked, this, &MainWindow::on_compose);

    btn_reply_ = new QPushButton(QStringLiteral("回复"), this);
    applyToolbarIcon(btn_reply_, QStringLiteral("reply"), QColor("#4F6FD9"));
    UiStyle::applyGhostButton(btn_reply_, 82);
    toolbar_->addWidget(btn_reply_);
    connect(btn_reply_, &QPushButton::clicked, this, &MainWindow::on_reply);

    btn_delete_ = new QPushButton(QStringLiteral("删除"), this);
    applyToolbarIcon(btn_delete_, QStringLiteral("delete"), QColor("#D64545"));
    UiStyle::applyDangerGhostButton(btn_delete_, 82);
    toolbar_->addWidget(btn_delete_);
    connect(btn_delete_, &QPushButton::clicked, this, &MainWindow::on_delete_mail);

    btn_restore_ = new QPushButton(QStringLiteral("接收全部"), this);
    applyToolbarIcon(btn_restore_, QStringLiteral("receive"), QColor("#21A8A3"));
    UiStyle::applyGhostButton(btn_restore_, 96);
    btn_restore_->setVisible(true);
    toolbar_->addWidget(btn_restore_);
    connect(btn_restore_, &QPushButton::clicked, this, &MainWindow::on_restore_mail);

    // 分隔线
    QWidget* sep = new QWidget(this);
    sep->setFixedWidth(1);
    sep->setFixedHeight(24);
    sep->setStyleSheet("background: #E0E5EB;");
    toolbar_->addWidget(sep);

    btn_multi_select_ = new QPushButton(QStringLiteral("多选"), this);
    applyToolbarIcon(btn_multi_select_, QStringLiteral("multi"), QColor("#53657A"));
    UiStyle::applySecondaryButton(btn_multi_select_, 88);
    toolbar_->addWidget(btn_multi_select_);
    connect(btn_multi_select_, &QPushButton::clicked, this, &MainWindow::on_toggle_multi_select);
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
    if (email_list_->is_multi_select_mode()) {
        update_batch_buttons();
        on_selection_changed();  // 刷新计数
    }
}

void MainWindow::show_email_list_page() {
    if (mail_stack_ && list_page_) {
        mail_stack_->setCurrentWidget(list_page_);
    }
}

void MainWindow::load_emails(const std::string& folder) {
    show_email_list_page();
    current_offset_ = 0;  // 重置分页
    if (current_account_id_ < 0) {
        email_list_->clear(); email_view_->clear();
        total_count_ = 0;
        return;
    }

    std::vector<Email> emails;
    if (folder == "trash") {
        emails = db_mgr_->get_deleted_emails(current_account_id_, kPageSize, 0);
        total_count_ = db_mgr_->get_deleted_count(current_account_id_);
    } else {
        emails = db_mgr_->get_emails(current_account_id_, folder, kPageSize, 0);
        total_count_ = db_mgr_->get_email_count(current_account_id_, folder);
    }
    current_offset_ = static_cast<int>(emails.size());
    email_list_->set_emails(emails);
    email_view_->clear();

    QString name;
    if (folder == "inbox") name = QStringLiteral("收件箱");
    else if (folder == "sent") name = QStringLiteral("已发送");
    else if (folder == "drafts") name = QStringLiteral("草稿箱");
    else name = QStringLiteral("废纸篓");
    status_label_->setText(QStringLiteral("%1 — %2 封邮件").arg(name).arg(total_count_));
}

void MainWindow::update_folder_counts() {
    if (current_account_id_ < 0) return;
    int unread = db_mgr_->get_unread_count(current_account_id_, "inbox");
    inbox_item_->setText(0, unread > 0
        ? QStringLiteral("收件箱 (%1)").arg(unread) : QStringLiteral("收件箱"));
    int sent = db_mgr_->get_total_count(current_account_id_, "sent");
    sent_item_->setText(0, sent > 0 ? QStringLiteral("已发送 (%1)").arg(sent) : QStringLiteral("已发送"));
    int drafts = db_mgr_->get_total_count(current_account_id_, "drafts");
    drafts_item_->setText(0, drafts > 0 ? QStringLiteral("草稿箱 (%1)").arg(drafts) : QStringLiteral("草稿箱"));
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
    if (mail_stack_ && detail_page_) {
        mail_stack_->setCurrentWidget(detail_page_);
    }
    if (!email.is_read) { db_mgr_->mark_read(email_id); email_list_->mark_email_read(email_id); update_folder_counts(); }
}

void MainWindow::on_send_draft(int draft_id, const Email& updated) {
    Account acc = db_mgr_->get_account(current_account_id_);
    begin_send_progress(QStringLiteral("发送草稿中"));

    typedef std::pair<bool, std::string> SR;
    auto* w = new QFutureWatcher<SR>(this);
    connect(w, &QFutureWatcher<SR>::finished, [this, w, draft_id, updated, acc]() {
        SR r = w->result();
        if (r.first) {
            db_mgr_->mark_deleted(draft_id);
            Email sent = updated; sent.account_id = acc.id; sent.folder = "sent";
            sent = copy_attachments_for_sent(sent);
            auto now = std::time(nullptr); char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            sent.received_date = buf;
            int eid = db_mgr_->save_email(sent);
            if (eid > 0) {
                for (const auto& att : sent.attachments) {
                    db_mgr_->add_attachment(eid, att);
                }
            }
            load_emails(current_folder_); update_folder_counts();
            set_status_success(QStringLiteral("草稿已发送"));
        } else {
            QMessageBox::critical(this, QStringLiteral("发送失败"), from_utf8(r.second));
            set_status_error(QStringLiteral("发送失败 — %1").arg(from_utf8(r.second)));
            load_emails(current_folder_); update_folder_counts();
        }
        w->deleteLater();
    });
        w->setFuture(QtConcurrent::run([this, updated, acc]() -> SR {
            auto progress = [this](size_t sent, size_t total) {
                QMetaObject::invokeMethod(this, [this, sent, total]() {
                    update_send_progress(sent, total);
                }, Qt::QueuedConnection);
            };
            return send_email_with_progress(updated, acc, progress);
    }));
}

void MainWindow::on_receive() {
    receive_from_server(true);
}

void MainWindow::receive_from_server(bool only_new) {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加邮箱账号。")); return;
    }
    Account acc = db_mgr_->get_account(current_account_id_);
    auto known_uids = only_new ? db_mgr_->get_downloaded_uids(acc.id)
                               : std::unordered_set<std::string>();
    btn_receive_->setEnabled(false);
    btn_restore_->setEnabled(false);
    start_busy_status(only_new ? QStringLiteral("正在刷新邮件")
                               : QStringLiteral("正在接收全部邮件"));

    typedef std::pair<std::vector<Email>, std::string> RR;
    auto* w = new QFutureWatcher<RR>(this);
    connect(w, &QFutureWatcher<RR>::finished, [this, w, acc, only_new]() {
        RR r = w->result(); auto emails = r.first; std::string err = r.second;
        if (!err.empty() && emails.empty()) {
            QMessageBox::warning(this, QStringLiteral("接收失败"), from_utf8(err));
            set_status_error(QStringLiteral("接收失败 — %1").arg(from_utf8(err)));
        } else {
            MimeDecoder dec; int new_count = 0;
            int failed_count = 0;
            std::string first_save_error;
            QString attach_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                 + "/attachments";
            for (auto& email : emails) {
                std::string uid = email.pop3_uid;
                if (only_new && !uid.empty() && db_mgr_->is_uid_downloaded(acc.id, uid)) continue;

                int old_eid = -1;
                bool old_is_read = false;
                bool old_is_flagged = false;
                if (!only_new && !uid.empty()) {
                    old_eid = db_mgr_->get_email_id_by_uid(acc.id, uid);
                    if (old_eid > 0) {
                        Email old_email = db_mgr_->get_email(old_eid);
                        old_is_read = old_email.is_read;
                        old_is_flagged = old_email.is_flagged;
                    }
                }

                Email decoded = dec.decode(email.body_plain, acc.id, to_utf8(attach_dir));
                decoded.pop3_uid = uid;
                decoded.folder = "inbox";
                if (old_eid > 0) {
                    decoded.is_read = old_is_read;
                    decoded.is_flagged = old_is_flagged;
                }
                if (!only_new) {
                    decoded.is_deleted = false;
                }
                if (decoded.received_date.empty()) {
                    auto now = std::time(nullptr);
                    char buf[64];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                    decoded.received_date = buf;
                }

                int eid = db_mgr_->save_email(decoded);
                if (eid > 0) {
                    if (old_eid > 0) {
                        db_mgr_->delete_permanently(old_eid);
                    }
                    // 保存附件记录到数据库
                    for (const auto& att : decoded.attachments) {
                        db_mgr_->add_attachment(eid, att);
                    }
                    if (!uid.empty()) db_mgr_->mark_uid_downloaded(acc.id, uid, eid);
                    new_count++;
                } else {
                    failed_count++;
                    if (first_save_error.empty()) {
                        first_save_error = db_mgr_->last_error();
                    }
                }
            }
            if (!err.empty()) {
                QMessageBox::warning(this, QStringLiteral("部分邮件接收失败"),
                                     QStringLiteral("部分邮件未完整接收：%1").arg(from_utf8(err)));
                set_status_error((only_new
                    ? QStringLiteral("接收完成 — %1 封新邮件，部分邮件未完整接收")
                    : QStringLiteral("接收完成 — %1 封邮件已更新，部分邮件未完整接收"))
                                     .arg(new_count));
            } else if (failed_count > 0) {
                QString msg = QStringLiteral("已接收 %1 封，%2 封保存失败")
                                  .arg(new_count).arg(failed_count);
                if (!first_save_error.empty()) {
                    msg += QStringLiteral("\n\n数据库错误：%1").arg(from_utf8(first_save_error));
                }
                QMessageBox::warning(this, QStringLiteral("部分邮件保存失败"), msg);
                set_status_error((only_new
                    ? QStringLiteral("接收完成 — %1 封新邮件，%2 封保存失败")
                    : QStringLiteral("接收完成 — %1 封邮件已更新，%2 封保存失败"))
                                     .arg(new_count).arg(failed_count));
            } else {
                set_status_success((only_new
                    ? QStringLiteral("接收完成 — %1 封新邮件")
                    : QStringLiteral("接收完成 — %1 封邮件已更新")).arg(new_count));
            }
            load_emails(current_folder_); update_folder_counts();
        }
        btn_receive_->setEnabled(true);
        btn_restore_->setEnabled(true);
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, acc, only_new, known_uids]() -> RR {
        Pop3Client pop3; int nc = 0;
        pop3.set_progress_callback([this](int current, int total) {
            QMetaObject::invokeMethod(this, [this, current, total]() {
                update_receive_progress(current, total);
            }, Qt::QueuedConnection);
        });
        auto emails = pop3.receive_emails(acc.pop3_server, acc.pop3_port, acc.pop3_ssl,
                                          acc.username, acc.password, &nc,
                                          only_new ? &known_uids : nullptr);
        std::string err;
        err = pop3.get_last_error();
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
        begin_send_progress(QStringLiteral("发送中"));

        typedef std::pair<bool, std::string> SR;
        auto* w = new QFutureWatcher<SR>(this);
        connect(w, &QFutureWatcher<SR>::finished, [this, w, email, acc]() {
            SR r = w->result();
            if (r.first) {
                Email sent = email; sent.account_id = acc.id; sent.folder = "sent";
                sent = copy_attachments_for_sent(sent);
                auto now = std::time(nullptr); char buf[64];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                sent.received_date = buf;
                int eid = db_mgr_->save_email(sent);
                // 保存附件
                if (eid > 0) {
                    for (const auto& att : sent.attachments) {
                        db_mgr_->add_attachment(eid, att);
                    }
                }
                load_emails(current_folder_); update_folder_counts();
                set_status_success(QStringLiteral("发送成功"));
            } else {
                QMessageBox::critical(this, QStringLiteral("发送失败"), from_utf8(r.second));
                set_status_error(QStringLiteral("发送失败 — %1").arg(from_utf8(r.second)));
                load_emails(current_folder_); update_folder_counts();
            }
            w->deleteLater();
        });
        w->setFuture(QtConcurrent::run([this, email, acc]() -> SR {
            auto progress = [this](size_t sent, size_t total) {
                QMetaObject::invokeMethod(this, [this, sent, total]() {
                    update_send_progress(sent, total);
                }, Qt::QueuedConnection);
            };
            return send_email_with_progress(email, acc, progress);
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

        typedef std::pair<bool, std::string> SR;
        auto* w = new QFutureWatcher<SR>(this);
        connect(w, &QFutureWatcher<SR>::finished, [this, w]() {
            SR r = w->result();
            if (r.first) {
                set_status_success(QStringLiteral("回复已发送"));
            } else {
                QMessageBox::critical(this, QStringLiteral("发送失败"), from_utf8(r.second));
                set_status_error(QStringLiteral("回复发送失败 — %1").arg(from_utf8(r.second)));
            }
            w->deleteLater();
        });
        begin_send_progress(QStringLiteral("回复发送中"));
        w->setFuture(QtConcurrent::run([this, email, acc]() -> SR {
            auto progress = [this](size_t sent, size_t total) {
                QMetaObject::invokeMethod(this, [this, sent, total]() {
                    update_send_progress(sent, total);
                }, Qt::QueuedConnection);
            };
            return send_email_with_progress(email, acc, progress);
        }));
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
    receive_from_server(false);
    return;

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

void MainWindow::on_load_more() {
    if (current_account_id_ < 0) return;
    if (current_offset_ >= total_count_) return;  // 已全部加载

    std::vector<Email> emails;
    if (current_folder_ == "trash") {
        emails = db_mgr_->get_deleted_emails(current_account_id_, kPageSize, current_offset_);
    } else {
        emails = db_mgr_->get_emails(current_account_id_, current_folder_, kPageSize, current_offset_);
    }

    if (!emails.empty()) {
        current_offset_ += static_cast<int>(emails.size());
        email_list_->append_emails(emails);
    }
}

// ---------- 批量选中 ----------

void MainWindow::on_toggle_multi_select() {
    bool active = !email_list_->is_multi_select_mode();
    email_list_->set_multi_select_mode(active);

    if (active) {
        btn_multi_select_->setText(QStringLiteral("完成"));
        applyToolbarIcon(btn_multi_select_, QStringLiteral("done"), QColor("#FFFFFF"));
        UiStyle::applyPrimaryButton(btn_multi_select_, 88);
        batch_bar_->setVisible(true);
        // 根据当前文件夹调整批量按钮显示
        update_batch_buttons();
        status_label_->setText(QStringLiteral("多选模式 — 点击邮件勾选，使用底部按钮批量操作"));
    } else {
        btn_multi_select_->setText(QStringLiteral("多选"));
        applyToolbarIcon(btn_multi_select_, QStringLiteral("multi"), QColor("#53657A"));
        UiStyle::applySecondaryButton(btn_multi_select_, 88);
        batch_bar_->setVisible(false);
        status_label_->setText(QStringLiteral("就绪"));
    }
}

void MainWindow::update_batch_buttons() {
    // 废纸篓：显示"批量恢复"；其他文件夹仅显示"批量删除"
    bool is_trash = (current_folder_ == "trash");
    btn_batch_restore_->setVisible(is_trash);
    btn_batch_mark_read_->setVisible(!is_trash);
    // 废纸篓中"批量删除"变成"彻底删除"
    btn_batch_delete_->setText(is_trash ? QStringLiteral("彻底删除") : QStringLiteral("批量删除"));
    on_selection_changed();
}

void MainWindow::on_selection_changed() {
    int count = email_list_->selected_count();
    batch_count_label_->setText(QStringLiteral("已选择 %1 封邮件").arg(count));
    btn_batch_select_all_->setText(email_list_->all_selected()
        ? QStringLiteral("取消全选")
        : QStringLiteral("全选"));
    bool has_selection = (count > 0);
    btn_batch_delete_->setEnabled(has_selection);
    btn_batch_restore_->setEnabled(has_selection);
    btn_batch_mark_read_->setEnabled(has_selection);
}

void MainWindow::on_batch_select_all() {
    if (!email_list_->is_multi_select_mode()) return;
    email_list_->set_all_checked(!email_list_->all_selected());
}

void MainWindow::on_batch_delete() {
    std::vector<int> ids = email_list_->selected_email_ids();
    if (ids.empty()) {
        status_label_->setText(QStringLiteral("请先勾选要删除的邮件"));
        return;
    }

    if (current_folder_ == "trash") {
        // 废纸篓：彻底删除
        QString msg = QStringLiteral("确定要彻底删除选中的 %1 封邮件吗？\n此操作不可恢复！").arg(ids.size());
        if (QMessageBox::question(this, QStringLiteral("彻底删除"), msg,
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            for (int id : ids) {
                db_mgr_->delete_permanently(id);
            }
            status_label_->setText(QStringLiteral("已彻底删除 %1 封邮件").arg(ids.size()));
        }
    } else {
        // 其他文件夹：移入废纸篓
        QString msg = QStringLiteral("确定要删除选中的 %1 封邮件吗？\n（可在废纸篓中找回）").arg(ids.size());
        if (QMessageBox::question(this, QStringLiteral("批量删除"), msg,
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            for (int id : ids) {
                db_mgr_->mark_deleted(id);
            }
            status_label_->setText(QStringLiteral("已移入废纸篓 %1 封邮件").arg(ids.size()));
        }
    }
    load_emails(current_folder_);
    update_folder_counts();
}

void MainWindow::on_batch_restore() {
    std::vector<int> ids = email_list_->selected_email_ids();
    if (ids.empty()) {
        status_label_->setText(QStringLiteral("请先勾选要恢复的邮件"));
        return;
    }
    for (int id : ids) {
        db_mgr_->mark_undeleted(id);
    }
    status_label_->setText(QStringLiteral("已恢复 %1 封邮件").arg(ids.size()));
    load_emails(current_folder_);
    update_folder_counts();
}

void MainWindow::on_batch_mark_read() {
    std::vector<int> ids = email_list_->selected_email_ids();
    if (ids.empty()) {
        status_label_->setText(QStringLiteral("请先勾选要标记的邮件"));
        return;
    }
    for (int id : ids) {
        db_mgr_->mark_read(id);
    }
    email_list_->mark_emails_read(ids);
    email_list_->set_all_checked(false);
    update_folder_counts();
    status_label_->setText(QStringLiteral("已标为已读 %1 封邮件").arg(ids.size()));
    update_batch_buttons();
}
