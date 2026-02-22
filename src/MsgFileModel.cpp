#include "MsgFileModel.h"

MsgFileModel::MsgFileModel(QObject* parent)
    : QFileSystemModel(parent)
{
    setNameFilters(QStringList() << "*.msg" << "*.MSG");
    setNameFilterDisables(false);
}

QVariant MsgFileModel::data(const QModelIndex& index, int role) const {
    return QFileSystemModel::data(index, role);
}
