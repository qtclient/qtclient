#include "sound/soundconfiguration.h"

#include "config/clientoptions.h"
#include "sound/soundengine.h"

namespace tibia::sound {

namespace {

using O = config::EClientOption;

constexpr int kMasterVolume = static_cast<int>(O::soundMasterVolume);

}

TSoundConfiguration::TSoundConfiguration(const TSoundSettings& settings, config::TClientOptions* options)
    : m_settings(settings)
    , m_options(options)
{
    if (m_options)
        connect(m_options, &config::TClientOptions::soundOptionsChanged, this, &TSoundConfiguration::onSoundOptionsChanged);
    Q_EMIT soundConfigurationChanged();
    if (m_settings.noSound)
        TSoundEngine::instance().setInitialized(false);
}

void TSoundConfiguration::onSoundOptionsChanged()
{
    Q_EMIT soundConfigurationChanged();
}

bool TSoundConfiguration::boolOption(const int id) const
{
    return m_options && m_options->boolOption(static_cast<O>(id));
}

int TSoundConfiguration::intOption(const int id) const
{
    return m_options ? m_options->intOption(static_cast<O>(id)) : 0;
}

float TSoundConfiguration::volume(const int id) const
{
    const float master = static_cast<float>(static_cast<uint8_t>(intOption(kMasterVolume))) / 100.0f;
    const float value = static_cast<float>(intOption(id)) / 100.0f;
    return value * master * 100.0f;
}

QString TSoundConfiguration::usedSoundDevice() const
{
    return m_options ? m_options->stringOption(O::usedSoundDevice) : QString();
}

bool TSoundConfiguration::isSoundEnabled() const
{
    return TSoundEngine::instance().isInitialized() && intOption(kMasterVolume) != 0;
}

float TSoundConfiguration::itemsVolume() const
{
    return volume(static_cast<int>(O::soundItemsVolume));
}

float TSoundConfiguration::eventsVolume() const
{
    return volume(static_cast<int>(O::soundEventsVolume));
}

float TSoundConfiguration::uiVolume() const
{
    return volume(static_cast<int>(O::soundUiVolume));
}

float TSoundConfiguration::ambienceVolume() const
{
    return volume(static_cast<int>(O::soundAmbienceVolume));
}

float TSoundConfiguration::musicVolume() const
{
    return volume(static_cast<int>(O::soundMusicVolume));
}

bool TSoundConfiguration::isAmbienceAudible() const
{
    if (!TSoundEngine::instance().isInitialized() || intOption(kMasterVolume) == 0)
        return false;
    return static_cast<int>(ambienceVolume()) != 0;
}

bool TSoundConfiguration::isAmbienceStreamEnabled() const
{
    return isAmbienceAudible() && !m_settings.ambienceStreamsDisabled;
}

bool TSoundConfiguration::isAmbienceEnabled() const
{
    return isAmbienceAudible() && !m_settings.ambienceDisabled;
}

bool TSoundConfiguration::isMusicEnabled() const
{
    if (!TSoundEngine::instance().isInitialized() || intOption(kMasterVolume) == 0)
        return false;
    return static_cast<int>(musicVolume()) != 0;
}

bool TSoundConfiguration::isTitleMusicEnabled() const
{
    return isMusicEnabled() && boolOption(static_cast<int>(O::soundAnthemEnabled));
}

bool TSoundConfiguration::uiInteractionSoundsEnabled() const
{
    if (!TSoundEngine::instance().isInitialized() || intOption(kMasterVolume) == 0)
        return false;
    return boolOption(static_cast<int>(O::soundUiInteractionsEnabled));
}

bool TSoundConfiguration::privateMessagesWithoutTabSoundEnabled() const
{
    return boolOption(static_cast<int>(O::ChatEnabled)) && boolOption(static_cast<int>(O::PrivateMessagesWithoutTabEnabled));
}

bool TSoundConfiguration::creatureAttackSoundsEnabled() const { return boolOption(static_cast<int>(O::soundBattleCreaturesAttacksEnabled)); }
bool TSoundConfiguration::ownWeaponSoundsEnabled() const { return boolOption(static_cast<int>(O::soundBattleOwnWeaponsEnabled)); }
bool TSoundConfiguration::othersWeaponSoundsEnabled() const { return boolOption(static_cast<int>(O::soundBattleOthersWeaponsEnabled)); }
bool TSoundConfiguration::creatureNoiseSoundsEnabled() const { return boolOption(static_cast<int>(O::soundBattleCreaturesNoisesEnabled)); }
bool TSoundConfiguration::creatureDeathSoundsEnabled() const { return boolOption(static_cast<int>(O::soundBattleCreaturesDeathEnabled)); }
bool TSoundConfiguration::eatingSoundsEnabled() const { return boolOption(static_cast<int>(O::soundEatingEnabled)); }
bool TSoundConfiguration::moveItemSoundsEnabled() const { return boolOption(static_cast<int>(O::soundMoveItemEnabled)); }
bool TSoundConfiguration::partySoundsEnabled() const { return boolOption(static_cast<int>(O::soundPartyEnabled)); }
bool TSoundConfiguration::vipSoundsEnabled() const { return boolOption(static_cast<int>(O::soundVipEnabled)); }
bool TSoundConfiguration::raidMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::RaidMessagesEnabled)); }
bool TSoundConfiguration::systemMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::SystemMessagesEnabled)); }
bool TSoundConfiguration::chatSoundsEnabled() const { return boolOption(static_cast<int>(O::ChatEnabled)); }
bool TSoundConfiguration::globalMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::GlobalMessagesEnabled)); }
bool TSoundConfiguration::guildMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::GuildMessagesEnabled)); }
bool TSoundConfiguration::npcMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::NpcMessageseEnabled)); }
bool TSoundConfiguration::partyMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::PartyMessagesEnabled)); }
bool TSoundConfiguration::privateMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::PrivateMessagesEnabled)); }
bool TSoundConfiguration::teamFinderMessageSoundsEnabled() const { return boolOption(static_cast<int>(O::TeamFinderMessagesEnabled)); }
bool TSoundConfiguration::ownSpellSoundsEnabled() const { return boolOption(static_cast<int>(O::soundBattleOwnSpellsEnabled)); }
bool TSoundConfiguration::othersSpellSoundsEnabled() const { return boolOption(static_cast<int>(O::soundBattleOthersSpellsEnabled)); }

