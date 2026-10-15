#include "mainwindow.h"
#include <QApplication>

const int MainWindow::MaxRecentFiles;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), photoNetworkManager(nullptr), currentFilePath("")
{
    setupUi();
    setupMenuBar();

    QStringList recentFiles = getRecentFiles();
    if (!recentFiles.isEmpty() && loadVCardFile(recentFiles.first())) {
        showContactsView();
    } else {
        showStartPage();
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::filterContacts(const QString &text)
{
    if (text.isEmpty()) {
        filteredContacts = contacts;
    } else {
        filteredContacts.clear();
        QString searchLower = text.toLower();

        for (const QMap<QString, QString> &contact : contacts) {
            bool matches = false;

            for (auto it = contact.constBegin(); it != contact.constEnd(); ++it) {
                // An embedded photo is tens of kilobytes of base64. Searching it
                // matches nothing anyone meant and costs a scan of every byte on
                // every keystroke, so the photo fields are skipped.
                if (it.key().startsWith("PHOTO")) {
                    continue;
                }
                if (it.value().toLower().contains(searchLower)) {
                    matches = true;
                    break;
                }
            }

            if (matches) {
                filteredContacts.append(contact);
            }
        }
    }

    updateContactsTable();
}

void MainWindow::exitApplication()
{
    close();
}

void MainWindow::onContactSelected(int row)
{
    if (row < 0 || row >= filteredContacts.size()) {
        return;
    }

    const QMap<QString, QString> &contact = filteredContacts[row];
    displayContactDetails(contact);
}