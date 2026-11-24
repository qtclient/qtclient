#include "game/gameeventprotocolmessagehandler.h"

#include "chat/chatchannel.h"
#include "input/gameaction.h"

#include <protocol.pb.h>

#include <QString>
#include <QTimer>

#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace tibia::game {

using ::tibia::protobuf::protocol::GameserverMessageGameEvent;

namespace {

/// The automatic screenshot types (TAKE_AUTOMATIC_SCREENSHOT parameter) the
/// game events take; 0 takes none.
constexpr int NoScreenshot = 0;
constexpr int AchievementScreenshot = 1;
constexpr int BestiaryEntryCompletedScreenshot = 2;
constexpr int BestiaryEntryUnlockedScreenshot = 3;
constexpr int LevelUpScreenshot = 7;
constexpr int SkillUpScreenshot = 12;

/// The bestiary progress steps that take a screenshot.
constexpr int BestiaryProgressUnlocked = 1;
constexpr int BestiaryProgressCompleted = 4;

/// The level whose level up also shows the promotion hint.
constexpr int PromotionHintLevel = 20;

/// The chat message mode of the info texts.
constexpr int InfoTextMessageMode = 30;

/// The screenshot is taken this long after the event.
constexpr std::chrono::milliseconds AutomaticScreenshotDelay(100);

/// The simple events (protocol value) and the screenshot type each takes.
const std::map<int, int>& simpleEventScreenshotTypes()
{
    static const std::map<int, int> types = {
        { 1, 4 }, // boss defeated
        { 2, 5 }, // death (PvE)
        { 3, 6 }, // death (PvP)
        { 4, 8 }, // player kill assist
        { 5, 9 }, // player kill
        { 6, 10 }, // player attacking
        { 7, 11 }, // treasure found
        { 8, 13 }, // gift of life
        { 9, NoScreenshot },
        { 10, NoScreenshot },
        { 11, NoScreenshot },
        { 12, NoScreenshot },
        { 13, NoScreenshot },
        { 14, NoScreenshot },
        { 15, NoScreenshot },
        { 16, NoScreenshot },
    };
    return types;
}

/// The info banner of a simple event that shows one (9..16), or null.
const char* simpleEventInfoBanner(const int Event)
{
    switch (Event) {
    case 9:
        return "auto_attack_spamming";
    case 10:
        return "capacity_warning";
    case 11:
        return "ammunition_depleted";
    case 12:
        return "adjacent_target";
    case 13:
        return "soulpoints_depleted";
    case 14:
        return "leave_newhaven";
    case 15:
        return "weekly_task_any_creature_finished";
    case 16:
        return "promotion_unlocked";
    default:
        return nullptr;
    }
}

/// The skill of a skill up (protocol skill 1..13); 0 for any other.
constexpr std::array<int, 13> SkillUpSkills = { 1, 0, 0, 0, 0, 7, 6, 2, 3, 4, 5, 0, 8 };

QString translated(const QString& Id)
{
    return qtTrId(Id.toStdString().c_str());
}

/// "<caption>. <text>"
QString captionAndText(const QString& Caption, const QString& Text)
{
    return QStringLiteral("%1. %2").arg(Caption).arg(Text);
}

/// The caption and text of an info banner ("info_banner_<key>_caption" / "_text").
QString infoBannerText(const QString& Key)
{
    const QString caption = translated(QStringLiteral("info_banner_%1_caption").arg(Key));
    const QString text = translated(QStringLiteral("info_banner_%1_text").arg(Key));
    return captionAndText(caption, text);
}

}

