#include "mainwindow.h"
#include "vcard.h"

// Parsing itself lives in src/vcard.cpp, which has no Qt in it and is covered
// by tests/vcard_test.cpp. This is the adapter: text in, Qt containers out.
void MainWindow::parseVCard(const QString &content)
{
    contacts.clear();
    filteredContacts.clear();

    for (const vcard::Contact &parsed : vcard::parse(content.toStdString())) {
        QMap<QString, QString> contact;
        for (const auto &field : parsed) {
            contact[QString::fromStdString(field.first)] = QString::fromStdString(field.second);
        }
        contacts.append(contact);
    }

    filteredContacts = contacts;
    updateContactsTable();
}
