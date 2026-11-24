#pragma once

#include <QObject>
#include <QString>

#include <set>

namespace tibia {
namespace exivaoptions {

/// The exiva restrictions (flags and character / guild whitelists) the
/// server reports. TExivaOptionsGameActionHandler reads them to build the
/// options it sends.
class TExivaOptionsStorage final : public QObject
{
    Q_OBJECT
public:
    /// The six flags of the protocol's exiva options, in wire order.
    struct Options
    {
        bool allowAll = false;
        bool allowOwnGuild = false;
        bool allowOwnParty = false;
        bool allowVipList = false;
        bool allowPlayerWhitelist = false;
        bool allowGuildWhitelist = false;
    };
    using Names = std::set<QString>;
    /// The character and guild whitelists of an options update.
    struct Whitelists
    {
        Names characters;
        Names guilds;
    };

    explicit TExivaOptionsStorage(QObject* parent = nullptr);
    ~TExivaOptionsStorage() override;

    bool allowEveryone() const { return m_options.allowAll; }
    bool allowOwnGuild() const { return m_options.allowOwnGuild; }
    bool allowOwnParty() const { return m_options.allowOwnParty; }
    bool allowVipList() const { return m_options.allowVipList; }
    bool allowCharacterWhitelist() const { return m_options.allowPlayerWhitelist; }
    bool allowGuildWhitelist() const { return m_options.allowGuildWhitelist; }
    /// Notifies only when the flag changes.
    void setAllowEveryone(bool allow);
    /// The five group flags at once; notifies when any of them changes.
    void setAllowedGroups(bool ownGuild, bool ownParty, bool vipList, bool characterWhitelist, bool guildWhitelist);
    /// Inserts / erases `names`; notifies when the whitelist's size changes.
    void addToCharacterWhitelist(const Names& names);
    void removeFromCharacterWhitelist(const Names& names);
    void addToGuildWhitelist(const Names& names);
    void removeFromGuildWhitelist(const Names& names);

    const Options& options() const { return m_options; }
    const Names& characterWhitelist() const { return m_characterWhitelist; }
    const Names& guildWhitelist() const { return m_guildWhitelist; }
    /// Case-sensitive, like the whitelist set.
    bool isCharacterWhitelisted(const QString& name) const { return m_characterWhitelist.contains(name); }

Q_SIGNALS:
    /// The allow-all flag only.
    void allowEveryoneChanged();
    /// The guild / party / VIP / whitelist flags.
    void allowedGroupsChanged();
    void characterWhitelistChanged();
    void guildWhitelistChanged();

private:
    Options m_options;
    Names m_characterWhitelist;
    Names m_guildWhitelist;
};

}
}
