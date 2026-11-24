#include "gamewindow/passiveabilitylistdatamodel.h"

#include "game/passiveabilityinfostorage.h"

namespace tibia::gamewindow {

TPassiveAbilityListDataModel::TPassiveAbilityListDataModel(const game::TPassiveAbilityInfoStorage* PassiveAbilityInfoStorage)
    : cip::tibia::gamewindow::TPassiveAbilityListDataModel(nullptr)
    , m_passiveAbilityInfoStorage(PassiveAbilityInfoStorage)
{
}

int TPassiveAbilityListDataModel::rowCount(const QModelIndex& /*parent*/) const
{
    // Host: the game client's storage can be missing (no client yet).
    if (!m_passiveAbilityInfoStorage)
        return 0;
    return static_cast<int>(m_passiveAbilityInfoStorage->count());
}

QVariant TPassiveAbilityListDataModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    game::TPassiveAbilityInfoStorage::TIterator it = m_passiveAbilityInfoStorage->begin();
    for (int row = 0; row < index.row(); ++row)
        it++;
    const game::TPassiveAbilityInfo info = *it;
    switch (role) {
    case EnumIdRole:
        return uint(info.id);
    case NameRole:
        return info.name;
    case IconIndexRole:
        return uint(info.iconId);
    case IconUrlRole:
        return QStringLiteral("image://passiveability-icons-32x32/%1").arg(info.iconId);
    default:
        return {};
    }
}

QHash<int, QByteArray> TPassiveAbilityListDataModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles[EnumIdRole] = "enumId";
    roles[NameRole] = "name";
    roles[IconIndexRole] = "iconIndex";
    roles[IconUrlRole] = "iconUrl";
    roles[UsableByCurrentVocationRole] = "usableByCurrentVocation";
    return roles;
}

}
