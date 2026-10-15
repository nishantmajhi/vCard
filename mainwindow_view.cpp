#include "mainwindow.h"
#include <QFont>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSettings>
#include <QtGlobal>

namespace {

const char *const kPhotoFrameStyle =
    "border: 2px solid #3e3e42; border-radius: 60px; background-color: #2d2d30;";
const char *const kPhotoPlaceholderStyle =
    "border: 2px solid #3e3e42; border-radius: 60px; background-color: #2d2d30; color: #858585;";

// A contact file can name any URL in its PHOTO field, so fetching one tells
// whoever sent you the file your IP address and the moment you opened it.
// Remote photos are therefore off unless the user turns them on, and even then
// only over https. Embedded photos — the form phones actually export — need no
// network at all and are always shown.
bool remotePhotosAllowed()
{
    QSettings settings;
    return settings.value("photos/allowRemote", false).toBool();
}

const int kPhotoTimeoutMs = 10000;
const qint64 kMaxPhotoBytes = 5 * 1024 * 1024;

void setPhotoPlaceholder(QLabel *label, const QString &text)
{
    label->setText(text);
    label->setStyleSheet(kPhotoPlaceholderStyle);
}

// Scales to fill the 120px frame and clips to a circle.
QPixmap circularPhoto(const QPixmap &source)
{
    QPixmap scaled = source.scaled(120, 120, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap rounded(120, 120);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addEllipse(0, 0, 120, 120);
    painter.setClipPath(path);

    painter.drawPixmap((120 - scaled.width()) / 2, (120 - scaled.height()) / 2, scaled);
    return rounded;
}

} // namespace

void MainWindow::showStartPage()
{
    searchContainer->setVisible(false);
    searchBar->setEnabled(false);
    contactsList->setVisible(false);
    detailsScrollArea->setVisible(false);
    startPageWidget->setVisible(true);
}

void MainWindow::showContactsView()
{
    searchContainer->setVisible(true);
    searchBar->setEnabled(true);
    startPageWidget->setVisible(false);
    contactsList->setVisible(true);
    detailsScrollArea->setVisible(true);
}

void MainWindow::updateContactsTable()
{
    contactsList->clear();

    for (int i = 0; i < filteredContacts.size(); ++i) {
        const QMap<QString, QString> &contact = filteredContacts[i];
        QString name = contact.value("NAME", "Unknown Contact");
        contactsList->addItem(name);
    }

    if (filteredContacts.size() > 0) {
        contactsList->setCurrentRow(0);
    }
}

void MainWindow::displayContactDetails(const QMap<QString, QString> &contact)
{
    // Clear existing details
    QLayoutItem *item;
    while ((item = detailsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Photo section (if available)
    if (contact.contains("PHOTO")) {
        const QString photoKind = contact.value("PHOTO_KIND");
        const QString photoValue = contact.value("PHOTO");

        QLabel *photoLabel = new QLabel(detailsPanel);
        photoLabel->setAlignment(Qt::AlignCenter);
        photoLabel->setFixedSize(120, 120);
        photoLabel->setStyleSheet(kPhotoFrameStyle);

        if (photoKind == "embedded") {
            // The usual case: the image came with the file, so decode it here.
            const QByteArray imageData = QByteArray::fromBase64(photoValue.toLatin1());
            QPixmap pixmap;
            if (!imageData.isEmpty() && pixmap.loadFromData(imageData)) {
                photoLabel->setPixmap(circularPhoto(pixmap));
            } else {
                setPhotoPlaceholder(photoLabel, "No Image");
            }
        } else if (photoKind == "remote" && remotePhotosAllowed()
                   && QUrl(photoValue).scheme().compare("https", Qt::CaseInsensitive) == 0) {
            if (!photoNetworkManager) {
                photoNetworkManager = new QNetworkAccessManager(this);
            }
            QNetworkRequest request{QUrl(photoValue)};
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            request.setTransferTimeout(kPhotoTimeoutMs);
#endif
            QNetworkReply *reply = photoNetworkManager->get(request);

            // Stop a reply that turns out to be something other than a photo
            // before it is all in memory.
            connect(reply, &QNetworkReply::downloadProgress, reply,
                    [reply](qint64 received, qint64 total) {
                        if (received > kMaxPhotoBytes || total > kMaxPhotoBytes) {
                            reply->abort();
                        }
                    });

            // Use QPointer to safely handle widget deletion
            QPointer<QLabel> safePhotoLabel(photoLabel);

            connect(reply, &QNetworkReply::finished, detailsPanel, [reply, safePhotoLabel]() {
                if (!safePhotoLabel) {
                    reply->deleteLater();
                    return;
                }

                QPixmap pixmap;
                if (reply->error() != QNetworkReply::NoError) {
                    setPhotoPlaceholder(safePhotoLabel, "Failed");
                } else if (pixmap.loadFromData(reply->readAll())) {
                    safePhotoLabel->setPixmap(circularPhoto(pixmap));
                } else {
                    setPhotoPlaceholder(safePhotoLabel, "No Image");
                }
                reply->deleteLater();
            });
        } else {
            // A remote photo that will not be fetched: either the setting is
            // off or the URL is not https. Nothing leaves the machine.
            setPhotoPlaceholder(photoLabel, "No Image");
        }

        detailsLayout->addWidget(photoLabel);
        detailsLayout->addSpacing(16);
    }

    // Name header
    QString name = contact.value("FN", contact.value("NAME", "Unknown Contact"));
    QLabel *nameLabel = new QLabel(name);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(16);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setStyleSheet("color: #cccccc; padding-bottom: 16px; border-bottom: 1px solid #2d2d30;");
    detailsLayout->addWidget(nameLabel);

    detailsLayout->addSpacing(16);

    // Phone numbers section
    QStringList phoneKeys = contact.keys().filter(QRegularExpression("^PHONE_"));
    if (!phoneKeys.isEmpty()) {
        QLabel *phoneHeader = new QLabel("PHONE");
        QFont headerFont = phoneHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        phoneHeader->setFont(headerFont);
        phoneHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(phoneHeader);

        for (const QString &key : phoneKeys) {
            QString value = contact.value(key).trimmed();
            if (!value.isEmpty()) {
                // Extract phone type from key dynamically
                QString phoneType;
                QRegularExpression typeRegex("TYPE=([^;:]+)");
                QRegularExpressionMatchIterator matches = typeRegex.globalMatch(key);

                QStringList types;
                while (matches.hasNext()) {
                    QRegularExpressionMatch match = matches.next();
                    QString type = match.captured(1);
                    if (!type.isEmpty() && type.compare("VOICE", Qt::CaseInsensitive) != 0) {
                        types.append(type.left(1).toUpper() + type.mid(1).toLower());
                    }
                }

                phoneType = types.isEmpty() ? "Phone" : types.join(", ");

                QLabel *phoneLabel = new QLabel(QString("  %1: %2").arg(phoneType, value));
                phoneLabel->setStyleSheet("color: #cccccc; padding-left: 0px; padding-top: 4px;");
                detailsLayout->addWidget(phoneLabel);
            }
        }
        detailsLayout->addSpacing(16);
    }

    // Email addresses section
    QStringList emailKeys = contact.keys().filter(QRegularExpression("^EMAIL_"));
    if (!emailKeys.isEmpty()) {
        QLabel *emailHeader = new QLabel("EMAIL");
        QFont headerFont = emailHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        emailHeader->setFont(headerFont);
        emailHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(emailHeader);

        for (const QString &key : emailKeys) {
            QString value = contact.value(key).trimmed();
            if (!value.isEmpty()) {
                // Extract email type from key dynamically
                QString emailType;
                QRegularExpression typeRegex("TYPE=([^;:]+)");
                QRegularExpressionMatchIterator matches = typeRegex.globalMatch(key);

                QStringList types;
                while (matches.hasNext()) {
                    QRegularExpressionMatch match = matches.next();
                    QString type = match.captured(1);
                    if (!type.isEmpty() && type.compare("INTERNET", Qt::CaseInsensitive) != 0) {
                        types.append(type.left(1).toUpper() + type.mid(1).toLower());
                    }
                }

                emailType = types.isEmpty() ? "Email" : types.join(", ");

                QLabel *emailLabel = new QLabel(QString("  %1: %2").arg(emailType, value));
                emailLabel->setStyleSheet("color: #cccccc; padding-left: 0px; padding-top: 4px;");
                detailsLayout->addWidget(emailLabel);
            }
        }
        detailsLayout->addSpacing(16);
    }

    // Organization
    if (contact.contains("ORG")) {
        QLabel *orgHeader = new QLabel("ORGANIZATION");
        QFont headerFont = orgHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        orgHeader->setFont(headerFont);
        orgHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(orgHeader);

        QLabel *orgValue = new QLabel("  " + contact.value("ORG"));
        orgValue->setStyleSheet("color: #cccccc; padding-top: 4px;");
        detailsLayout->addWidget(orgValue);
        detailsLayout->addSpacing(16);
    }

    // Title
    if (contact.contains("TITLE")) {
        QLabel *titleHeader = new QLabel("TITLE");
        QFont headerFont = titleHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        titleHeader->setFont(headerFont);
        titleHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(titleHeader);

        QLabel *titleValue = new QLabel("  " + contact.value("TITLE"));
        titleValue->setStyleSheet("color: #cccccc; padding-top: 4px;");
        detailsLayout->addWidget(titleValue);
        detailsLayout->addSpacing(16);
    }

    // Address
    if (contact.contains("ADR")) {
        QLabel *adrHeader = new QLabel("ADDRESS");
        QFont headerFont = adrHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        adrHeader->setFont(headerFont);
        adrHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(adrHeader);

        QString address = contact.value("ADR").replace(";", ", ");
        QLabel *adrValue = new QLabel("  " + address);
        adrValue->setStyleSheet("color: #cccccc; padding-top: 4px;");
        adrValue->setWordWrap(true);
        detailsLayout->addWidget(adrValue);
        detailsLayout->addSpacing(16);
    }

    // Birthday
    if (contact.contains("BDAY")) {
        QLabel *bdayHeader = new QLabel("BIRTHDAY");
        QFont headerFont = bdayHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        bdayHeader->setFont(headerFont);
        bdayHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(bdayHeader);

        QLabel *bdayValue = new QLabel("  " + contact.value("BDAY"));
        bdayValue->setStyleSheet("color: #cccccc; padding-top: 4px;");
        detailsLayout->addWidget(bdayValue);
        detailsLayout->addSpacing(16);
    }

    // Website
    if (contact.contains("URL")) {
        QLabel *urlHeader = new QLabel("WEBSITE");
        QFont headerFont = urlHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        urlHeader->setFont(headerFont);
        urlHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(urlHeader);

        QLabel *urlValue = new QLabel("  " + contact.value("URL"));
        urlValue->setStyleSheet("color: #cccccc; padding-top: 4px;");
        detailsLayout->addWidget(urlValue);
        detailsLayout->addSpacing(16);
    }

    // Categories
    if (contact.contains("CATEGORIES")) {
        QLabel *catHeader = new QLabel("CATEGORIES");
        QFont headerFont = catHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        catHeader->setFont(headerFont);
        catHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(catHeader);

        // Create container for tags
        QWidget *tagsContainer = new QWidget();
        QHBoxLayout *tagsLayout = new QHBoxLayout(tagsContainer);
        tagsLayout->setContentsMargins(0, 4, 0, 0);
        tagsLayout->setSpacing(8);
        tagsLayout->setAlignment(Qt::AlignLeft);

        QStringList categories = contact.value("CATEGORIES").split(",", Qt::SkipEmptyParts);
        for (const QString &category : categories) {
            QLabel *tag = new QLabel(category.trimmed());

            // Set styling using stylesheet
            tag->setStyleSheet("background-color: #2d2d30; color: #cccccc; padding: 4px 12px; border: none;");

            QFont tagFont = tag->font();
            tagFont.setPointSize(9);
            tag->setFont(tagFont);

            tagsLayout->addWidget(tag);
        }
        tagsLayout->addStretch();
        detailsLayout->addWidget(tagsContainer);
        detailsLayout->addSpacing(16);
    }

    // Note
    if (contact.contains("NOTE")) {
        detailsLayout->addSpacing(4);

        QFrame *divider = new QFrame();
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet("background-color: #2d2d30;");
        divider->setFixedHeight(1);
        detailsLayout->addWidget(divider);

        detailsLayout->addSpacing(16);

        QLabel *noteHeader = new QLabel("NOTE");
        QFont headerFont = noteHeader->font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        noteHeader->setFont(headerFont);
        noteHeader->setStyleSheet("color: #858585; letter-spacing: 1px;");
        detailsLayout->addWidget(noteHeader);

        QLabel *noteValue = new QLabel(contact.value("NOTE"));
        noteValue->setStyleSheet("color: #cccccc; padding-top: 6px;");
        noteValue->setWordWrap(true);
        detailsLayout->addWidget(noteValue);
    }

    // Footer with vCard version
    if (contact.contains("VERSION")) {
        detailsLayout->addStretch();

        QFrame *footerDivider = new QFrame();
        footerDivider->setFrameShape(QFrame::HLine);
        footerDivider->setStyleSheet("background-color: #2d2d30;");
        footerDivider->setFixedHeight(1);
        detailsLayout->addWidget(footerDivider);

        QLabel *versionLabel = new QLabel("vCard " + contact.value("VERSION"));
        QFont versionFont = versionLabel->font();
        versionFont.setPointSize(9);
        versionLabel->setFont(versionFont);
        versionLabel->setAlignment(Qt::AlignRight);
        versionLabel->setStyleSheet("color: #6a6a6a; padding-top: 8px;");
        detailsLayout->addWidget(versionLabel);
    }

    // Add final stretch to push content to top
    detailsLayout->addStretch();
}
