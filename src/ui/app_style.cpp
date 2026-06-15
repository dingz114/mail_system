#include "app_style.h"

#include <QPushButton>

namespace UiStyle {

QString globalStyle() {
    return QStringLiteral(R"(
        QMainWindow, QDialog {
            background: #F4F6F8;
        }
        QWidget {
            font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 14px;
            color: #1F2937;
        }
        QMenuBar {
            background: #FFFFFF;
            border-bottom: 1px solid #E5E7EB;
            padding: 3px 14px;
        }
        QMenuBar::item {
            padding: 7px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background: #F3F4F6;
        }
        QMenu {
            background: #FFFFFF;
            border: 1px solid #DADFE7;
            border-radius: 6px;
            padding: 6px 0;
        }
        QMenu::item {
            padding: 8px 34px 8px 16px;
        }
        QMenu::item:selected {
            background: #EEF4FF;
            color: #1457D9;
        }
        QToolBar {
            background: #FFFFFF;
            border: none;
            border-bottom: 1px solid #E5E7EB;
            padding: 8px 18px;
            spacing: 10px;
            min-height: 56px;
        }
        QPushButton {
            min-height: 34px;
            border-radius: 6px;
            padding: 7px 16px;
            font-weight: 500;
        }
        QPushButton:disabled {
            color: #9CA3AF;
            background: #F3F4F6;
            border-color: #E5E7EB;
        }
        QPushButton#primaryBtn {
            color: #FFFFFF;
            background: #2563EB;
            border: 1px solid #2563EB;
        }
        QPushButton#primaryBtn:hover {
            background: #1D4ED8;
            border-color: #1D4ED8;
        }
        QPushButton#secondaryBtn {
            color: #374151;
            background: #FFFFFF;
            border: 1px solid #D1D5DB;
        }
        QPushButton#secondaryBtn:hover {
            color: #1D4ED8;
            background: #F8FAFC;
            border-color: #93C5FD;
        }
        QPushButton#ghostBtn {
            color: #374151;
            background: transparent;
            border: 1px solid transparent;
        }
        QPushButton#ghostBtn:hover {
            color: #1D4ED8;
            background: #EFF6FF;
        }
        QPushButton#dangerBtn {
            color: #B91C1C;
            background: #FFFFFF;
            border: 1px solid #FECACA;
        }
        QPushButton#dangerBtn:hover {
            background: #FEF2F2;
            border-color: #FCA5A5;
        }
        QComboBox {
            background: #FFFFFF;
            border: 1px solid #D1D5DB;
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
            border: 1px solid #D1D5DB;
            selection-background-color: #EEF4FF;
            selection-color: #1D4ED8;
            padding: 4px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox {
            background: #FFFFFF;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            padding: 8px 10px;
            selection-background-color: #BFDBFE;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus {
            border-color: #2563EB;
        }
        QGroupBox {
            background: #FFFFFF;
            border: 1px solid #E5E7EB;
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
            background: #2563EB;
            border-color: #2563EB;
        }
        QTreeWidget {
            background: #FFFFFF;
            border: none;
            outline: none;
        }
        QTreeWidget::item {
            min-height: 44px;
            padding: 0 18px;
            color: #4B5563;
            border-left: 3px solid transparent;
        }
        QTreeWidget::item:hover:!selected {
            background: #F3F6FA;
        }
        QTreeWidget::item:selected {
            color: #1D4ED8;
            background: #EFF6FF;
            border-left: 3px solid #2563EB;
            font-weight: 600;
        }
        QTableWidget {
            background: #FFFFFF;
            border: none;
            gridline-color: #EEF1F5;
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
            border-bottom: 1px solid #E5E7EB;
            padding: 10px 12px;
            font-weight: 600;
        }
        QSplitter::handle {
            background: #E5E7EB;
        }
        QStatusBar {
            background: #FFFFFF;
            border-top: 1px solid #E5E7EB;
            color: #6B7280;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 9px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #CBD5E1;
            border-radius: 4px;
            min-height: 40px;
        }
        QScrollBar::handle:vertical:hover {
            background: #94A3B8;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
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

} // namespace UiStyle
