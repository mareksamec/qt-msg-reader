#ifndef ATTACHMENTMODEL_H
#define ATTACHMENTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "EmailTypes.h"

/**
 * Qt table model for displaying email attachments.
 * Provides filename and size columns for the attachments table view.
 */
class AttachmentModel : public QAbstractTableModel {
    Q_OBJECT
    
public:
    enum Column {
        ColumnFilename = 0,
        ColumnSize,
        ColumnCount
    };
    
    explicit AttachmentModel(QObject* parent = nullptr);
    
    /** Replaces the current attachments with a new list. */
    void setAttachments(const QList<EmailAttachment>& attachments);
    /** Returns the attachment at the given row. */
    const EmailAttachment& attachment(int row) const;
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
private:
    QList<EmailAttachment> m_attachments;
};

#endif
