#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QSplitter>
#include <QTabWidget>
#include <QSet>
#include "MsgParser.h"
#include "MsgFileModel.h"
#include "AttachmentModel.h"

class QCloseEvent;

/**
 * Main application window for viewing MSG email files.
 * Provides a file browser, message header display, body viewer, attachments table, and status log.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    /** Loads and displays an MSG file. */
    void loadFile(const QString& filePath);

protected:
    /** Persists window/layout state before the window closes. */
    void closeEvent(QCloseEvent* event) override;

private slots:
    /** Opens file dialog to select an MSG file. */
    void onOpenFile();
    /** Saves the currently selected attachment. */
    void onSaveAttachment();
    /** Handles double-click on a file in the browser. */
    void onFileDoubleClicked(const QModelIndex& index);
    /** Handles double-click on an attachment to save it. */
    void onAttachmentDoubleClicked(const QModelIndex& index);
    /** Updates the path editor to reflect the newly selected browser item. */
    void onFileBrowserCurrentChanged(const QModelIndex& current);
    /** Navigates the file browser to (and, for .msg files, opens) the typed/pasted path. */
    void onPathEditReturnPressed();
    /** Tracks a folder being expanded so its state can be restored on restart. */
    void onFileBrowserExpanded(const QModelIndex& index);
    /** Tracks a folder being collapsed so its state can be restored on restart. */
    void onFileBrowserCollapsed(const QModelIndex& index);
    /** Expands any previously-expanded folders as the async file system model finishes loading them. */
    void onDirectoryLoaded(const QString& path);

private:
    /** Sets up the UI layout and widgets. */
    void setupUi();
    /** Creates the menu bar with File and Help menus. */
    void setupMenus();
    /** Updates the message view with parsed email data. */
    void updateMessageView(const EmailMessage& msg);
    /** Replaces "cid:" image references in HTML with inline data: URIs sourced
     *  from matching attachments' PR_ATTACH_CONTENT_ID, so inline images show
     *  in the body instead of only appearing in the attachments list. */
    QString resolveInlineImages(const QString& html, const QList<EmailAttachment>& attachments) const;
    /** Logs a message to the status log with timestamp. */
    void log(const QString& message);
    /** Logs a warning message (orange) to the status log. */
    void logWarning(const QString& message);
    /** Logs an error message (red) to the status log. */
    void logError(const QString& message);
    /** Restores window geometry, splitter sizes, column widths, and expanded folders from QSettings. */
    void loadSettings();
    /** Saves window geometry, splitter sizes, column widths, and expanded folders to QSettings. */
    void saveSettings();
    /** Expands folders under loadedPath that are pending restoration, letting the cascade
     *  continue into their children as the model finishes loading each level. */
    void expandPendingPaths(const QString& loadedPath);

    QSplitter* m_mainSplitter;
    QSplitter* m_contentSplitter;

    QLineEdit* m_pathEdit;
    QTreeView* m_fileBrowser;
    MsgFileModel* m_fileModel;
    QSet<QString> m_expandedPaths;
    QSet<QString> m_pendingExpandedPaths;

    QWidget* m_messagePanel;
    QLabel* m_subjectLabel;
    QLabel* m_fromLabel;
    QLabel* m_toLabel;
    QLabel* m_ccLabel;
    QLabel* m_dateLabel;
    QTabWidget* m_bodyTabs;
    QTextEdit* m_htmlBodyView;
    QTextEdit* m_plainBodyView;
    
    QTableView* m_attachmentView;
    AttachmentModel* m_attachmentModel;
    
    QTextEdit* m_statusLog;
    
    QString m_currentFile;
    EmailMessage m_currentMessage;
};

#endif
