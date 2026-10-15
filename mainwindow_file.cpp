#include "mainwindow.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QFileInfo>

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open vCard File",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        "vCard Files (*.vcf);;All Files (*.*)"
        );

    if (!fileName.isEmpty()) {
        loadingLabel->setVisible(true);
        startPageWidget->setVisible(false);
        contactsList->setVisible(false);
        detailsScrollArea->setVisible(false);
        QApplication::processEvents();

        loadVCardFile(fileName);
        updateRecentFiles(fileName);

        loadingLabel->setVisible(false);
        showContactsView();
    }
}

void MainWindow::openRecentFile()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action) {
        QString filePath = action->data().toString();

        loadingLabel->setVisible(true);
        startPageWidget->setVisible(false);
        contactsList->setVisible(false);
        detailsScrollArea->setVisible(false);
        QApplication::processEvents();

        loadVCardFile(filePath);
        updateRecentFiles(filePath);

        loadingLabel->setVisible(false);
        showContactsView();
    }
}

void MainWindow::closeFile()
{
    currentFilePath.clear();
    contacts.clear();
    filteredContacts.clear();
    contactsList->clear();
    searchBar->clear();
    fileNameLabel->clear();

    // Clear details panel
    QLayoutItem *item;
    while ((item = detailsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    closeAction->setEnabled(false);

    showStartPage();
}

void MainWindow::clearRecentFiles()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Clear Recent Files"),
                                  tr("Are you sure you want to clear the recent files list?"),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSettings settings;
        settings.remove("recentFiles");
        updateRecentFilesMenu();
    }
}

bool MainWindow::loadVCardFile(const QString &filePath)
{
    // File missing — silently prune from recents and fall back to start page
    if (!QFileInfo::exists(filePath)) {
        QSettings settings;
        QStringList files = settings.value("recentFiles").toStringList();
        files.removeAll(filePath);
        settings.setValue("recentFiles", files);
        updateRecentFilesMenu();
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filePath);
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    currentFilePath = filePath;
    fileNameLabel->setText(QFileInfo(filePath).fileName());
    closeAction->setEnabled(true);

    parseVCard(content);
    return true;
}

void MainWindow::updateRecentFiles(const QString &filePath)
{
    QSettings settings;
    QStringList files = settings.value("recentFiles").toStringList();

    files.removeAll(filePath);
    files.prepend(filePath);

    while (files.size() > MaxRecentFiles) {
        files.removeLast();
    }

    settings.setValue("recentFiles", files);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    QStringList files = getRecentFiles();

    int numRecentFiles = qMin(files.size(), MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = QString("&%1. %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
        recentFileActions[i]->setText(text);
        recentFileActions[i]->setData(files[i]);
        recentFileActions[i]->setVisible(true);
    }

    for (int i = numRecentFiles; i < MaxRecentFiles; ++i) {
        recentFileActions[i]->setVisible(false);
    }

    clearRecentAction->setEnabled(numRecentFiles > 0);
}

QStringList MainWindow::getRecentFiles()
{
    QSettings settings;
    return settings.value("recentFiles").toStringList();
}