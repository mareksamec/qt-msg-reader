#include "MainWindow.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>
#include <QTime>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_fileModel(new MsgFileModel(this))
    , m_attachmentModel(new AttachmentModel(this))
{
    setupUi();
    setupMenus();
    
    resize(1000, 700);
    setWindowTitle(tr("Qt MSG Reader"));
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    
    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, tr("About Qt MSG Reader"),
            tr("Qt MSG Reader v1.0\n\nA simple viewer for Microsoft Outlook MSG files."));
    });
}

void MainWindow::setupUi() {
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);
    
    m_fileBrowser = new QTreeView(m_mainSplitter);
    m_fileBrowser->setModel(m_fileModel);
    QModelIndex rootIndex = m_fileModel->setRootPath(QDir::homePath());
    m_fileBrowser->setRootIndex(rootIndex);
    m_fileBrowser->setWindowTitle(tr("File Browser"));
    m_fileBrowser->setMinimumWidth(200);
    m_fileBrowser->setSortingEnabled(true);
    m_fileBrowser->sortByColumn(0, Qt::AscendingOrder);
    m_fileBrowser->setAlternatingRowColors(true);
    connect(m_fileBrowser, &QTreeView::doubleClicked, this, &MainWindow::onFileDoubleClicked);
    
    m_contentSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
    
    m_messagePanel = new QWidget(m_contentSplitter);
    QVBoxLayout* messageLayout = new QVBoxLayout(m_messagePanel);
    messageLayout->setContentsMargins(8, 8, 8, 8);
    
    QGroupBox* headerGroup = new QGroupBox(tr("Message Header"), m_messagePanel);
    QGridLayout* headerLayout = new QGridLayout(headerGroup);
    
    int row = 0;
    headerLayout->addWidget(new QLabel(tr("<b>Subject:</b>")), row, 0);
    m_subjectLabel = new QLabel;
    m_subjectLabel->setWordWrap(true);
    m_subjectLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(m_subjectLabel, row, 1);
    
    ++row;
    headerLayout->addWidget(new QLabel(tr("<b>From:</b>")), row, 0);
    m_fromLabel = new QLabel;
    m_fromLabel->setWordWrap(true);
    m_fromLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(m_fromLabel, row, 1);
    
    ++row;
    headerLayout->addWidget(new QLabel(tr("<b>To:</b>")), row, 0);
    m_toLabel = new QLabel;
    m_toLabel->setWordWrap(true);
    m_toLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(m_toLabel, row, 1);
    
    ++row;
    headerLayout->addWidget(new QLabel(tr("<b>Cc:</b>")), row, 0);
    m_ccLabel = new QLabel;
    m_ccLabel->setWordWrap(true);
    m_ccLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(m_ccLabel, row, 1);
    
    ++row;
    headerLayout->addWidget(new QLabel(tr("<b>Date:</b>")), row, 0);
    m_dateLabel = new QLabel;
    m_dateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(m_dateLabel, row, 1);
    
    headerLayout->setColumnStretch(1, 1);
    messageLayout->addWidget(headerGroup);
    
    QGroupBox* bodyGroup = new QGroupBox(tr("Message Body"), m_messagePanel);
    QVBoxLayout* bodyLayout = new QVBoxLayout(bodyGroup);
    
    m_bodyView = new QTextEdit;
    m_bodyView->setReadOnly(true);
    bodyLayout->addWidget(m_bodyView);
    
    messageLayout->addWidget(bodyGroup, 1);
    
    m_contentSplitter->addWidget(m_messagePanel);
    
    QGroupBox* attachmentGroup = new QGroupBox(tr("Attachments"), m_contentSplitter);
    QVBoxLayout* attachmentLayout = new QVBoxLayout(attachmentGroup);
    
    m_attachmentView = new QTableView;
    m_attachmentView->setModel(m_attachmentModel);
    m_attachmentView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_attachmentView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_attachmentView->horizontalHeader()->setStretchLastSection(true);
    m_attachmentView->verticalHeader()->setVisible(false);
    m_attachmentView->setAlternatingRowColors(true);
    connect(m_attachmentView, &QTableView::doubleClicked, this, &MainWindow::onAttachmentDoubleClicked);
    
    attachmentLayout->addWidget(m_attachmentView);
    
    m_contentSplitter->addWidget(attachmentGroup);
    
    QGroupBox* statusGroup = new QGroupBox(tr("Status Log"), m_contentSplitter);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);
    
    m_statusLog = new QTextEdit;
    m_statusLog->setReadOnly(true);
    m_statusLog->setFont(QFont("monospace"));
    m_statusLog->setMaximumHeight(120);
    statusLayout->addWidget(m_statusLog);
    
    m_contentSplitter->addWidget(statusGroup);
    
    m_contentSplitter->setSizes({400, 100, 100});
    m_mainSplitter->setSizes({250, 750});
}

