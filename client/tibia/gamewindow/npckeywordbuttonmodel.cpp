#include "npckeywordbuttonmodel.h"

namespace tibia::gamewindow {

namespace {

/// CIP DAT_1022b47a0: KEYWORDBUTTONICON_* -> strip cell (GENERALTRADE 0, POTIONTRADE 1,
/// EQUIPMENTTRADE 2, SAIL 3, DEPOSITALL 4, WITHDRAW 5, BALANCE 6, YES 7, NO 8, BYE 9).
constexpr int IconCells[] = { 1, 2, 3, 0, 5, 6, 4, 7, 8, 9 };

}

QVariant TNpcKeywordButtonModel::itemData(const TKeywordButton& button, const int role) const
{
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