bool TSoundConfiguration::ownAttackSpellSoundsEnabled() const
{
    return ownSpellSoundsEnabled() && boolOption(static_cast<int>(O::soundBattleOwnAttackSpellsEnabled));
}

bool TSoundConfiguration::ownHealingSpellSoundsEnabled() const
{
    return ownSpellSoundsEnabled() && boolOption(static_cast<int>(O::soundBattleOwnHealingSpellsEnabled));
}

bool TSoundConfiguration::ownSupportSpellSoundsEnabled() const
{
    return ownSpellSoundsEnabled() && boolOption(static_cast<int>(O::soundBattleOwnSupportSpellsEnabled));
}

bool TSoundConfiguration::othersAttackSpellSoundsEnabled() const
{
    return othersSpellSoundsEnabled() && boolOption(static_cast<int>(O::soundBattleOthersAttackSpellsEnabled));
}

bool TSoundConfiguration::othersHealingSpellSoundsEnabled() const
{
    return othersSpellSoundsEnabled() && boolOption(static_cast<int>(O::soundBattleOthersHealingSpellsEnabled));
}

bool TSoundConfiguration::othersSupportSpellSoundsEnabled() const
{
    return othersSpellSoundsEnabled() && boolOption(static_cast<int>(O::soundBattleOthersSupportSpellsEnabled));
}

float TSoundConfiguration::ownBattleVolume() const
{
    return volume(static_cast<int>(O::soundBattleOwnVolume));
}

float TSoundConfiguration::othersBattleVolume() const
{
    return volume(static_cast<int>(O::soundBattleOthersVolume));
}

float TSoundConfiguration::creatureBattleVolume() const
{
    return volume(static_cast<int>(O::soundBattleCreaturesVolume));
}

bool TSoundConfiguration::chatMessageSoundEnabled(const std::optional<TChatSoundContext>& chat) const
{
    // CipSoft looks up the channel here (the active tab mutes it; the channel
    // kind picks the option). qtclient's chat storage does that before it
    // triggers the sound and marks the request as gated.
    if (!chatSoundsEnabled())
        return false;
    return chat && chat->gatedByChannel;
}

TSoundConfiguration::TEffectSettings TSoundConfiguration::effectSettings(const int type,
                                                                         const std::optional<TChatSoundContext>& chat,
                                                                         const int source) const
{
    const auto battle = [&](const bool enabled, const float vol) { return TEffectSettings{ enabled, vol }; };
    const auto bySource = [&](const bool own, const bool others) -> std::optional<TEffectSettings> {
        switch (source) {
        case 1:
            return battle(own, ownBattleVolume());
        case 2:
            return battle(others, othersBattleVolume());
        case 3:
            return battle(creatureAttackSoundsEnabled(), creatureBattleVolume());
        default:
            return std::nullopt;
        }
    };

    std::optional<TEffectSettings> settings;
    switch (type) {
    case 1:
        settings = bySource(ownAttackSpellSoundsEnabled(), othersAttackSpellSoundsEnabled());
        break;
    case 2:
        settings = bySource(ownHealingSpellSoundsEnabled(), othersHealingSpellSoundsEnabled());
        break;
    case 3:
        settings = bySource(ownSupportSpellSoundsEnabled(), othersSupportSpellSoundsEnabled());
        break;
    case 4:
        settings = bySource(ownWeaponSoundsEnabled(), othersWeaponSoundsEnabled());
        break;
    case 5:
        return { creatureNoiseSoundsEnabled(), creatureBattleVolume() };
    case 6:
        return { creatureDeathSoundsEnabled(), creatureBattleVolume() };
    case 7:
        return { creatureAttackSoundsEnabled(), creatureBattleVolume() };
    case 8:
        return { true, ambienceVolume() };
    case 9:
        return { eatingSoundsEnabled(), itemsVolume() };
    case 10:
        return { moveItemSoundsEnabled(), itemsVolume() };
    case 11:
        return { TSoundEngine::instance().isInitialized() && intOption(kMasterVolume) != 0, eventsVolume() };
    case 12:
        return { uiInteractionSoundsEnabled(), uiVolume() };
    case 13:
        return { privateMessagesWithoutTabSoundEnabled(), uiVolume() };
    case 14:
        return { chatMessageSoundEnabled(chat), uiVolume() };
    case 15:
        return { partySoundsEnabled(), uiVolume() };
    case 16:
        return { vipSoundsEnabled(), uiVolume() };
    case 17:
        return { raidMessageSoundsEnabled(), uiVolume() };
    case 18:
        return { systemMessageSoundsEnabled(), uiVolume() };
    case 19:
        settings = bySource(ownHealingSpellSoundsEnabled() || ownAttackSpellSoundsEnabled() || ownSupportSpellSoundsEnabled(),
                            othersHealingSpellSoundsEnabled() || othersAttackSpellSoundsEnabled()
                                || othersSupportSpellSoundsEnabled());
        break;
    default:
        break;
    }
    if (settings)
        return *settings;
    return { true, static_cast<float>(static_cast<uint8_t>(intOption(kMasterVolume))) };
}

}
