// Host API v9: the cooldown and game data tables (abi/gamedata_api.h) over the session's
// CipSoft TCooldownStorage, the process clock (TClockService), CipSoft's spell table
// (TSpellInfoStorage, with the server's modifications), the Cyclopedia charm table, the
// outfit palette and the 8-bit colours.
#include "mods/abi_internal.h"

#include "client/eightbitcolor.h"
#include "cyclopedia/bestiarystorage.h"
#include "game/cooldownstorage.h"
#include "game/currentgamesession.h"
#include "qmlcomponents/outfitpalette.h"
#include "spells/spellinfostorage.h"

#include "clockservice.h"

#include <QColor>
#include <QMetaObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <map>
#include <optional>

namespace tibia::mods {
namespace {

// --- cooldowns ---

int64_t cdNow()
{
    return static_cast<int64_t>(shared::TClockService::process()->nowMillis());
}

void fill(const game::TCooldown& c, QtClientCooldown* out)
{
    out->remaining_ms = c.remaining;
    out->duration_ms = c.duration;
    out->end_ms = c.end;
}

int cdSpell(uint16_t spell_id, int64_t now_ms, QtClientCooldown* out)
{
    const game::TCooldownStorage* storage = game::currentCooldownStorage();
    if (!storage || !out)
        return 0;
    const game::TCooldown c = storage->spellOwnCooldown(spell_id, now_ms);
    fill(c, out);
    return c.end != 0 || c.duration != 0 ? 1 : 0;
}

int cdGroup(uint8_t group_id, int64_t now_ms, QtClientCooldown* out)
{
    const game::TCooldownStorage* storage = game::currentCooldownStorage();
    if (!storage || !out)
        return 0;
    const game::TCooldown c = storage->spellGroupCooldown(group_id, now_ms);
    fill(c, out);
    return c.end != 0 || c.duration != 0 ? 1 : 0;
}

int cdMultiUse(int64_t now_ms, QtClientCooldown* out)
{
    const game::TCooldownStorage* storage = game::currentCooldownStorage();
    if (!storage || !out)
        return 0;
    const game::TCooldown c = storage->multiUseCooldown(now_ms);
    fill(c, out);
    return c.end != 0 || c.duration != 0 ? 1 : 0;
}

size_t cdSpells(int64_t now_ms, QtClientSpellCooldown* out, size_t cap)
{
    const game::TCooldownStorage* storage = game::currentCooldownStorage();
    if (!storage)
        return 0;
    const std::map<int, game::TCooldown> all = storage->spellCooldowns(now_ms);
    size_t i = 0;
    for (const auto& [id, c] : all) {
        if (!out || i >= cap)
            break;
        out[i].spell_id = static_cast<uint16_t>(id);
        fill(c, &out[i].cooldown);
        ++i;
    }
    return all.size();
}

struct TCooldownWatch
{
    Subscribers<std::function<void(int)>> listeners;
    int sessionListener = 0;
    std::vector<QMetaObject::Connection> connections;
};

TCooldownWatch& cooldownWatch()
{
    static TCooldownWatch w;
    return w;
}

void bindCooldownStorage()
{
    TCooldownWatch& w = cooldownWatch();
    for (const QMetaObject::Connection& c : w.connections)
        QObject::disconnect(c);
    w.connections.clear();
    game::TCooldownStorage* storage = game::currentCooldownStorage();
    if (!storage)
        return;
    using Storage = cip::tibia::game::TCooldownStorage;
    w.connections.push_back(QObject::connect(storage, &Storage::spellOrSpellGroupCooldownChanged, storage,
                                             [] { cooldownWatch().listeners.call(QTCLIENT_COOLDOWN_SPELL_OR_GROUP); }));
    w.connections.push_back(QObject::connect(storage, &Storage::multiUseCooldownChanged, storage,
                                             [] { cooldownWatch().listeners.call(QTCLIENT_COOLDOWN_MULTI_USE); }));
}

int cdSubscribe(void (*fn)(void* user, int what), void* user)
{
    if (!fn)
        return 0;
    TCooldownWatch& w = cooldownWatch();
    if (w.sessionListener == 0) {
        w.sessionListener = game::subscribeCurrentGameSessionChanged([] {
            bindCooldownStorage();
            cooldownWatch().listeners.call(QTCLIENT_COOLDOWN_SESSION);
        });
        bindCooldownStorage();
    }
    return w.listeners.add([fn, user](const int what) { fn(user, what); });
}

void cdUnsubscribe(int id)
{
    cooldownWatch().listeners.remove(id);
}

const QtClientCooldownApiV1 g_cooldownApi = {
    .struct_size = sizeof(QtClientCooldownApiV1),
    .now_ms = &cdNow,
    .spell_cooldown = &cdSpell,
    .group_cooldown = &cdGroup,
    .multi_use_cooldown = &cdMultiUse,
    .spell_cooldowns = &cdSpells,
    .subscribe_changed = &cdSubscribe,
    .unsubscribe_changed = &cdUnsubscribe,
};

// --- spells ---

spells::TSpellInfoStorage* spellStorage()
{
    return spells::TSpellInfoStorage::instance();
}

void fillSpell(const spells::TSpellInfo& s, QtClientSpellInfo* out)
{
    *out = QtClientSpellInfo{};
    out->id = static_cast<uint16_t>(s.id);
    out->icon_index = static_cast<int16_t>(s.iconIndex);
    out->type = static_cast<uint8_t>(s.type);
    out->group_primary = s.cast.groupPrimary;
    out->group_secondary = s.cast.groupSecondary;
    uint32_t flags = 0;
    if (s.premium)
        flags |= QTCLIENT_SPELL_FLAG_PREMIUM;
    if (s.cast.aggressive || s.aggressive)
        flags |= QTCLIENT_SPELL_FLAG_AGGRESSIVE;
    if (s.isRune)
        flags |= QTCLIENT_SPELL_FLAG_IS_RUNE;
    if (s.isRuneCreatable)
        flags |= QTCLIENT_SPELL_FLAG_RUNE_CREATABLE;
    if (s.autoUnlock)
        flags |= QTCLIENT_SPELL_FLAG_AUTO_UNLOCK;
    if (s.crossHairTarget)
        flags |= QTCLIENT_SPELL_FLAG_CROSSHAIR_TARGET;
    if (s.aimAtTarget)
        flags |= QTCLIENT_SPELL_FLAG_AIM_AT_TARGET;
    out->flags = flags;
    out->minimum_level = static_cast<uint16_t>(s.cast.minimumLevel ? s.cast.minimumLevel : s.minimumLevel);
    out->minimum_magic_level = static_cast<uint16_t>(s.minimumMagicLevel);
    out->mana = static_cast<uint16_t>(std::max(0, s.cast.mana));
    out->soul = s.cast.soul;
    out->cooldown_self_ms = static_cast<uint32_t>(std::max<qint64>(0, s.cast.cooldownSelf));
    out->cooldown_primary_group_ms = static_cast<uint32_t>(std::max<qint64>(0, s.cast.cooldownPrimaryGroup));
    out->cooldown_secondary_group_ms = static_cast<uint32_t>(std::max<qint64>(0, s.cast.cooldownSecondaryGroup));
    out->rune_object_id = static_cast<uint16_t>(s.rune ? s.rune->objectId : s.runeObjectId);
    out->rune_amount = static_cast<uint16_t>(s.rune ? s.rune->amount : s.runeAmount);
    copyUtf8(out->name, sizeof out->name, s.name);
    copyUtf8(out->words, sizeof out->words, s.words);
    copyUtf8(out->parameter_hints, sizeof out->parameter_hints, s.parameterHints);
    copyUtf8(out->vocations, sizeof out->vocations, s.vocations.join(QLatin1Char(',')));
}

size_t gdSpells(QtClientSpellInfo* out, size_t cap)
{
    const spells::TSpellInfoStorage* storage = spellStorage();
    if (!storage)
        return 0;
    size_t i = 0;
    for (const auto& [id, record] : storage->spells()) {
        if (!out || i >= cap)
            break;
        if (const std::optional<spells::TSpellInfo> s = storage->spellInfo(id))
            fillSpell(*s, &out[i]);
        else
            fillSpell(record, &out[i]);
        ++i;
    }
    return storage->spells().size();
}

int gdSpell(uint16_t spell_id, QtClientSpellInfo* out)
{
    const spells::TSpellInfoStorage* storage = spellStorage();
    if (!storage || !out)
        return 0;
    const std::optional<spells::TSpellInfo> s = storage->spellInfo(spell_id);
    if (!s)
        return 0;
    fillSpell(*s, out);
    return 1;
}

int gdSpellOfText(const char* text_utf8, QtClientSpellInfo* out)
{
    const spells::TSpellInfoStorage* storage = spellStorage();
    if (!storage || !out || !text_utf8)
        return 0;
    const spells::TSpellInfo* record = storage->spellOfText(QString::fromUtf8(text_utf8));
    return record ? gdSpell(static_cast<uint16_t>(record->id), out) : 0;
}

int gdRuneSpellOfObject(uint16_t object_type_id, QtClientSpellInfo* out)
{
    const spells::TSpellInfoStorage* storage = spellStorage();
    if (!storage || !out)
        return 0;
    const std::optional<spells::TSpellInfo> s = storage->runeSpellInfo(object_type_id);
    if (!s)
        return 0;
    fillSpell(*s, out);
    return 1;
}

int gdSpellIconUrl(uint16_t spell_id, char* buf, size_t buf_len)
{
    const spells::TSpellInfoStorage* storage = spellStorage();
    const spells::TSpellInfo* record = storage ? storage->spell(spell_id) : nullptr;
    if (!record)
        return -1;
    return writeSizedOut(spells::TSpellInfoStorage::iconUrl(*record), buf, buf_len);
}

int gdCharmInfo(int charm_id, int grade, QtClientCharmInfo* out)
{
    const cyclopedia::TCharmInfo* charm = cyclopedia::charmInfo(charm_id);
    if (!charm || !out)
        return 0;
    *out = QtClientCharmInfo{};
    out->id = charm->id;
    out->kind = charm->kind;
    for (int i = 0; i < 3; ++i) {
        out->cost[i] = charm->cost[i];
        out->percent[i] = charm->percent[i];
    }
    copyUtf8(out->name, sizeof out->name, QString::fromUtf8(charm->name ? charm->name : ""));
    copyUtf8(out->description, sizeof out->description, cyclopedia::charmDescription(charm_id, std::clamp(grade, 1, 3)));
    copyUtf8(out->image_source, sizeof out->image_source, cyclopedia::charmImageSource(charm_id));
    return 1;
}

uint32_t gdOutfitColor(int palette_index)
{
    if (palette_index < 0 || palette_index >= qmlcomponents::outfitpalette::Count)
        return 0;
    return qmlcomponents::outfitpalette::color(palette_index).rgba();
}

uint32_t gdEightBitColor(uint8_t color)
{
    return client::colorFromEightBit(color).rgba();
}

const QtClientGameDataApiV1 g_gameDataApi = {
    .struct_size = sizeof(QtClientGameDataApiV1),
    .spells = &gdSpells,
    .spell = &gdSpell,
    .spell_of_text = &gdSpellOfText,
    .rune_spell_of_object = &gdRuneSpellOfObject,
    .spell_icon_url = &gdSpellIconUrl,
    .charm_info = &gdCharmInfo,
    .outfit_color_rgba = &gdOutfitColor,
    .eight_bit_color_rgba = &gdEightBitColor,
};

} // namespace

const QtClientCooldownApiV1* cooldownApiV1()
{
    return &g_cooldownApi;
}

const QtClientGameDataApiV1* gameDataApiV1()
{
    return &g_gameDataApi;
}

} // namespace tibia::mods
