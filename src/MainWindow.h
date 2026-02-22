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

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void loadFile(const QString& filePath);
    
private slots:
    void onOpenFile();
    void onSaveAttachment();
    void onFileDoubleClicked(const QModelIndex& index);
    void onAttachmentDoubleClicked(const QModelIndex& index);
    
private:
    void setupUi();
    void setupMenus();
    void updateMessageView(const EmailMessage& msg);
    void log(const QString& message);
    void logWarning(const QString& message);
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
