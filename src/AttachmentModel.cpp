#include "AttachmentModel.h"

AttachmentModel::AttachmentModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void AttachmentModel::setAttachments(const QList<EmailAttachment>& attachments) {
    beginResetModel();
    m_attachments = attachments;
    endResetModel();
}

const EmailAttachment& AttachmentModel::attachment(int row) const {
    return m_attachments.at(row);
}

int AttachmentModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_attachments.size();
}

int AttachmentModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant AttachmentModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_attachments.size())
        return QVariant();
    
    if (role == Qt::DisplayRole) {
        const EmailAttachment& att = m_attachments[index.row()];
        
        switch (index.column()) {
            case ColumnFilename:
                return att.filename;
            case ColumnSize: {
                // Format size in human-readable format
                if (att.size < 1024)
                    return QString("%1 B").arg(att.size);
                else if (att.size < 1024 * 1024)
                    return QString("%1 KB").arg(att.size / 1024);
                else
                    return QString("%1 MB").arg(att.size / (1024 * 1024));
            }
        }
    }
    
    return QVariant();
}

QVariant AttachmentModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case ColumnFilename: return tr("Filename");
            case ColumnSize: return tr("Size");
        }
    }
    return QVariant();
}
