#ifndef DIALOGSTYLE_H
#define DIALOGSTYLE_H
#include <QString>

// Feuille de style commune aux boites de dialogue (meme charte que l'application : fond bleu nuit, cyan/violet)
inline QString hackflowDialogStyle()
{
    return QStringLiteral(R"(
QDialog { background-color: #00132F; }
QLabel { color: #D5DDF0; font-size: 10.5pt; background: transparent; }
QLabel#dlgTitle { color: #FFFFFF; font-size: 17pt; font-weight: bold; }
QLabel#dlgSub { color: #AEBBDD; font-size: 10.5pt; }
QLabel#dlgError { color: #F87171; background-color: rgba(239,68,68,0.12); border: 1px solid #EF4444; border-radius: 8px; padding: 8px 12px; }
QLabel#dlgInfo  { color: #8FD9FF; background-color: rgba(56,182,246,0.10); border: 1px solid rgba(56,182,246,0.6); border-radius: 8px; padding: 8px 12px; }
QLabel#dlgOk    { color: #34D399; background-color: rgba(16,185,129,0.12); border: 1px solid #10B981; border-radius: 8px; padding: 8px 12px; }
QLineEdit, QComboBox, QSpinBox { background-color: #05264F; border: 1px solid rgba(47,103,224,0.85); border-radius: 10px; padding: 9px 12px; color: #FFFFFF; font-size: 11pt; selection-background-color: #4B26A0; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #0BF8F6; }
QComboBox QAbstractItemView { background: #05264F; color: #FFFFFF; selection-background-color: #4B26A0; border: 1px solid #2F67E0; }
QCheckBox { color: #D5DDF0; font-size: 10pt; }
QPushButton { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #38ABFD, stop:0.5 #6D44FA, stop:1 #A04CF6); color: #FFFFFF; font-weight: bold; font-size: 10.5pt; border: none; border-radius: 10px; padding: 10px 18px; min-height: 20px; }
QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5BBDFF, stop:0.5 #8260FF, stop:1 #B565FF); }
QPushButton:disabled { background-color: #1B2B4F; color: #6F7FA8; }
QPushButton[flat="true"], QPushButton#linkBtn { background: transparent; color: #20B4F2; padding: 4px 6px; }
QPushButton#secondary { background-color: rgba(47,103,224,0.25); border: 1px solid #2F67E0; }
QPushButton#danger { background-color: rgba(239,68,68,0.30); border: 1px solid #EF4444; }
QTableWidget { background-color: #031A42; alternate-background-color: #05204F; color: #E6ECFA; gridline-color: transparent; border: 1px solid rgba(47,103,224,0.5); border-radius: 8px; selection-background-color: rgba(124,77,255,0.45); }
QHeaderView::section { background-color: #0A2A5E; color: #AEBBDD; border: none; padding: 6px; font-weight: bold; }
QProgressBar { background: #031A42; border: 1px solid rgba(47,103,224,0.5); border-radius: 6px; text-align: center; color: #FFFFFF; min-height: 14px; }
QProgressBar::chunk { background-color: #0BF8F6; border-radius: 5px; }
QGroupBox { color: #AEBBDD; border: 1px solid rgba(47,103,224,0.5); border-radius: 8px; margin-top: 10px; padding-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QToolTip { background-color: #05264F; color: #FFFFFF; border: 1px solid #2F67E0; }
)");
}
#endif
