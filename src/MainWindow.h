#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QTextEdit>
#include <QLabel>
#include <QTableView>
#include <QSplitter>
#include "MsgParser.h"
#include "MsgFileModel.h"
#include "AttachmentModel.h"

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
    
private slots:
    /** Opens file dialog to select an MSG file. */
    void onOpenFile();
    /** Saves the currently selected attachment. */
    void onSaveAttachment();
    /** Handles double-click on a file in the browser. */
    void onFileDoubleClicked(const QModelIndex& index);
    /** Handles double-click on an attachment to save it. */
    void onAttachmentDoubleClicked(const QModelIndex& index);
    
private:
    /** Sets up the UI layout and widgets. */
    void setupUi();
    /** Creates the menu bar with File and Help menus. */
    void setupMenus();
    /** Updates the message view with parsed email data. */
    void updateMessageView(const EmailMessage& msg);
    /** Logs a message to the status log with timestamp. */
    void log(const QString& message);
    /** Logs a warning message (orange) to the status log. */
    void logWarning(const QString& message);
    /** Logs an error message (red) to the status log. */
    void logError(const QString& message);
    
    QSplitter* m_mainSplitter;
    QSplitter* m_contentSplitter;
    
    QTreeView* m_fileBrowser;
    MsgFileModel* m_fileModel;
    
    QWidget* m_messagePanel;
    QLabel* m_subjectLabel;
    QLabel* m_fromLabel;
    QLabel* m_toLabel;
    QLabel* m_ccLabel;
    QLabel* m_dateLabel;
    QTextEdit* m_bodyView;
    
    QTableView* m_attachmentView;
    AttachmentModel* m_attachmentModel;
    
    QTextEdit* m_statusLog;
    
    QString m_currentFile;
    EmailMessage m_currentMessage;
};

#endif
