#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application metadata
    app.setApplicationName("vCard Viewer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ManishMajhi");

    // Apply dark theme stylesheet
    QString darkStyleSheet = R"(
        QMainWindow {
            background-color: #1e1e1e;
        }
        QMenuBar {
            background-color: #2d2d30;
            color: #f1f1f1;
            border-bottom: 1px solid #3e3e42;
        }
        QMenuBar::item {
            background-color: transparent;
            padding: 4px 12px;
        }
        QMenuBar::item:selected {
            background-color: #3e3e42;
        }
        QMenuBar::item:pressed {
            background-color: #007acc;
        }
        QMenu {
            background-color: #2d2d30;
            color: #f1f1f1;
            border: 1px solid #3e3e42;
        }
        QMenu::item {
            padding: 6px 25px 6px 12px;
        }
        QMenu::item:selected {
            background-color: #3e3e42;
        }
        QMenu::separator {
            height: 1px;
            background-color: #3e3e42;
            margin: 4px 0px;
        }
        QLineEdit {
            background-color: #3c3c3c;
            color: #f1f1f1;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 6px;
            selection-background-color: #007acc;
        }
        QLineEdit:focus {
            border: 1px solid #007acc;
        }
        QTableWidget {
            background-color: #252526;
            color: #f1f1f1;
            gridline-color: #3e3e42;
            border: 1px solid #3e3e42;
            selection-background-color: #094771;
        }
        QHeaderView::section {
            background-color: #2d2d30;
            color: #f1f1f1;
            padding: 8px;
            border: none;
            border-right: 1px solid #3e3e42;
            border-bottom: 1px solid #3e3e42;
            font-weight: bold;
        }
        QScrollBar:vertical {
            background-color: #1e1e1e;
            width: 8px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background-color: black;
            min-height: 30px;
            border-radius: 7px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #1e1e1e;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #1e1e1e;
            height: 8px;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background-color: black;
            min-width: 30px;
            border-radius: 7px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #1e1e1e;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QLabel {
            color: #f1f1f1;
        }
        QTextEdit {
            background-color: #252526;
            color: #f1f1f1;
            border: 1px solid #3e3e42;
            selection-background-color: #007acc;
        }
    )";

    app.setStyleSheet(darkStyleSheet);
    app.setWindowIcon(QIcon(":/icons/icon.png"));

    MainWindow window;
    window.show();

    return app.exec();
}
