#include "gamewindow/passiveabilitylistdatamodelsortfilterproxy.h"

#include "game/gamesessionbase.h"
#include "game/passiveabilityinfostorage.h"
#include "game/playerdata.h"

namespace tibia::gamewindow {

TPassiveAbilityListDataModelSortFilterProxy::TPassiveAbilityListDataModelSortFilterProxy(
    const game::TPassiveAbilityInfoStorage* PassiveAbilityInfoStorage)
    : cip::tibia::gamewindow::TPassiveAbilityListDataModelSortFilterProxy(nullptr)
    , m_model(std::make_shared<TPassiveAbilityListDataModel>(PassiveAbilityInfoStorage))
{
    setSourceModel(m_model.get());
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
}

TPassiveAbilityListDataModelSortFilterProxy::~TPassiveAbilityListDataModelSortFilterProxy()
{
    m_gameSessionConnections.disconnectAll();
}

void TPassiveAbilityListDataModelSortFilterProxy::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_playerData.reset();
    m_gameSession = GameSession;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        m_playerData = session->playerData();
        if (m_playerData)
            m_gameSessionConnections.add(connect(m_playerData.get(), &game::TPlayerData::playerDataChanged, this,
                                                 &TPassiveAbilityListDataModelSortFilterProxy::onPlayerDataChanged));
    }
    onPlayerDataChanged();
}

uint8_t TPassiveAbilityListDataModelSortFilterProxy::passiveAbilityAt(const int RowIndex) const
{
    return static_cast<uint8_t>(m_model->data(mapToSource(index(RowIndex, 0)), TPassiveAbilityListDataModel::EnumIdRole).toUInt());
}

QVariant TPassiveAbilityListDataModelSortFilterProxy::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const QModelIndex source = mapToSource(index);
    if (role != TPassiveAbilityListDataModel::UsableByCurrentVocationRole)
        return source.model() ? source.model()->data(source, role) : QVariant();
    const QVariant id = source.model() ? source.model()->data(source, TPassiveAbilityListDataModel::EnumIdRole) : QVariant();
    const game::TPassiveAbilityInfo info = m_model->passiveAbilityInfoStorage()->info(static_cast<uint8_t>(id.toUInt()));
    return info.vocations.count(m_playerVocation) != 0;
}

QVariant TPassiveAbilityListDataModelSortFilterProxy::getDataWithRoleName(const int RowIndex, const QByteArray& RoleName)
{
    const QModelIndex source = mapToSource(index(RowIndex, 0));
    const int role = roleNames().key(RoleName, 0);
    return m_model->data(source, role).toString();
}

void TPassiveAbilityListDataModelSortFilterProxy::onPlayerDataChanged()
{
    if (!m_playerData) {
        m_playerVocation = 0;
        m_playerLevel = 0;
    } else {
        const bool vocationChanged = m_playerVocation != m_playerData->vocation();
        if (vocationChanged)
            m_playerVocation = m_playerData->vocation();
        if (m_playerLevel == m_playerData->level()) {
            if (!vocationChanged)
                return;
        } else {
            m_playerLevel = m_playerData->level();
        }
    }
    invalidate();
}

bool TPassiveAbilityListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex source = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!source.isValid())
        return false;
    const uint id = source.model()->data(source, TPassiveAbilityListDataModel::EnumIdRole).toUInt();
    const game::TPassiveAbilityInfo info = m_model->passiveAbilityInfoStorage()->info(static_cast<uint8_t>(id));
    if (!m_showOnlyCurrentVocation)
        return true;
    return info.vocations.count(m_playerVocation) != 0;
}

bool TPassiveAbilityListDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftName = (left.model() ? left.model()->data(left, TPassiveAbilityListDataModel::NameRole) : QVariant()).toString().toLower();
    const QString rightName = (right.model() ? right.model()->data(right, TPassiveAbilityListDataModel::NameRole) : QVariant()).toString().toLower();
    return QString::compare(leftName, rightName, Qt::CaseSensitive) < 0;
}

}
