#include "npckeywordbuttonmodel.h"

namespace tibia::gamewindow {

namespace {

/// CIP DAT_1022b47a0: KEYWORDBUTTONICON_* -> strip cell (GENERALTRADE 0, POTIONTRADE 1,
/// EQUIPMENTTRADE 2, SAIL 3, DEPOSITALL 4, WITHDRAW 5, BALANCE 6, YES 7, NO 8, BYE 9).
constexpr int IconCells[] = { 1, 2, 3, 0, 5, 6, 4, 7, 8, 9 };

}

void TNpcKeywordButtonModel::setButtons(std::vector<Button> buttons)
{
    const int oldCount = int(m_buttons.size());
    const int newCount = int(buttons.size());
    if (newCount < oldCount) {
        beginRemoveRows({}, newCount, oldCount - 1);
        m_buttons = std::move(buttons);
        endRemoveRows();
    } else if (newCount > oldCount) {
        beginInsertRows({}, oldCount, newCount - 1);
        m_buttons = std::move(buttons);
        endInsertRows();
    } else {
        m_buttons = std::move(buttons);
    }
    if (newCount > 0)
        Q_EMIT dataChanged(index(0), index(newCount - 1));
}

int TNpcKeywordButtonModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_buttons.size());
}

QVariant TNpcKeywordButtonModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_buttons.size()))
        return {};
    const Button& button = m_buttons[size_t(index.row())];
    switch (role) {
    case ButtonImageRole:
        return imageOf(button.icon);
    case KeywordRole:
        return button.keyword;
    default:
        return {};
    }
}

QHash<int, QByteArray> TNpcKeywordButtonModel::roleNames() const
{
    return { { ButtonImageRole, "buttonImage" }, { KeywordRole, "keyword" } };
}

QString TNpcKeywordButtonModel::imageOf(const int icon)
{
    if (icon < 0 || icon >= int(std::size(IconCells)))
        return {};
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("icons-npcdialog")).arg(IconCells[icon]);
}

}
