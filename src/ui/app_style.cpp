#include "app_style.h"

#include <QPushButton>

namespace UiStyle {

QString globalStyle() {
    return QStringLiteral(R"(
        QMainWindow, QDialog {
            background: #F5F7FA;
        }
        QWidget {
            font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 14px;
            color: #1F2937;
        }
        QMenuBar {
            background: #FFFFFF;
            border-bottom: 1px solid #E7EBF0;
            padding: 3px 14px;
        }
        QMenuBar::item {
            padding: 7px 10px;
            border-radius: 6px;
        }
        QMenuBar::item:selected {
            background: #F0F4F8;
        }
        QMenu {
            background: #FFFFFF;
            border: 1px solid #DDE3EA;
            border-radius: 6px;
            padding: 6px 0;
        }
        QMenu::item {
            padding: 8px 34px 8px 16px;
        }
        QMenu::item:selected {
            background: #EEF4FF;
            color: #1677FF;
        }
        QToolBar {
            background: #FFFFFF;
            border: none;
            border-bottom: 1px solid #E7EBF0;
            padding: 8px 18px;
            spacing: 10px;
            min-height: 56px;
        }
        QPushButton {
            min-height: 34px;
            border-radius: 6px;
            padding: 7px 16px;
            font-weight: 500;
            border: 1px solid transparent;
        }
        QPushButton:disabled {
            color: #9CA3AF;
            background: #EEF2F6;
            border-color: transparent;
        }
        QPushButton#primaryBtn {
            color: #FFFFFF;
            background: #1677FF;
            border: 1px solid #1677FF;
        }
        QPushButton#primaryBtn:hover {
            background: #0F63D9;
            border-color: #0F63D9;
        }
        QPushButton#primaryBtn:pressed {
            background: #0B55BE;
            border-color: #0B55BE;
        }
        QPushButton#secondaryBtn {
            color: #2C3E50;
            background: #FFFFFF;
            border: 1px solid #DCE3EA;
        }
        QPushButton#secondaryBtn:hover {
            color: #1677FF;
            background: #F7FAFF;
            border-color: #BBD7FF;
        }
        QPushButton#secondaryBtn:pressed {
            background: #EDF5FF;
            border-color: #8FC0FF;
        }
        QPushButton#ghostBtn {
            color: #405266;
            background: transparent;
            border: 1px solid transparent;
        }
        QPushButton#ghostBtn:hover {
            color: #1677FF;
            background: #EEF5FF;
        }
        QPushButton#ghostBtn:pressed {
            background: #E2EEFF;
        }
        QPushButton#dangerBtn {
            color: #B91C1C;
            background: #FFFFFF;
            border: 1px solid #FFD3D3;
        }
        QPushButton#dangerBtn:hover {
            background: #FEF2F2;
            border-color: #FCA5A5;
        }
        QPushButton#dangerGhostBtn {
            color: #C24141;
            background: transparent;
            border: 1px solid transparent;
        }
        QPushButton#dangerGhostBtn:hover {
            color: #B91C1C;
            background: #FFF1F1;
        }
        QPushButton#dangerGhostBtn:pressed {
            background: #FFE3E3;
        }
        QPushButton#composeSidebarButton {
            color: #FFFFFF;
            background: #1677FF;
            border: 1px solid #1677FF;
            border-radius: 12px;
            padding: 10px 18px 10px 24px;
            min-height: 46px;
            font-size: 15px;
            font-weight: 700;
            text-align: left;
        }
        QPushButton#composeSidebarButton:hover {
            background: #2B85FF;
            border-color: #2B85FF;
        }
        QPushButton#composeSidebarButton:pressed {
            background: #0F63D9;
            border-color: #0F63D9;
        }
        QComboBox {
            background: #FFFFFF;
            border: 1px solid #DCE3EA;
            border-radius: 6px;
            padding: 6px 28px 6px 10px;
            min-height: 28px;
        }
        QComboBox:hover, QComboBox:focus {
            border-color: #93C5FD;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: #FFFFFF;
            border: 1px solid #DCE3EA;
            selection-background-color: #EEF4FF;
            selection-color: #1677FF;
            padding: 4px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox {
            background: #FFFFFF;
            border: 1px solid #DCE3EA;
            border-radius: 6px;
            padding: 8px 10px;
            selection-background-color: #BFDBFE;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus {
            border-color: #1677FF;
        }
        QGroupBox {
            background: #FFFFFF;
            border: 1px solid #E7EBF0;
            border-radius: 8px;
            margin-top: 14px;
            padding: 18px 16px 14px 16px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 6px;
            color: #374151;
        }
        QCheckBox {
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #9CA3AF;
            border-radius: 3px;
            background: #FFFFFF;
        }
        QCheckBox::indicator:checked {
            background: #1677FF;
            border-color: #1677FF;
        }
        QTreeWidget {
            background: #2C3E50;
            border: none;
            outline: none;
            padding: 8px 0;
        }
        QTreeWidget::item {
            min-height: 44px;
            padding: 0 18px 0 20px;
            color: #DCE6F1;
            border-left: 4px solid transparent;
        }
        QTreeWidget::item:hover:!selected {
            background: rgba(255, 255, 255, 0.08);
        }
        QTreeWidget::item:selected {
            color: #FFFFFF;
            background: rgba(22, 119, 255, 0.22);
            border-left: 4px solid #1677FF;
            font-weight: 600;
        }
        QListWidget, QListView {
            background: #FFFFFF;
            border: none;
            outline: none;
            padding: 6px 0;
        }
        QListWidget::item, QListView::item {
            border: none;
        }
        QTableWidget {
            background: #FFFFFF;
            border: none;
            gridline-color: #EEF2F6;
            outline: none;
            selection-background-color: #EAF2FF;
            selection-color: #111827;
        }
        QTableWidget::item {
            padding: 8px 12px;
            border-bottom: 1px solid #EEF1F5;
        }
        QTableWidget::item:selected {
            background: #EAF2FF;
        }
        QHeaderView::section {
            background: #F8FAFC;
            color: #6B7280;
            border: none;
            border-bottom: 1px solid #E7EBF0;
            padding: 10px 12px;
            font-weight: 600;
        }
        QSplitter::handle {
            background: #E7EBF0;
        }
        QStatusBar {
            background: #FFFFFF;
            border-top: 1px solid #E7EBF0;
            color: #6B7280;
        }
        QFrame#contentSurface {
            background: #FFFFFF;
            border: 1px solid #E7EBF0;
            border-radius: 10px;
        }
        QWidget#mailListHeader {
            background: #FFFFFF;
            border-bottom: 1px solid #EDF1F5;
        }
        QLabel#mailListTitle {
            color: #17212F;
            font-size: 15px;
            font-weight: 700;
        }
        QStackedWidget#mailStack {
            background: #FFFFFF;
            border: none;
        }
        QWidget#detailToolbar {
            background: #FFFFFF;
            border-bottom: 1px solid #EDF1F5;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #C7D0DB;
            border-radius: 4px;
            min-height: 44px;
        }
        QScrollBar::handle:vertical:hover {
            background: #97A6B7;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
            background: transparent;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #C7D0DB;
            border-radius: 4px;
            min-width: 44px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #97A6B7;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
            background: transparent;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )");
}

static void applyButton(QPushButton* button, const char* object_name, int min_width) {
    if (!button) return;
    button->setObjectName(QString::fromLatin1(object_name));
    button->setMinimumWidth(min_width);
    button->setCursor(Qt::PointingHandCursor);
}

void applyPrimaryButton(QPushButton* button, int min_width) {
    applyButton(button, "primaryBtn", min_width);
}

void applySecondaryButton(QPushButton* button, int min_width) {
    applyButton(button, "secondaryBtn", min_width);
}

void applyDangerButton(QPushButton* button, int min_width) {
    applyButton(button, "dangerBtn", min_width);
}

void applyGhostButton(QPushButton* button, int min_width) {
    applyButton(button, "ghostBtn", min_width);
}

void applyDangerGhostButton(QPushButton* button, int min_width) {
    applyButton(button, "dangerGhostBtn", min_width);
}

} // namespace UiStyle
