#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QStringList>
#include <QSettings>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>

class QNetworkAccessManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void openRecentFile();
    void closeFile();
    void clearRecentFiles();
    void filterContacts(const QString &text);
    void exitApplication();
    void onContactSelected(int row);

private:
    // Setup methods (in mainwindow_setup.cpp)
    void setupUi();
    void setupMenuBar();
    void createStartPage();
    void createContactsView();
    void createDetailsPanel();

    // File operations (in mainwindow_file.cpp)
    bool loadVCardFile(const QString &filePath);
    void updateRecentFiles(const QString &filePath);
    void updateRecentFilesMenu();
    QStringList getRecentFiles();

    // vCard parsing (in mainwindow_parser.cpp)
    void parseVCard(const QString &content);

    // View management (in mainwindow_view.cpp)
    void showStartPage();
    void showContactsView();
    void updateContactsTable();
    void displayContactDetails(const QMap<QString, QString> &contact);

    // UI Components
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *contentLayout;
    QLabel *fileNameLabel;
    QWidget *searchContainer;
    QLineEdit *searchBar;
    QListWidget *contactsList;
    QWidget *startPageWidget;
    QLabel *startPageTitle;
    QLabel *startPageVersion;
    QLabel *startPageInstructions;
    QLabel *startPageCopyright;
    QScrollArea *detailsScrollArea;
    QWidget *detailsPanel;
    QVBoxLayout *detailsLayout;
    QLabel *loadingLabel;

    // Created on first use, and only when remote photo loading is enabled.
    QNetworkAccessManager *photoNetworkManager;

    // Menu components
    QMenu *fileMenu;
    QMenu *recentMenu;
    QAction *openAction;
    QAction *closeAction;
    QAction *exitAction;

    // Recent files actions
    static const int MaxRecentFiles = 10;
    QAction *recentFileActions[MaxRecentFiles];
    QAction *clearRecentAction;

    // Data storage
    QList<QMap<QString, QString>> contacts;
    QList<QMap<QString, QString>> filteredContacts;

    QString currentFilePath;
};

#endif // MAINWINDOW_H