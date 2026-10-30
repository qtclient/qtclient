#pragma once

#include "client/gamecore.h"

#include <QObject>
#include <QString>

#include <set>
#include <vector>

namespace tibia::exivaoptions {

/// The exiva restrictions (CIP: TExivaOptionsStorage, getters 0x100ad7200..
/// 0x100ad7270, character whitelist 0x100ad72f0), fed by the server's 0xCA
/// message (TExivaOptionsProtocolMessageHandler: the added names join the
/// whitelists, the removed ones leave them, 0x100ad5c60), plus
/// TExivaOptionsGameActionHandler 0x100ad4c10: EXIVA_TOGGLE_ALLOW_EVERYONE,
/// EXIVA_ADD/REMOVE_CHARACTER_TO/FROM_WHITELIST and the dialog's
/// EXIVA_UPADATE_OPTIONS (update()).
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

    const Options& options() const { return m_options; }
    const Names& characterWhitelist() const { return m_characterWhitelist; }
    const Names& guildWhitelist() const { return m_guildWhitelist; }
    /// Case-sensitive, like CIP's whitelist set.
    bool isCharacterWhitelisted(const QString& name) const { return m_characterWhitelist.contains(name); }

    /// CIP 0x100ad4f50 (EXIVA_UPADATE_OPTIONS): `options` with the names that
    /// join or leave each whitelist compared to the stored ones.
    void update(const Options& options, const Names& characters, const Names& guilds) const;

Q_SIGNALS:
    void optionsChanged();
    void characterWhitelistChanged();
    void guildWhitelistChanged();

private:
    /// CIP 0x100ad4d40 (EXIVA_TOGGLE_ALLOW_EVERYONE): the current options with
    /// "allow everyone" flipped, and a status line about it.
    void toggleAllowEveryone() const;
    /// CIP 0x100ad5750 (EXIVA_ADD_CHARACTER_TO_WHITELIST) / 0x100ad5940
    /// (EXIVA_REMOVE_CHARACTER_FROM_WHITELIST): the current options with `name`
    /// as the only added / removed character.
    void requestWhitelistChange(const QString& name, bool add) const;
    void send(const Options& options, const Names& addCharacters, const Names& removeCharacters, const Names& addGuilds,
              const Names& removeGuilds) const;

    std::vector<client::gamecore::Hook> m_hooks;
    Options m_options;
    Names m_characterWhitelist;
    Names m_guildWhitelist;
};

}
