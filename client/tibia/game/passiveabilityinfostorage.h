#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QString>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace tibia {
namespace gamewindow {
class TPassiveAbilityListDataModel;
}
}

namespace tibia {
namespace game {

/// What the client knows about a passive ability: its id, translated name,
/// the index of its icon in the passive ability icon strip and the vocations
/// that can use it. `flag` is the record's last field; the passive ability
/// list does not read it.
struct TPassiveAbilityInfo
{
    uint8_t id = 0;
    QString name;
    uint16_t iconId = 0;
    std::set<uint8_t> vocations;
    bool flag = false;
};

/// The client's fixed table of passive abilities. It holds one ability, the
/// Gift of Life (id 1, icon 0, vocations 1 to 5).
class TPassiveAbilityInfoStorage final : public cip::tibia::game::TPassiveAbilityInfoStorage
{
    Q_OBJECT
public:
    /// Walks the table handing out copies of its records; the copy is made on
    /// the first dereference.
    class TIterator
    {
    public:
        explicit TIterator(std::map<uint8_t, TPassiveAbilityInfo>::const_iterator node);

        const TPassiveAbilityInfo& operator*();
        const TPassiveAbilityInfo* operator->() { return &**this; }
        bool operator==(const TIterator& other) const;
        bool operator!=(const TIterator& other) const { return !(*this == other); }
        /// Moves to the next record and returns the iterator as it was.
        TIterator operator++(int);

    private:
        std::map<uint8_t, TPassiveAbilityInfo>::const_iterator m_node;
        std::optional<TPassiveAbilityInfo> m_value;
    };

    explicit TPassiveAbilityInfoStorage(QObject* parent = nullptr);
    /// The game client's table; null without a client.
    static TPassiveAbilityInfoStorage* instance();
    /// Installs the game client's table (null: none).
    static void setInstance(TPassiveAbilityInfoStorage* storage);

    /// The ability's record; throws std::out_of_range for an id the table
    /// does not have.
    TPassiveAbilityInfo info(uint8_t id) const;
    /// The ability's record, none for an unknown id.
    std::optional<TPassiveAbilityInfo> findInfo(uint8_t id) const;
    /// How many abilities the table has.
    std::size_t count() const;
    TIterator begin() const;
    TIterator end() const;
    /// The ability's name, empty for an unknown id.
    QString name(uint8_t id) const;

    /// Whether the table has the ability.
    static bool isKnownPassiveAbility(uint8_t id);

    gamewindow::TPassiveAbilityListDataModel* listModel() const { return m_model; }
    uint32_t currentCooldown() const { return m_currentCooldown; }
    uint32_t maxCooldown() const { return m_maxCooldown; }
    bool canDecay() const { return m_canDecay; }

private:
    /// The table, built on first use.
    static const std::map<uint8_t, TPassiveAbilityInfo>& infos();

    static TPassiveAbilityInfoStorage* s_instance;
    gamewindow::TPassiveAbilityListDataModel* m_model = nullptr;
    uint32_t m_currentCooldown = 0;
    uint32_t m_maxCooldown = 0;
    bool m_canDecay = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