void TGameEventProtocolMessageHandler::handleGameEventMessage(const GameserverMessageGameEvent& GameEventMessage)
{
    using E = input::gameactions::TGameActionGameEvent;
    const auto publishGameEvent = [this](E::TEvent Event) {
        Q_EMIT publishGameAction(std::make_shared<E>(std::move(Event)));
    };

    QString text;
    int screenshotType = NoScreenshot;
    switch (GameEventMessage.event_case()) {
    case GameserverMessageGameEvent::kSimpleEvent: {
        const int event = GameEventMessage.simple_event();
        const auto& types = simpleEventScreenshotTypes();
        const auto type = types.find(event);
        if (type == types.end())
            throw std::runtime_error("unhandled enum value");
        screenshotType = type->second;
        if (const char* banner = simpleEventInfoBanner(event)) {
            publishGameEvent(E::TSimple{ event });
            text = infoBannerText(QString::fromUtf8(banner));
        }
        break;
    }
    case GameserverMessageGameEvent::kAchievement:
        publishGameEvent(E::TAchievement{ GameEventMessage.achievement().name() });
        screenshotType = AchievementScreenshot;
        break;
    case GameserverMessageGameEvent::kTitle:
        publishGameEvent(E::TTitle{ GameEventMessage.title().name() });
        break;
    case GameserverMessageGameEvent::kLevelUp: {
        const auto level = static_cast<uint16_t>(GameEventMessage.level_up().level());
        publishGameEvent(E::TLevelUp{ level });
        screenshotType = LevelUpScreenshot;
        if (level == PromotionHintLevel) {
            text = captionAndText(qtTrId("info_banner_level_up_caption").arg(level),
                                  qtTrId("info_banner_promotion_text"));
        }
        break;
    }
    case GameserverMessageGameEvent::kSkillUp: {
        const auto& skillUp = GameEventMessage.skill_up();
        const uint32_t index = skillUp.skill() - 1;
        const int skill = index < SkillUpSkills.size() ? SkillUpSkills[index] : 0;
        publishGameEvent(E::TSkillUp{ skill, static_cast<uint16_t>(skillUp.level()) });
        screenshotType = SkillUpScreenshot;
        break;
    }
    case GameserverMessageGameEvent::kBestiaryProgress: {
        const auto& progress = GameEventMessage.bestiary_progress();
        const auto step = static_cast<uint8_t>(progress.progress());
        if (step == BestiaryProgressUnlocked)
            screenshotType = BestiaryEntryUnlockedScreenshot;
        else if (step == BestiaryProgressCompleted)
            screenshotType = BestiaryEntryCompletedScreenshot;
        publishGameEvent(E::TBestiaryProgress{ static_cast<uint16_t>(progress.race_id()), step });
        break;
    }
    case GameserverMessageGameEvent::kBosstiaryProgress: {
        const auto& progress = GameEventMessage.bosstiary_progress();
        publishGameEvent(E::TBosstiaryProgress{ static_cast<uint16_t>(progress.race_id()),
                                                static_cast<uint8_t>(progress.progress()) });
        break;
    }
    case GameserverMessageGameEvent::kQuestProgress: {
        const auto& quest = GameEventMessage.quest_progress();
        // Protocol state 1 is 0 (started), 2 is 1 (completed).
        const int state = quest.state() == 2 ? 1 : 0;
        publishGameEvent(E::TQuestProgress{ quest.quest_name(), state });
        break;
    }
    case GameserverMessageGameEvent::kOutfitPartUnlocked: {
        const auto& outfit = GameEventMessage.outfit_part_unlocked();
        const uint32_t part = outfit.part() - 1;
        publishGameEvent(E::TOutfitPartUnlocked{ static_cast<uint16_t>(outfit.outfit_id()), outfit.name(),
                                                 part < 4 ? static_cast<int>(part) : 0 });
        break;
    }
    case GameserverMessageGameEvent::kWeaponProficiency: {
        const auto& proficiency = GameEventMessage.weapon_proficiency();
        publishGameEvent(E::TWeaponProficiency{ static_cast<uint16_t>(proficiency.object_id()), proficiency.name() });
        break;
    }
    case GameserverMessageGameEvent::kBountyTaskFinished:
        publishGameEvent(
            E::TBountyTaskFinished{ static_cast<uint16_t>(GameEventMessage.bounty_task_finished().race_id()) });
        break;
    case GameserverMessageGameEvent::kWeeklyTaskSpecificCreatureFinished:
        publishGameEvent(E::TWeeklyTaskSpecificCreatureFinished{
            static_cast<uint16_t>(GameEventMessage.weekly_task_specific_creature_finished().race_id()) });
        break;
    case GameserverMessageGameEvent::kSpellUnlocked:
        publishGameEvent(E::TSpellUnlocked{ GameEventMessage.spell_unlocked().spell_id() });
        break;
    case GameserverMessageGameEvent::kLeaderMonsterKilled: {
        const auto& leader = GameEventMessage.leader_monster_killed();
        publishGameEvent(E::TLeaderMonsterKilled{ static_cast<uint16_t>(leader.race_id()), leader.charm_points() });
        break;
    }
    case GameserverMessageGameEvent::kSubAreaUnlocked:
        publishGameEvent(E::TSubAreaUnlocked{ GameEventMessage.sub_area_unlocked().sub_area_id() });
        break;
    case GameserverMessageGameEvent::kAreaUnlocked:
        publishGameEvent(E::TAreaUnlocked{ GameEventMessage.area_unlocked().area_id() });
        break;
    default:
        break;
    }

    if (screenshotType != NoScreenshot) {
        QTimer::singleShot(AutomaticScreenshotDelay, this, [this, screenshotType] {
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionTakeAutomaticScreenshot>(screenshotType));
        });
    }
    if (!text.isEmpty()) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionAddTextMessage>(
            input::EGameAction::ADD_SYSTEM_CHAT_MESSAGE, text, chat::consoleColor(InfoTextMessageMode)));
    }
}

}
