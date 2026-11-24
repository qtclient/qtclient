#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace tibia {
namespace creatures {

/// The VIP list and its groups (CIP: creatures::TVipStorage). Filled by
/// TVipProtocolMessageHandler and edited by TVipGameActionHandler; each game session owns one.
class TVipStorage final : public cip::tibia::creatures::TVipStorage
{
    Q_OBJECT
public:
    /// CIP EVipState: 0 offline, 1 online, 2 pending, 3 exercise weapon
    /// training, 4 online (logged in), 5 offline (logged out).
    enum EVipState
    {
        Offline = 0,
        Online = 1,
        Pending = 2,
        Training = 3,
        LoggedIn = 4,
        LoggedOut = 5,
    };
    /// CIP TVipData. Icons are 0..10.
    struct Vip
    {
        uint32_t id = 0;
        QString name;
        int state = 0;
        QString description;
        int iconId = 0;
        bool notifyLogin = false;
        std::vector<uint8_t> groups;
    };
    /// CIP TVipGroupData.
    struct Group
    {
        uint8_t id = 0;
        QString name;
        bool editable = false;
    };

    explicit TVipStorage(QObject* parent = nullptr);

    /// How many VIPs the list holds.
    size_t vipCount() const;
    /// The VIP with `id`; throws std::out_of_range for an unknown id.
    Vip vipAt(uint32_t id) const { return m_vips.at(id); }
    /// The VIP with `id`, or nothing for an unknown id (host: callers that skip unknown VIPs).
    std::optional<Vip> vipById(uint32_t id) const;
    /// The first VIP whose name equals `name` exactly.
    std::optional<Vip> vipByName(const QString& name) const;
    std::vector<Vip> vips() const;
    /// Stores `vip` under its id and reports it.
    void addOrUpdateVip(const Vip& vip);
    /// addOrUpdateVip from the protocol fields; icons above 10 become 0.
    void addVip(uint32_t id, const QString& name, const QString& description, uint8_t iconId, bool notifyLogin, int state,
                const std::vector<uint8_t>& groups);
    /// A known VIP's new state: logging in or out is reported, and a VIP with
    /// login notification coming online is announced in the status line and
    /// the console.
    void updateVipState(uint32_t id, int state);
    void removeVip(uint32_t id);
    /// Replaces the groups and the number of groups the account may still create.
    void setGroups(const std::vector<Group>& groups, int groupsLeft);
    std::vector<Group> groups() const;
    /// The groups by id.
    std::map<uint8_t, Group> groupMap() const;
    /// The group named exactly `name`.
    std::optional<Group> groupByName(const QString& name) const;
    int groupsLeft() const;

Q_SIGNALS:
    void vipDataChangedOrAdded(const tibia::creatures::TVipStorage::Vip& vip);
    void vipStateChanged(uint32_t id, int state);
    void vipDataRemoved(uint32_t id);
    void publishGameAction(const tibia::input::IGameActionPtr& action);
    void loginOrLogoutStateUpdate(int state);

private:
    std::map<uint32_t, Vip> m_vips;
    std::map<uint8_t, Group> m_groups;
    int m_groupsLeft = 0;
};

}
}
