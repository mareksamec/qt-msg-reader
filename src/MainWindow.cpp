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
#include <QRegularExpression>
#include <QMimeDatabase>
#include <QMimeType>
#include <QHash>
#include <QTextDocument>
#include <QSettings>
#include <QCloseEvent>
#include <QDir>
#include <QItemSelectionModel>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_fileModel(new MsgFileModel(this))
    , m_attachmentModel(new AttachmentModel(this))
{
    setupUi();
    setupMenus();

    resize(1000, 700);
    setWindowTitle(tr("Qt MSG Reader"));

    loadSettings();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupMenus() {
    // Create File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    
    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // Create Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, tr("About Qt MSG Reader"),
            tr("Qt MSG Reader v1.0\n\nA simple viewer for Microsoft Outlook MSG files."));
    });
}

void MainWindow::setupUi() {
    // Main horizontal splitter: file browser | content area
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);
    
    // File browser panel: a path editor (address bar) above the tree view
    QWidget* fileBrowserPanel = new QWidget(m_mainSplitter);
    QVBoxLayout* fileBrowserLayout = new QVBoxLayout(fileBrowserPanel);
    fileBrowserLayout->setContentsMargins(0, 0, 0, 0);
    fileBrowserLayout->setSpacing(2);

    m_pathEdit = new QLineEdit(fileBrowserPanel);
    m_pathEdit->setPlaceholderText(tr("Path..."));
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &MainWindow::onPathEditReturnPressed);
    fileBrowserLayout->addWidget(m_pathEdit);

    m_fileBrowser = new QTreeView(fileBrowserPanel);
    m_fileBrowser->setModel(m_fileModel);
    QModelIndex rootIndex = m_fileModel->setRootPath(QDir::homePath());
    m_fileBrowser->setRootIndex(rootIndex);
    m_fileBrowser->setWindowTitle(tr("File Browser"));
    m_fileBrowser->setMinimumWidth(200);
    m_fileBrowser->setSortingEnabled(true);
    m_fileBrowser->sortByColumn(0, Qt::AscendingOrder);
    m_fileBrowser->setAlternatingRowColors(true);
    connect(m_fileBrowser, &QTreeView::doubleClicked, this, &MainWindow::onFileDoubleClicked);
    connect(m_fileBrowser, &QTreeView::expanded, this, &MainWindow::onFileBrowserExpanded);
    connect(m_fileBrowser, &QTreeView::collapsed, this, &MainWindow::onFileBrowserCollapsed);
    connect(m_fileBrowser->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onFileBrowserCurrentChanged);
    connect(m_fileModel, &QFileSystemModel::directoryLoaded, this, &MainWindow::onDirectoryLoaded);
    fileBrowserLayout->addWidget(m_fileBrowser);

    // Vertical splitter for message content
    m_contentSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
    
    // Message panel with header and body
    m_messagePanel = new QWidget(m_contentSplitter);
    QVBoxLayout* messageLayout = new QVBoxLayout(m_messagePanel);
    messageLayout->setContentsMargins(8, 8, 8, 8);
    
    // Header section with labels
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
    
    // Body section
    QGroupBox* bodyGroup = new QGroupBox(tr("Message Body"), m_messagePanel);
    QVBoxLayout* bodyLayout = new QVBoxLayout(bodyGroup);

    m_bodyTabs = new QTabWidget;

    m_htmlBodyView = new QTextEdit;
    m_htmlBodyView->setReadOnly(true);
    m_bodyTabs->addTab(m_htmlBodyView, tr("HTML"));

    m_plainBodyView = new QTextEdit;
    m_plainBodyView->setReadOnly(true);
    m_bodyTabs->addTab(m_plainBodyView, tr("Plain Text"));

    bodyLayout->addWidget(m_bodyTabs);

    messageLayout->addWidget(bodyGroup, 1);
    
    m_contentSplitter->addWidget(m_messagePanel);
    
    // Attachments section
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
    
    // Status log section
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

static QString mimeTypeForAttachment(const EmailAttachment& att) {
    if (!att.mimeType.isEmpty()) return att.mimeType;

    QMimeDatabase db;
    QMimeType byName = db.mimeTypeForFile(att.filename, QMimeDatabase::MatchExtension);
    if (byName.isValid() && byName.name() != "application/octet-stream") return byName.name();

    QMimeType byData = db.mimeTypeForData(att.data);
    return byData.name();
}

QString MainWindow::resolveInlineImages(const QString& html, const QList<EmailAttachment>& attachments) const {
    if (html.isEmpty() || attachments.isEmpty()) return html;

    QHash<QString, int> attachmentByContentId;
    for (int i = 0; i < attachments.size(); i++) {
        QString cid = attachments[i].contentId.trimmed();
        if (cid.isEmpty()) continue;
        if (cid.startsWith('<') && cid.endsWith('>')) cid = cid.mid(1, cid.length() - 2);
        attachmentByContentId.insert(cid, i);
    }
    if (attachmentByContentId.isEmpty()) return html;

    static const QRegularExpression cidRef(
        R"((?:src|background)\s*=\s*(["'])cid:([^"'>]+)\1)",
        QRegularExpression::CaseInsensitiveOption);

    QString result;
    result.reserve(html.size());
    int lastPos = 0;
    QRegularExpressionMatchIterator it = cidRef.globalMatch(html);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        result += html.mid(lastPos, m.capturedStart() - lastPos);
        lastPos = m.capturedEnd();

        int index = attachmentByContentId.value(m.captured(2), -1);
        if (index < 0) {
            result += m.captured(0);
            continue;
        }

        const EmailAttachment& att = attachments[index];
        result += QString("src=\"data:%1;base64,%2\"")
                      .arg(mimeTypeForAttachment(att), QString::fromLatin1(att.data.toBase64()));
    }
    result += html.mid(lastPos);
    return result;
}

