#include "mainwindow.h"
#include <QFont>

void MainWindow::setupUi()
{
    setWindowTitle("vCard Viewer");
    resize(1000, 600);

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Search bar container
    searchContainer = new QWidget();
    searchContainer->setStyleSheet("background-color: #2d2d30; padding: 8px;");
    QHBoxLayout *searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(10, 10, 10, 10);

    fileNameLabel = new QLabel();
    fileNameLabel->setStyleSheet("color: #cccccc; font-weight: bold; font-size: 16px;");
    searchLayout->addWidget(fileNameLabel);

    searchLayout->addStretch();

    searchBar = new QLineEdit();
    searchBar->setPlaceholderText("Search contacts...");
    searchBar->setMinimumHeight(30);
    searchBar->setMaximumWidth(330);
    connect(searchBar, &QLineEdit::textChanged, this, &MainWindow::filterContacts);

    searchLayout->addWidget(searchBar);

    mainLayout->addWidget(searchContainer);
    searchContainer->setVisible(false);

    // Content layout (contacts list + details panel)
    contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Create start page
    createStartPage();

    // Create contacts view
    createContactsView();

    // Create details panel
    createDetailsPanel();

    QWidget *contentWidget = new QWidget();
    contentWidget->setLayout(contentLayout);
    mainLayout->addWidget(contentWidget);

    // Loading indicator
    loadingLabel = new QLabel("Loading...");
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLabel->setStyleSheet("font-size: 20px; color: #007acc; padding: 20px;");
    loadingLabel->setVisible(false);
    mainLayout->addWidget(loadingLabel);
}

void MainWindow::createStartPage()
{
    startPageWidget = new QWidget();
    startPageWidget->setStyleSheet("background-color: #1e1e1e;");
    QVBoxLayout *layout = new QVBoxLayout(startPageWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);
    layout->setContentsMargins(40, 40, 40, 40);

    // Title
    startPageTitle = new QLabel("vCard Viewer 1.0");
    QFont titleFont = startPageTitle->font();
    titleFont.setPointSize(32);
    titleFont.setBold(true);
    startPageTitle->setFont(titleFont);
    startPageTitle->setAlignment(Qt::AlignCenter);
    startPageTitle->setStyleSheet("color: #007acc; margin-bottom: 20px;");

    // Instructions
    startPageInstructions = new QLabel(
        "To load a vCard file:\n"
        "Click Options -> Open File "
        );
    QFont instructFont = startPageInstructions->font();
    instructFont.setPointSize(11);
    startPageInstructions->setFont(instructFont);
    startPageInstructions->setAlignment(Qt::AlignCenter);
    startPageInstructions->setStyleSheet("color: #cccccc; line-height: 1.8;");
    startPageInstructions->setWordWrap(true);
    startPageInstructions->setMaximumWidth(600);

    layout->addWidget(startPageTitle);
    layout->addSpacing(10);
    layout->addWidget(startPageInstructions);

    contentLayout->addWidget(startPageWidget);
}

void MainWindow::createContactsView()
{
    contactsList = new QListWidget();
    contactsList->setMinimumWidth(250);
    contactsList->setMaximumWidth(350);

    contactsList->setStyleSheet(R"(
        QListWidget {
            background-color: #252526;
            border: 1px solid #3e3e42;
            border-right: 2px solid #3e3e42;
            outline: none;
            show-decoration-selected: 0;
        }
        QListWidget::item {
            padding: 12px;
            border-bottom: 1px solid #3e3e42;
            outline: none;
        }
        QListWidget::item:selected {
            background-color: #094771;
            border: none;
            outline: none;
        }
        QListWidget::item:focus {
            border: none;
            outline: none;
        }
        QListWidget::item:hover {
            background-color: #2a2d2e;
        }
    )");

    connect(contactsList, &QListWidget::currentRowChanged, this, &MainWindow::onContactSelected);

    contactsList->setVisible(false);
    contentLayout->addWidget(contactsList);
}

void MainWindow::createDetailsPanel()
{
    detailsScrollArea = new QScrollArea();
    detailsScrollArea->setWidgetResizable(true);
    detailsScrollArea->setFrameShape(QFrame::NoFrame);
    detailsScrollArea->setStyleSheet("background-color: #1e1e1e;");

    detailsPanel = new QWidget();
    detailsPanel->setStyleSheet("background-color: #1e1e1e;");

    detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(28, 24, 28, 24);
    detailsLayout->setSpacing(0);
    detailsLayout->setAlignment(Qt::AlignTop);

    detailsScrollArea->setWidget(detailsPanel);
    detailsScrollArea->setVisible(false);
    contentLayout->addWidget(detailsScrollArea);
}

void MainWindow::setupMenuBar()
{
    fileMenu = menuBar()->addMenu("&Options");

    openAction = new QAction("&Open File", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction(openAction);

    recentMenu = fileMenu->addMenu("Open &Recent");

    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentFileActions[i] = new QAction(this);
        recentFileActions[i]->setVisible(false);
        connect(recentFileActions[i], &QAction::triggered, this, &MainWindow::openRecentFile);
        recentMenu->addAction(recentFileActions[i]);
    }

    recentMenu->addSeparator();

    clearRecentAction = new QAction("Clear Recent Files", this);
    connect(clearRecentAction, &QAction::triggered, this, &MainWindow::clearRecentFiles);
    recentMenu->addAction(clearRecentAction);

    updateRecentFilesMenu();

    fileMenu->addSeparator();

    closeAction = new QAction("&Close File", this);
    closeAction->setShortcut(QKeySequence::Close);
    closeAction->setEnabled(false);
    connect(closeAction, &QAction::triggered, this, &MainWindow::closeFile);
    fileMenu->addAction(closeAction);

    fileMenu->addSeparator();

    exitAction = new QAction("E&xit App", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
    fileMenu->addAction(exitAction);
}
