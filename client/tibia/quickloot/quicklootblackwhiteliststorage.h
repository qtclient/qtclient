#pragma once

#include "config/icharacterconfiguration.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <set>

namespace tibia {
namespace quickloot {

/// The quick loot black- and whitelist of the character (configuration "lootBlackWhitelist"):
/// which of the two lists is active and the object types on each (at most 500).
class TQuickLootBlackWhitelistStorage final : public cip::tibia::quickloot::TQuickLootBlackWhitelistStorage,
                                              public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    enum EListType
    {
        Blacklist = 0,
        Whitelist = 1,
    };
    static constexpr std::size_t MaxListSize = 500;

    explicit TQuickLootBlackWhitelistStorage(QObject* parent = nullptr);

    QString configurationName() const override;
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;
    /// A new game session: blacklist active, both lists empty.
    void clear();

    int listType() const { return m_listType; }
    const std::set<uint16_t>& list(int listType) const;
    bool isListed(uint16_t objectType) const;
    bool isListFull() const;
    /// Adds to the active list unless it is full or holds the type already (the managed
    /// containers handler sends it on configurationChangedFromUserInput); true when added.
    bool addItem(uint16_t objectType);
    /// Removes from the active list.
    void removeItem(uint16_t objectType);
    /// Switches the active list.
    void setListType(int listType);
    /// Replaces both lists and the active type.
    void replaceLists(int listType, std::set<uint16_t> blacklist, std::set<uint16_t> whitelist);

private:
    std::set<uint16_t>& activeList();

    int m_listType = Blacklist;
    std::set<uint16_t> m_blacklist;
    std::set<uint16_t> m_whitelist;
};

}
}
