#ifndef MSGFILEMODEL_H
#define MSGFILEMODEL_H

#include <QFileSystemModel>

class MsgFileModel : public QFileSystemModel {
    Q_OBJECT
    
public:
    explicit MsgFileModel(QObject* parent = nullptr);
    
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
};

#endif