void MainWindow::loadFile(const QString& filePath) {
    log(tr("Loading file: %1").arg(filePath));
    
    MsgParser parser;
    EmailMessage msg = parser.parse(filePath);
    
    if (!msg.isValid) {
        logError(tr("Failed to parse file: %1").arg(msg.errorMessage));
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to open file: %1\n\n%2").arg(filePath, msg.errorMessage));
        return;
    }
    
    m_currentFile = filePath;
    m_currentMessage = msg;
    updateMessageView(msg);
    
    setWindowTitle(tr("Qt MSG Reader - %1").arg(QFileInfo(filePath).fileName()));
    log(tr("File loaded successfully"));
}

void MainWindow::updateMessageView(const EmailMessage& msg) {
    m_subjectLabel->setText(msg.subject.isEmpty() ? tr("(no subject)") : msg.subject);
    log(tr("Subject: %1").arg(msg.subject.isEmpty() ? tr("(no subject)") : msg.subject));
    
    QString fromText;
    if (!msg.senderName.isEmpty() && !msg.senderEmail.isEmpty()) {
        fromText = QString("%1 <%2>").arg(msg.senderName, msg.senderEmail);
    } else if (!msg.senderName.isEmpty()) {
        fromText = msg.senderName;
    } else if (!msg.senderEmail.isEmpty()) {
        fromText = msg.senderEmail;
    } else {
        fromText = tr("(unknown sender)");
    }
    m_fromLabel->setText(fromText);
    
    m_toLabel->setText(msg.toRecipients.isEmpty() ? tr("(no recipients)") : msg.toRecipients);
    m_ccLabel->setText(msg.ccRecipients.isEmpty() ? tr("-") : msg.ccRecipients);
    
    if (msg.date.isValid()) {
        m_dateLabel->setText(msg.date.toLocalTime().toString(Qt::ISODate));
    } else {
        m_dateLabel->setText(tr("(unknown date)"));
    }
    
    QString bodyText = msg.bodyHtml.isEmpty() ? msg.bodyPlainText : msg.bodyHtml;
    bodyText.remove('\0');
    
    if (!msg.bodyHtml.isEmpty()) {
        m_bodyView->setHtml(bodyText);
        log(tr("Body: HTML (%1 chars)").arg(msg.bodyHtml.length()));
    } else if (!msg.bodyPlainText.isEmpty()) {
        m_bodyView->setPlainText(bodyText);
        log(tr("Body: Plain text (%1 chars)").arg(msg.bodyPlainText.length()));
    } else {
        m_bodyView->setPlainText(tr("(no message body)"));
        logWarning(tr("No message body found"));
    }
    
    m_attachmentModel->setAttachments(msg.attachments);
    
    if (msg.attachments.isEmpty()) {
        m_attachmentView->hide();
        log(tr("Attachments: None"));
    } else {
        m_attachmentView->show();
        m_attachmentView->resizeColumnsToContents();
        log(tr("Attachments: %1 found").arg(msg.attachments.size()));
        for (const auto& att : msg.attachments) {
            log(tr("  - %1 (%2 bytes)").arg(att.filename).arg(att.size));
        }
    }
}

void MainWindow::onOpenFile() {
    QString filePath = QFileDialog::getOpenFileName(this,
        tr("Open MSG File"),
        QDir::homePath(),
        tr("MSG Files (*.msg *.MSG);;All Files (*)"));
    
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void MainWindow::onSaveAttachment() {
    QModelIndex index = m_attachmentView->currentIndex();
    if (!index.isValid()) return;
    
    const EmailAttachment& att = m_attachmentModel->attachment(index.row());
    
    QString savePath = QFileDialog::getSaveFileName(this,
        tr("Save Attachment"),
        QDir::homePath() + "/" + att.filename);
    
    if (!savePath.isEmpty()) {
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(att.data);
            file.close();
        } else {
            QMessageBox::warning(this, tr("Error"),
                tr("Failed to save attachment: %1").arg(savePath));
        }
    }
}

void MainWindow::onFileDoubleClicked(const QModelIndex& index) {
    QString filePath = m_fileModel->filePath(index);
    
    if (filePath.endsWith(".msg", Qt::CaseInsensitive)) {
        loadFile(filePath);
    }
}

void MainWindow::onAttachmentDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    
    const EmailAttachment& att = m_attachmentModel->attachment(index.row());
    
    QString savePath = QFileDialog::getSaveFileName(this,
        tr("Save Attachment"),
        QDir::homePath() + "/" + att.filename);
    
    if (!savePath.isEmpty()) {
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(att.data);
            file.close();
            log(tr("Saved attachment: %1").arg(savePath));
            QMessageBox::information(this, tr("Saved"),
                tr("Attachment saved to: %1").arg(savePath));
        } else {
            logError(tr("Failed to save attachment: %1").arg(savePath));
            QMessageBox::warning(this, tr("Error"),
                tr("Failed to save attachment: %1").arg(savePath));
        }
    }
}

void MainWindow::log(const QString& message) {
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    m_statusLog->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::logWarning(const QString& message) {
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    m_statusLog->append(QString("<span style='color: orange;'>[%1] WARNING: %2</span>").arg(timestamp, message));
}

void MainWindow::logError(const QString& message) {
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    m_statusLog->append(QString("<span style='color: red;'>[%1] ERROR: %2</span>").arg(timestamp, message));
}
