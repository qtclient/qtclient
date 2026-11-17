#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "input/gameaction.h"

#include <QObject>
#include <QString>

#include <set>
#include <vector>

namespace tibia {
namespace exivaoptions {

/// The exiva restrictions (flags and character / guild whitelists) the
/// server reports, plus the exiva game actions (toggle allow-everyone, add /
/// remove a whitelisted character, update from the dialog).
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

    explicit TExivaOptionsStorage(QObject* parent = nullptr);
    ~TExivaOptionsStorage() override;

    /// The storage the server's exiva messages update (the VIP widget owns it).
    static TExivaOptionsStorage* instance();

    bool allowEveryone() const { return m_options.allowAll; }
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

    /// (EXIVA_UPADATE_OPTIONS): `options` with the names that
    /// join or leave each whitelist compared to the stored ones.
    void update(const Options& options, const Names& characters, const Names& guilds) const;

Q_SIGNALS:
    /// The allow-all flag only.
    void allowEveryoneChanged();
    /// The guild / party / VIP / whitelist flags.
    void allowedGroupsChanged();
    void characterWhitelistChanged();
    void guildWhitelistChanged();
    /// The status line its toggle shows (CIP: TExivaOptionsGameActionHandler's publishGameAction).
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    /// The exiva actions: toggle allow-everyone, add / remove a whitelisted
    /// character, update the options.
    bool handleGameAction(const input::IGameAction& action);
    /// (EXIVA_TOGGLE_ALLOW_EVERYONE): the current options with
    /// "allow everyone" flipped, and a status line about it.
    void toggleAllowEveryone();
    /// (EXIVA_ADD_CHARACTER_TO_WHITELIST) / 
    /// (EXIVA_REMOVE_CHARACTER_FROM_WHITELIST): the current options with `name`
    /// as the only added / removed character.
    void requestWhitelistChange(const QString& name, bool add) const;
    void send(const Options& options, const Names& addCharacters, const Names& removeCharacters, const Names& addGuilds,
              const Names& removeGuilds) const;

    std::vector<client::gamecore::Hook> m_hooks;
    Options m_options;
    Names m_characterWhitelist;
    Names m_guildWhitelist;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
