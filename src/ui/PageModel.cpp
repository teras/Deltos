#include "PageModel.h"
#include <QFileInfo>

namespace deltos {

QVariant PageModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= rowCount()) return {};
    const Page& p = pages_[size_t(idx.row())];
    switch (role) {
    case Qt::DisplayRole: {
        QString label = QStringLiteral("%1. %2").arg(idx.row() + 1).arg(QFileInfo(p.sourcePath).fileName());
        if (!p.manualName.isEmpty()) label += QStringLiteral("\n%1").arg(p.manualName);
        else if (!p.standard.isEmpty()) label += QStringLiteral("\n%1").arg(p.standard);
        return label;
    }
    case Qt::DecorationRole: return p.thumbnail;
    default: return {};
    }
}

Qt::ItemFlags PageModel::flags(const QModelIndex& idx) const {
    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
    if (idx.isValid()) f |= Qt::ItemIsDragEnabled;
    return f;
}

bool PageModel::moveRows(const QModelIndex&, int src, int count, const QModelIndex&, int dst) {
    if (count != 1 || src < 0 || src >= rowCount() || dst < 0 || dst > rowCount() || dst == src || dst == src + 1)
        return false;
    beginMoveRows({}, src, src, {}, dst);
    Page p = std::move(pages_[size_t(src)]);
    pages_.erase(pages_.begin() + src);
    pages_.insert(pages_.begin() + (dst > src ? dst - 1 : dst), std::move(p));
    endMoveRows();
    return true;
}

bool PageModel::removeRows(int row, int count, const QModelIndex&) {
    if (row < 0 || count <= 0 || row + count > rowCount()) return false;
    beginRemoveRows({}, row, row + count - 1);
    pages_.erase(pages_.begin() + row, pages_.begin() + row + count);
    endRemoveRows();
    return true;
}

int PageModel::addPage(Page page) {
    const int row = rowCount();
    beginInsertRows({}, row, row);
    pages_.push_back(std::move(page));
    endInsertRows();
    return row;
}

void PageModel::pageChanged(int row) {
    const QModelIndex i = index(row);
    emit dataChanged(i, i);
}

} // namespace deltos