void MainWindow::updateMessageView(const EmailMessage& msg) {
    // Update subject
    m_subjectLabel->setText(msg.subject.isEmpty() ? tr("(no subject)") : msg.subject);
    log(tr("Subject: %1").arg(msg.subject.isEmpty() ? tr("(no subject)") : msg.subject));
    
    // Format sender display
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
    
    // Update recipients
    m_toLabel->setText(msg.toRecipients.isEmpty() ? tr("(no recipients)") : msg.toRecipients);
    m_ccLabel->setText(msg.ccRecipients.isEmpty() ? tr("-") : msg.ccRecipients);
    
    // Update date
    if (msg.date.isValid()) {
        m_dateLabel->setText(msg.date.toLocalTime().toString(Qt::ISODate));
    } else {
        m_dateLabel->setText(tr("(unknown date)"));
    }
    
    // Update body - HTML tab (default) and Plain Text tab
    QString htmlBody = msg.bodyHtml;
    htmlBody.remove('\0');
    QString plainBody = msg.bodyPlainText;
    plainBody.remove('\0');

    if (!htmlBody.isEmpty()) {
        QString htmlWithInlineImages = resolveInlineImages(htmlBody, msg.attachments);
        m_htmlBodyView->setHtml(htmlWithInlineImages);
        log(tr("Body: HTML (%1 chars)").arg(htmlBody.length()));
    } else {
        m_htmlBodyView->setPlainText(tr("(no message body)"));
        logWarning(tr("No message body found"));
    }

    if (!plainBody.isEmpty()) {
        m_plainBodyView->setPlainText(plainBody);
    } else if (!htmlBody.isEmpty()) {
        // No separate plain text part was stored; derive one from the HTML
        // so the Plain Text tab isn't just empty.
        QTextDocument doc;
        doc.setHtml(htmlBody);
        m_plainBodyView->setPlainText(doc.toPlainText());
    } else {
        m_plainBodyView->setPlainText(tr("(no message body)"));
    }

    m_bodyTabs->setCurrentIndex(0);
    
    // Update attachments
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

void MainWindow::onFileBrowserCurrentChanged(const QModelIndex& current) {
    if (!current.isValid()) return;
    m_pathEdit->setText(m_fileModel->filePath(current));
}

void MainWindow::onPathEditReturnPressed() {
    QString path = QDir::cleanPath(m_pathEdit->text().trimmed());
    if (path.isEmpty()) return;

    QFileInfo info(path);
    if (!info.exists()) {
        logWarning(tr("Path does not exist: %1").arg(path));
        return;
    }

    QModelIndex index = m_fileModel->index(path);
    if (!index.isValid()) {
        logWarning(tr("Path is not accessible: %1").arg(path));
        return;
    }

    m_fileBrowser->setCurrentIndex(index);
    m_fileBrowser->scrollTo(index);
    if (info.isDir()) {
        m_fileBrowser->setExpanded(index, true);
    } else if (path.endsWith(".msg", Qt::CaseInsensitive)) {
        loadFile(path);
    }
}

void MainWindow::onFileBrowserExpanded(const QModelIndex& index) {
    m_expandedPaths.insert(m_fileModel->filePath(index));
}

void MainWindow::onFileBrowserCollapsed(const QModelIndex& index) {
    m_expandedPaths.remove(m_fileModel->filePath(index));
}

void MainWindow::onDirectoryLoaded(const QString& path) {
    expandPendingPaths(path);
}

void MainWindow::expandPendingPaths(const QString& loadedPath) {
    if (m_pendingExpandedPaths.isEmpty()) return;

    QString cleanLoadedPath = QDir::cleanPath(loadedPath);
    const QList<QString> pending = m_pendingExpandedPaths.values();
    for (const QString& path : pending) {
        if (QDir::cleanPath(QFileInfo(path).path()) != cleanLoadedPath) continue;

        m_pendingExpandedPaths.remove(path);
        QModelIndex index = m_fileModel->index(path);
        if (index.isValid()) {
            // Expanding asks the model to fetch this folder's children, which will
            // eventually emit directoryLoaded(path) again and continue the cascade
            // into any of its own pending descendants.
            m_fileBrowser->setExpanded(index, true);
        }
    }
}

void MainWindow::loadSettings() {
    QSettings settings;

    settings.beginGroup("MainWindow");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("mainSplitterState")) {
        m_mainSplitter->restoreState(settings.value("mainSplitterState").toByteArray());
    }
    if (settings.contains("contentSplitterState")) {
        m_contentSplitter->restoreState(settings.value("contentSplitterState").toByteArray());
    }
    settings.endGroup();

    settings.beginGroup("FileBrowser");
    if (settings.contains("headerState")) {
        m_fileBrowser->header()->restoreState(settings.value("headerState").toByteArray());
    }
    QStringList expandedPaths = settings.value("expandedPaths").toStringList();
    settings.endGroup();

    if (!expandedPaths.isEmpty()) {
        m_pendingExpandedPaths = QSet<QString>(expandedPaths.begin(), expandedPaths.end());
        // The root directory may already be loaded by the time settings are restored
        // (or may still be loading, in which case onDirectoryLoaded() will pick this up).
        expandPendingPaths(m_fileModel->rootPath());
    }
}

void MainWindow::saveSettings() {
    QSettings settings;

    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("mainSplitterState", m_mainSplitter->saveState());
    settings.setValue("contentSplitterState", m_contentSplitter->saveState());
    settings.endGroup();

    settings.beginGroup("FileBrowser");
    settings.setValue("headerState", m_fileBrowser->header()->saveState());
    settings.setValue("expandedPaths", QStringList(m_expandedPaths.values()));
    settings.endGroup();
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
