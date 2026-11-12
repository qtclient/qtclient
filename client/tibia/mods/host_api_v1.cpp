#include "qtclient/abi/host_api_v1.h"

#include "mods/attention_backend.h"
#include "mods/instance_backend.h"
#include "mods/mission_backend.h"
#include "mods/registry.h"
#include "mods/pack_host_abi.h"
#include "mods/weather_driver.h"
#include "mods/worldview.h"
#include "sprites/hdscale.h"

#include "app/clientoptions.h"

#include "game/creaturesession.h"
#include "game/gamesession.h"
#include "game/mapsession.h"
#include "game/thingsession.h"
#include "game/gamenotify.h"
#include "game/tgamenotify.h"
#include "runtime/host.h"

#include "creatures/creaturestorage.h"

#include <client/const.h>
#include <client/item.h>
#include <client/position.h>
#include <teamfinder/teamfinder_types.h>
#include <tibia/assets/things.h>
#include <tibia/schedule/schedule.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <worldmap/worldmapfield.h>

#include <QByteArray>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMetaObject>
#include <QObject>
#include <QQuickWindow>
#include <QString>
#include <QSGNode>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#if !defined(QTCLIENT_HOST_BUILD_ID)
#  define QTCLIENT_HOST_BUILD_ID ""
#endif

namespace {

tibia::game::MapPos toGameMapPos(const QtClientMapPos p)
{
    return {p.x, p.y, p.z};
}

Position toTipPosition(const QtClientMapPos p)
{
    return Position(p.x, p.y, static_cast<uint8_t>(p.z));
}

void fillAbiInventory(const tibia::game::InventorySlotInfo& src, QtClientInventorySlotInfo* dst)
{
    if (!dst)
        return;
    dst->empty = src.empty ? 1 : 0;
    dst->type_id = src.typeId;
    dst->stackable = src.stackable ? 1 : 0;
    dst->count = src.count;
    dst->charges = src.charges;
}

void fillAbiOutfit(const tibia::game::CreatureOutfitInfo& src, QtClientCreatureOutfitInfo* dst)
{
    if (!dst)
        return;
    dst->exists = src.exists ? 1 : 0;
    dst->is_object = src.isObject ? 1 : 0;
    dst->looktype = src.looktype;
    dst->head = src.head;
    dst->body = src.body;
    dst->legs = src.legs;
    dst->feet = src.feet;
    dst->addons = src.addons;
}

void fillAbiSnapshot(const tibia::game::CreatureSnapshot& src, QtClientCreatureSnapshot* dst)
{
    if (!dst)
        return;
    std::memset(dst, 0, sizeof(*dst));
    dst->id = src.id;
    const QByteArray nameUtf8 = QString::fromStdString(src.name).toUtf8();
    std::strncpy(dst->name, nameUtf8.constData(), sizeof(dst->name) - 1);
    dst->pos = {src.pos.x, src.pos.y, src.pos.z};
    dst->health_percent = src.healthPercent;
    dst->shield = src.shield;
    dst->vocation = src.vocation;
    dst->creature_type = src.creatureType;
    dst->is_player = src.isPlayer ? 1 : 0;
    dst->is_npc = src.isNpc ? 1 : 0;
    dst->can_be_seen = src.canBeSeen ? 1 : 0;
    dst->is_dead = src.isDead ? 1 : 0;
    dst->is_local_player = src.isLocalPlayer ? 1 : 0;
    fillAbiOutfit(src.outfit, &dst->outfit);
}

void fillAbiRaceOutfit(const tibia::game::RaceOutfitInfo& src, QtClientRaceOutfitInfo* dst)
{
    if (!dst)
        return;
    std::memset(dst, 0, sizeof(*dst));
    dst->exists = src.exists ? 1 : 0;
    dst->is_object = src.isObject ? 1 : 0;
    dst->looktype = src.looktype;
    dst->head = src.head;
    dst->body = src.body;
    dst->legs = src.legs;
    dst->feet = src.feet;
    dst->addons = src.addons;
    const QByteArray n = QString::fromStdString(src.name).toUtf8();
    std::strncpy(dst->name, n.constData(), sizeof(dst->name) - 1);
}

void fillAbiQuest(const tibia::game::QuestInfo& src, QtClientQuestInfo* dst)
{
    if (!dst)
        return;
    std::memset(dst, 0, sizeof(*dst));
    dst->exists = src.exists ? 1 : 0;
    dst->id = src.id;
    const QByteArray n = QString::fromStdString(src.name).toUtf8();
    std::strncpy(dst->name, n.constData(), sizeof(dst->name) - 1);
}

TeamFinderAssemble teamFinderFromAbi(const QtClientTeamFinderAssemble* a)
{
    if (!a)
        return {};
    TeamFinderAssemble t{};
    t.minLevel = a->min_level;
    t.maxLevel = a->max_level;
    t.vocationIDs = a->vocation_ids;
    t.teamSlots = a->team_slots;
    t.freeSlots = a->free_slots;
    t.partyBool = a->party_bool != 0;
    t.timestamp = a->timestamp;
    t.teamType = a->team_type;
    t.bossID = a->boss_id;
    t.huntType = a->hunt_type;
    t.huntArea = a->hunt_area;
    t.questID = a->quest_id;
    return t;
}

thread_local ItemPtr g_sessFindPlayerItemLast;

QtClientImageView imageViewFromQImage(QImage& img)
{
    QtClientImageView v{};
    if (img.isNull())
        return v;
    // Lighting grids are authored as ARGB32_Premultiplied (toLightImage). The pack ABI
    // documents RGBA8888 bytes — always convert so R/B are not swapped on little-endian.
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);
    v.pixels = img.bits();
    v.width = img.width();
    v.height = img.height();
    v.bytes_per_line = img.bytesPerLine();
    v.format = 0;
    return v;
}

class AbiLightingAdapter final : public tibia::mods::WorldLightingDriver
{
public:
    explicit AbiLightingAdapter(QtClientLightingDriver driver)
        : m_driver(driver)
    {
    }

    bool enabled() const override
    {
        return m_driver.enabled && m_driver.enabled(m_driver.self) != 0;
    }
    bool frameActive() const override
    {
        return m_driver.frame_active && m_driver.frame_active(m_driver.self) != 0;
    }
    bool hasFieldAmbient() const override
    {
        return !m_driver.has_field_ambient || m_driver.has_field_ambient(m_driver.self) != 0;
    }
    qint64 gatherNs() const override
    {
        return m_driver.gather_ns ? m_driver.gather_ns(m_driver.self) : 0;
    }
    qint64 roofsNs() const override
    {
        return m_driver.roofs_ns ? m_driver.roofs_ns(m_driver.self) : 0;
    }
    void setFieldAmbient(QImage ambient) override
    {
        if (!m_driver.set_field_ambient)
            return;
        m_driver.set_field_ambient(m_driver.self, imageViewFromQImage(ambient));
    }
    void setFieldClassic(QImage classic) override
    {
        if (!m_driver.set_field_classic)
            return;
        m_driver.set_field_classic(m_driver.self, imageViewFromQImage(classic));
    }
    void clearSession() override
    {
        if (m_driver.clear_session)
            m_driver.clear_session(m_driver.self);
    }
    bool beginFrame(tibia::mods::LightingMapPos camera, int drawWidth, int drawHeight, int firstFloor,
                    int lastFloor) override
    {
        if (!m_driver.begin_frame)
            return false;
        const QtClientLightingMapPos pos{camera.x, camera.y, camera.z};
        return m_driver.begin_frame(m_driver.self, pos, drawWidth, drawHeight, firstFloor, lastFloor) != 0;
    }
    void beginFloor(int z) override
    {
        if (m_driver.begin_floor)
            m_driver.begin_floor(m_driver.self, z);
    }
    void resetShade(int ix, int iy, bool canShade) override
    {
        if (m_driver.reset_shade)
            m_driver.reset_shade(m_driver.self, ix, iy, canShade ? 1 : 0);
    }
    void endFrame(const void* worldGeometry) override
    {
        if (m_driver.end_frame)
            m_driver.end_frame(m_driver.self, const_cast<void*>(worldGeometry));
    }
    void addCenterLight(int x, int y, uint8_t intensity, uint8_t color) override
    {
        if (m_driver.add_center_light)
            m_driver.add_center_light(m_driver.self, x, y, intensity, color);
    }
    void addFrame(const tibia::mods::LightingFrameDesc& frame) override
    {
        if (!m_driver.add_frame)
            return;
        QtClientLightingFrameDesc f{};
        f.sprite_id = frame.spriteId;
        f.anchor_x = frame.anchorX;
        f.anchor_y = frame.anchorY;
        f.displacement_x = frame.displacementX;
        f.displacement_y = frame.displacementY;
        f.shape = frame.shape;
        f.elevation = frame.elevation;
        f.hook = frame.hook;
        f.on_top = frame.onTop ? 1 : 0;
        f.casts_shadow = frame.castsShadow ? 1 : 0;
        f.lying_corpse = frame.lyingCorpse ? 1 : 0;
        f.has_light = frame.hasLight ? 1 : 0;
        f.light_intensity = frame.lightIntensity;
        f.light_color = frame.lightColor;
        m_driver.add_frame(m_driver.self, &f);
    }
    int stampPhase(int animationPhase) const override
    {
        return m_driver.stamp_phase ? m_driver.stamp_phase(m_driver.self, animationPhase) : animationPhase;
    }
    void beginContext(const tibia::mods::LightingThingContext& ctx) override
    {
        if (!m_driver.begin_context)
            return;
        QtClientLightingThingContext c{};
        c.lift = ctx.lift;
        c.creature = ctx.creature ? 1 : 0;
        c.walking = ctx.walking ? 1 : 0;
        c.local_player = ctx.localPlayer ? 1 : 0;
        c.loose = ctx.loose ? 1 : 0;
        c.transient = ctx.transient ? 1 : 0;
        c.caster = ctx.caster ? 1 : 0;
        c.creature_id = ctx.creatureId;
        m_driver.begin_context(m_driver.self, &c);
    }
    void endContext() override
    {
        if (m_driver.end_context)
            m_driver.end_context(m_driver.self);
    }

private:
    QtClientLightingDriver m_driver{};
};

class AbiOverlayAdapter final : public tibia::mods::WorldOverlayFactory
{
public:
    explicit AbiOverlayAdapter(QtClientWorldOverlayFactory factory)
        : m_factory(factory)
    {
    }

    int order() const override
    {
        return m_factory.order ? m_factory.order(m_factory.self) : 100;
    }

    void updateScene(QQuickWindow* window, QSGNode* camera, QSGNode* afterSprites) override
    {
        if (!m_factory.update_scene)
            return;
        QtClientHandle w{};
        w.ptr = window;
        QtClientHandle c{};
        c.ptr = camera;
        QtClientHandle a{};
        a.ptr = afterSprites;
        m_factory.update_scene(m_factory.self, w, c, a);
    }

    void abandonNode() override
    {
        if (m_factory.abandon_node)
            m_factory.abandon_node(m_factory.self);
    }

    void dropNode() override
    {
        if (m_factory.drop_node)
            m_factory.drop_node(m_factory.self);
    }

private:
    QtClientWorldOverlayFactory m_factory{};
};

std::unique_ptr<AbiLightingAdapter> g_abiLighting;
std::vector<std::unique_ptr<AbiOverlayAdapter>> g_abiOverlays;

constexpr int kMaxBootstraps = 64;
QtClientModBootstrapFn g_bootFns[kMaxBootstraps] = {};
int g_bootCount = 0;
QtClientModBootstrapFn g_earlyFns[kMaxBootstraps] = {};
int g_earlyCount = 0;

using TipBootstrapFn = void (*)(const tibia::mods::ModContext&);

template <int I>
void bootTrampoline(const tibia::mods::ModContext& ctx)
{
    QtClientModLoadCtx load{};
    load.early_ctx = const_cast<tibia::mods::ModContext*>(&ctx);
    if (g_bootFns[I])
        g_bootFns[I](&load);
}

template <int I>
void earlyTrampoline(const tibia::mods::ModContext& ctx)
{
    QtClientModLoadCtx load{};
    load.early_ctx = const_cast<tibia::mods::ModContext*>(&ctx);
    if (g_earlyFns[I])
        g_earlyFns[I](&load);
}

TipBootstrapFn bootTrampolineAt(int i)
{
    static TipBootstrapFn table[kMaxBootstraps] = {
        &bootTrampoline<0>,  &bootTrampoline<1>,  &bootTrampoline<2>,  &bootTrampoline<3>,
        &bootTrampoline<4>,  &bootTrampoline<5>,  &bootTrampoline<6>,  &bootTrampoline<7>,
        &bootTrampoline<8>,  &bootTrampoline<9>,  &bootTrampoline<10>, &bootTrampoline<11>,
        &bootTrampoline<12>, &bootTrampoline<13>, &bootTrampoline<14>, &bootTrampoline<15>,
        &bootTrampoline<16>, &bootTrampoline<17>, &bootTrampoline<18>, &bootTrampoline<19>,
        &bootTrampoline<20>, &bootTrampoline<21>, &bootTrampoline<22>, &bootTrampoline<23>,
        &bootTrampoline<24>, &bootTrampoline<25>, &bootTrampoline<26>, &bootTrampoline<27>,
        &bootTrampoline<28>, &bootTrampoline<29>, &bootTrampoline<30>, &bootTrampoline<31>,
        &bootTrampoline<32>, &bootTrampoline<33>, &bootTrampoline<34>, &bootTrampoline<35>,
        &bootTrampoline<36>, &bootTrampoline<37>, &bootTrampoline<38>, &bootTrampoline<39>,
        &bootTrampoline<40>, &bootTrampoline<41>, &bootTrampoline<42>, &bootTrampoline<43>,
        &bootTrampoline<44>, &bootTrampoline<45>, &bootTrampoline<46>, &bootTrampoline<47>,
        &bootTrampoline<48>, &bootTrampoline<49>, &bootTrampoline<50>, &bootTrampoline<51>,
        &bootTrampoline<52>, &bootTrampoline<53>, &bootTrampoline<54>, &bootTrampoline<55>,
        &bootTrampoline<56>, &bootTrampoline<57>, &bootTrampoline<58>, &bootTrampoline<59>,
        &bootTrampoline<60>, &bootTrampoline<61>, &bootTrampoline<62>, &bootTrampoline<63>,
    };
    return table[i];
}

TipBootstrapFn earlyTrampolineAt(int i)
{
    static TipBootstrapFn table[kMaxBootstraps] = {
        &earlyTrampoline<0>,  &earlyTrampoline<1>,  &earlyTrampoline<2>,  &earlyTrampoline<3>,
        &earlyTrampoline<4>,  &earlyTrampoline<5>,  &earlyTrampoline<6>,  &earlyTrampoline<7>,
        &earlyTrampoline<8>,  &earlyTrampoline<9>,  &earlyTrampoline<10>, &earlyTrampoline<11>,
        &earlyTrampoline<12>, &earlyTrampoline<13>, &earlyTrampoline<14>, &earlyTrampoline<15>,
        &earlyTrampoline<16>, &earlyTrampoline<17>, &earlyTrampoline<18>, &earlyTrampoline<19>,
        &earlyTrampoline<20>, &earlyTrampoline<21>, &earlyTrampoline<22>, &earlyTrampoline<23>,
        &earlyTrampoline<24>, &earlyTrampoline<25>, &earlyTrampoline<26>, &earlyTrampoline<27>,
        &earlyTrampoline<28>, &earlyTrampoline<29>, &earlyTrampoline<30>, &earlyTrampoline<31>,
        &earlyTrampoline<32>, &earlyTrampoline<33>, &earlyTrampoline<34>, &earlyTrampoline<35>,
        &earlyTrampoline<36>, &earlyTrampoline<37>, &earlyTrampoline<38>, &earlyTrampoline<39>,
        &earlyTrampoline<40>, &earlyTrampoline<41>, &earlyTrampoline<42>, &earlyTrampoline<43>,
        &earlyTrampoline<44>, &earlyTrampoline<45>, &earlyTrampoline<46>, &earlyTrampoline<47>,
        &earlyTrampoline<48>, &earlyTrampoline<49>, &earlyTrampoline<50>, &earlyTrampoline<51>,
        &earlyTrampoline<52>, &earlyTrampoline<53>, &earlyTrampoline<54>, &earlyTrampoline<55>,
        &earlyTrampoline<56>, &earlyTrampoline<57>, &earlyTrampoline<58>, &earlyTrampoline<59>,
        &earlyTrampoline<60>, &earlyTrampoline<61>, &earlyTrampoline<62>, &earlyTrampoline<63>,
    };
    return table[i];
}

void abiRegisterMod(QtClientModEntry entry)
{
    tibia::mods::ModEntry e;
    if (entry.identity.id)
        e.identity.id = entry.identity.id;
    if (entry.identity.display_name)
        e.identity.displayName = entry.identity.display_name;
    if (entry.identity.organization)
        e.identity.organization = entry.identity.organization;
    if (entry.identity.organization_domain)
        e.identity.organizationDomain = entry.identity.organization_domain;
    if (entry.identity.bundle_id)
        e.identity.bundleId = entry.identity.bundle_id;
    if (entry.identity.cdn_base_url)
        e.identity.cdnBaseUrl = entry.identity.cdn_base_url;
    e.identity.priority = entry.identity.priority;
    if (entry.bootstrap) {
        if (g_bootCount >= kMaxBootstraps)
            return;
        const int idx = g_bootCount++;
        g_bootFns[idx] = entry.bootstrap;
        e.bootstrap = bootTrampolineAt(idx);
    }
    tibia::mods::registerMod(e);
}

void abiRegisterEarly(QtClientModBootstrapFn fn)
{
    if (!fn || g_earlyCount >= kMaxBootstraps)
        return;
    const int idx = g_earlyCount++;
    g_earlyFns[idx] = fn;
    tibia::mods::registerEarlyBootstrap(earlyTrampolineAt(idx));
}

QtClientApplyImageTintFn g_abiImageTint = nullptr;

void tipApplyImageTint(QImage& image, uint8_t tint)
{
    if (!g_abiImageTint)
        return;
    auto view = imageViewFromQImage(image);
    g_abiImageTint(&view, tint);
}

void abiSetApplyImageTint(QtClientApplyImageTintFn fn)
{
    g_abiImageTint = fn;
    tibia::mods::setApplyImageTintHook(fn ? &tipApplyImageTint : nullptr);
}

void abiSetLighting(const QtClientLightingDriver* driver)
{
    if (!driver) {
        g_abiLighting.reset();
        tibia::mods::setWorldLightingDriver(nullptr);
        return;
    }
    g_abiLighting = std::make_unique<AbiLightingAdapter>(*driver);
    tibia::mods::setWorldLightingDriver(g_abiLighting.get());
}

void abiSetFrameHooks(QtClientWorldFrameHooks hooks)
{
    tibia::mods::WorldFrameHooks h;
    h.requestOutdoorGather = hooks.request_outdoor_gather != 0;
    tibia::mods::setWorldFrameHooks(h);
}

void abiSetMission(void* backend)
{
    tibia::mods::setMissionBackend(static_cast<tibia::mods::MissionBackend*>(backend));
}
void abiSetAttention(void* backend)
{
    tibia::mods::setAttentionBackend(static_cast<tibia::mods::AttentionBackend*>(backend));
}
void abiSetInstance(void* backend)
{
    tibia::mods::setInstanceBackend(static_cast<tibia::mods::InstanceBackend*>(backend));
}
void abiSetWeather(void* driver)
{
    tibia::mods::setWeatherDriver(static_cast<tibia::mods::WeatherDriver*>(driver));
}
void abiAddOverlay(const QtClientWorldOverlayFactory* factory)
{
    if (!factory)
        return;
    g_abiOverlays.push_back(std::make_unique<AbiOverlayAdapter>(*factory));
    tibia::mods::addWorldOverlayFactory(g_abiOverlays.back().get());
}

QtClientLoginSyncEnqueueFn g_abiLoginSync = nullptr;

void tipLoginSync(std::function<void()> fn)
{
    if (!g_abiLoginSync)
        return;
    auto* heap = new std::function<void()>(std::move(fn));
    g_abiLoginSync(
        [](void* user) {
            auto* f = static_cast<std::function<void()>*>(user);
            if (f && *f)
                (*f)();
            delete f;
        },
        heap);
}

void abiSetLoginSync(QtClientLoginSyncEnqueueFn fn)
{
    g_abiLoginSync = fn;
    tibia::mods::setLoginSyncEnqueueHook(fn ? &tipLoginSync : nullptr);
}

void abiSetHdScaler(int factor, QtClientHdScaleFn fn, void* user)
{
    // ImagePtr↔view bridge lands with IdleOT MODULE peel; clear or keep factor only.
    Q_UNUSED(fn);
    Q_UNUSED(user);
    if (!fn) {
        tibia::sprites::setHdScaler(1, nullptr);
        return;
    }
    tibia::sprites::setHdScaler(factor, nullptr);
}

void abiHostOs(char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return;
    const std::string os = tibia::host::app().getOs();
    std::snprintf(buf, buf_len, "%s", os.c_str());
}

void abiEnableFeature(int feature)
{
    tibia::game::enableFeature(static_cast<tibia::client::GameFeature>(feature));
}

void abiSetCustomOs(int operating_system)
{
    tibia::game::setCustomOs(static_cast<tibia::client::OperatingSystem_t>(operating_system));
}

// --- Session nested API ---

int sessIsOnline() { return tibia::game::isOnline() ? 1 : 0; }
int sessIsDead() { return tibia::game::isDead() ? 1 : 0; }
int sessIsFrozen() { return tibia::game::isSceneFrozen() ? 1 : 0; }
void sessSetFrozen(int frozen) { tibia::game::setSceneFrozen(frozen != 0); }
int sessPlayerPresent() { return tibia::game::localPlayer() ? 1 : 0; }

QtClientMapPos sessPlayerMapPos()
{
    const auto p = tibia::game::localPlayerMapPos();
    return QtClientMapPos{p.x, p.y, p.z};
}

int sessPlayerName(char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const std::string name = tibia::game::localPlayerName();
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

uint32_t sessPlayerId() { return tibia::game::localPlayerId(); }
int sessPlayerLevel() { return tibia::game::localPlayerLevel(); }
void sessStopWalk() { tibia::game::localPlayerStopAutoWalk(); }
int sessIsWalking() { return tibia::game::localPlayerIsAutoWalking() ? 1 : 0; }

int sessAutoWalk(QtClientMapPos goal, int retry, int path_mode)
{
    tibia::game::MapPos pos{goal.x, goal.y, goal.z};
    return tibia::game::localPlayerAutoWalk(
               pos, retry != 0, static_cast<tibia::client::AutoWalkPathMode>(path_mode))
        ? 1
        : 0;
}

void sessTalk(const char* text)
{
    if (text)
        tibia::game::talk(text);
}

void sessTurn(int direction)
{
    tibia::game::turn(static_cast<tibia::client::Direction>(direction));
}

uint64_t sessMillis() { return tibia::game::millis(); }
uint8_t sessSeaFloor() { return tibia::game::mapSeaFloor(); }
uint8_t sessWorldLight() { return tibia::game::worldLightIntensity(); }
uint32_t sessItemRev() { return tibia::game::mapItemRevision(); }

int sessCipFeature(int feature)
{
    return tibia::game::cipFeature(static_cast<tibia::client::GameFeature>(feature)) ? 1 : 0;
}

int sessHasReachedMain() { return tibia::game::localPlayerHasReachedMain() ? 1 : 0; }

uint16_t sessInvTypeId(uint8_t slot)
{
    return tibia::game::localPlayerInventoryTypeId(static_cast<tibia::client::InventorySlot>(slot));
}

void sessInvSlot(uint8_t slot, QtClientInventorySlotInfo* out)
{
    if (!out)
        return;
    const auto info = tibia::game::localPlayerInventorySlot(static_cast<tibia::client::InventorySlot>(slot));
    fillAbiInventory(info, out);
}

void sessOpenInv(uint8_t slot)
{
    tibia::game::openLocalPlayerInventory(static_cast<tibia::client::InventorySlot>(slot));
}

uint8_t sessBlessVisual() { return tibia::game::localPlayerBlessVisualState(); }

void sessSetInfoChannel(uint32_t ms) { tibia::game::localPlayerSetInformationChannel(ms); }
void sessClearInfoChannel() { tibia::game::localPlayerClearInformationChannel(); }
void sessSetSoftDead(int v) { tibia::game::localPlayerSetSoftDead(v != 0); }

uint32_t sessLpHealth() { return tibia::game::localPlayerHealth(); }
uint32_t sessLpMaxHealth() { return tibia::game::localPlayerMaxHealth(); }
uint32_t sessLpMana() { return tibia::game::localPlayerMana(); }
uint32_t sessLpMaxMana() { return tibia::game::localPlayerMaxMana(); }
uint16_t sessLpLevelPct() { return tibia::game::localPlayerLevelPercent(); }
uint16_t sessLpStamina() { return tibia::game::localPlayerStamina(); }
uint8_t sessLpVocation() { return tibia::game::localPlayerVocation(); }
uint8_t sessLpShield() { return tibia::game::localPlayerShield(); }
uint64_t sessLpMoney() { return tibia::game::localPlayerTotalMoney(); }
uint32_t sessLpFreeCap() { return tibia::game::localPlayerFreeCapacity(); }
uint32_t sessLpInvCount(uint16_t itemId) { return tibia::game::localPlayerInventoryCount(itemId); }

void sessLpOutfit(QtClientCreatureOutfitInfo* out)
{
    if (!out)
        return;
    const Outfit& o = tibia::game::localPlayerOutfit();
    const bool isObject = o.getId() == 0 && o.getAuxId() != 0;
    const uint16_t looktype = isObject ? o.getAuxId() : o.getId();
    tibia::game::CreatureOutfitInfo info{};
    if (looktype != 0) {
        info.exists = true;
        info.isObject = isObject;
        info.looktype = looktype;
        info.head = o.getHead();
        info.body = o.getBody();
        info.legs = o.getLegs();
        info.feet = o.getFeet();
        info.addons = o.getAddons();
    }
    fillAbiOutfit(info, out);
}

int sessItemName(uint16_t id, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const std::string name = tibia::game::itemName(id);
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

int sessItemIsContainer(uint16_t id) { return tibia::game::itemIsContainer(id) ? 1 : 0; }
int sessItemIsPickupable(uint16_t id) { return tibia::game::itemIsPickupable(id) ? 1 : 0; }
int64_t sessItemMeanPrice(uint16_t id) { return tibia::game::itemMeanPrice(id); }
int64_t sessItemMaxNpcSale(uint16_t id) { return tibia::game::itemMaxNpcSaleGoldPrice(id); }

int sessRaceName(uint32_t raceId, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const std::string name = tibia::game::raceName(raceId);
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

int sessRaceExists(uint32_t raceId) { return tibia::game::raceExists(raceId) ? 1 : 0; }
int sessRaceIsBoss(uint32_t raceId) { return tibia::game::raceIsBoss(raceId) ? 1 : 0; }

void sessRaceOutfitAbi(uint32_t raceId, QtClientRaceOutfitInfo* out)
{
    if (!out)
        return;
    fillAbiRaceOutfit(tibia::game::raceOutfit(raceId), out);
}

int sessRaceIdsByName(const char* name, uint32_t* out_ids, int max_ids)
{
    if (!name || !out_ids || max_ids <= 0)
        return 0;
    const std::vector<uint32_t> ids = tibia::game::raceIdsByName(std::string(name));
    const int n = static_cast<int>(std::min(ids.size(), static_cast<size_t>(max_ids)));
    for (int i = 0; i < n; ++i)
        out_ids[i] = ids[static_cast<size_t>(i)];
    return n;
}

int sessAllRaceIds(uint32_t* out_ids, int max_ids)
{
    if (!out_ids || max_ids <= 0)
        return 0;
    const std::vector<uint32_t> ids = tibia::game::allRaceIds();
    const int n = static_cast<int>(std::min(ids.size(), static_cast<size_t>(max_ids)));
    for (int i = 0; i < n; ++i)
        out_ids[i] = ids[static_cast<size_t>(i)];
    return n;
}

void sessQuestInfoAbi(uint16_t id, QtClientQuestInfo* out)
{
    if (!out)
        return;
    fillAbiQuest(tibia::game::questInfo(id), out);
}

int sessAllQuestInfos(QtClientQuestInfo* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;
    const std::vector<tibia::game::QuestInfo> all = tibia::game::allQuestInfos();
    const int n = static_cast<int>(std::min(all.size(), static_cast<size_t>(max_out)));
    for (int i = 0; i < n; ++i)
        fillAbiQuest(all[static_cast<size_t>(i)], &out[i]);
    return n;
}

void sessCreatureAt(QtClientMapPos pos, QtClientCreatureSnapshot* out)
{
    if (!out)
        return;
    std::memset(out, 0, sizeof(*out));
    tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(toTipPosition(pos));
    if (!tile)
        return;
    uint32_t topCreature = 0;
    for (int i = tile->getThingCount() - 1; i >= 0; --i) {
        if (const uint32_t cid = tile->stackCreatureId(i)) {
            topCreature = cid;
            break;
        }
    }
    fillAbiSnapshot(tibia::game::creatureAt(topCreature), out);
}

void sessCreatureAtId(uint32_t creature_id, QtClientCreatureSnapshot* out)
{
    if (!out)
        return;
    fillAbiSnapshot(tibia::game::creatureAt(creature_id), out);
}

int sessSpectatorsNear(QtClientMapPos center, int multi_floor, int left, int right, int top, int bottom,
                       QtClientCreatureSnapshot* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;
    const std::vector<tibia::game::CreatureSnapshot> snaps =
        tibia::game::spectatorsNear(toGameMapPos(center), multi_floor != 0, left, right, top, bottom);
    const int n = static_cast<int>(std::min(snaps.size(), static_cast<size_t>(max_out)));
    for (int i = 0; i < n; ++i)
        fillAbiSnapshot(snaps[static_cast<size_t>(i)], &out[i]);
    return n;
}

void sessAwareRange(QtClientMapAwareRange* out)
{
    if (!out)
        return;
    const auto r = tibia::game::awareRange();
    out->left = r.left;
    out->top = r.top;
    out->right = r.right;
    out->bottom = r.bottom;
}

int sessItemHits(QtClientMapPos pos, QtClientMapUseHit* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;
    const std::vector<tibia::game::MapUseHit> hits = tibia::game::itemHits(toGameMapPos(pos));
    const int n = static_cast<int>(std::min(hits.size(), static_cast<size_t>(max_out)));
    for (int i = 0; i < n; ++i) {
        const auto& h = hits[static_cast<size_t>(i)];
        out[i].ok = h.ok ? 1 : 0;
        out[i].item_id = h.itemId;
        out[i].stack_pos = h.stackPos;
    }
    return n;
}

void sessTopUseHit(QtClientMapPos pos, QtClientMapUseHit* out)
{
    if (!out)
        return;
    const auto h = tibia::game::topUseHit(toGameMapPos(pos));
    out->ok = h.ok ? 1 : 0;
    out->item_id = h.itemId;
    out->stack_pos = h.stackPos;
}

int sessFindItemsInRange(QtClientMapPos center, int range_x, int range_y, uint16_t type_id_or_0,
                         QtClientMapItemHit* out, int max_out)
{
    if (!out || max_out <= 0 || range_x < 0 || range_y < 0)
        return 0;
    const Position c = toTipPosition(center);
    if (!c.isValid())
        return 0;

    int written = 0;
    for (int x = c.x - range_x; x <= c.x + range_x && written < max_out; ++x) {
        for (int y = c.y - range_y; y <= c.y + range_y && written < max_out; ++y) {
            tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(Position(x, y, c.z));
            if (!tile)
                continue;
            const int count = tile->getThingCount();
            for (int sp = 0; sp < count && written < max_out; ++sp) {
                if (tile->stackCreatureId(sp) != 0)
                    continue;
                const uint16_t clientId = tile->itemClientIdAt(sp);
                if (clientId == 0)
                    continue;
                if (type_id_or_0 != 0 && clientId != type_id_or_0)
                    continue;
                if (!g_things.isValidDatId(clientId, ThingCategoryItem))
                    continue;
                out[written].pos = {x, y, static_cast<int>(c.z)};
                out[written].type_id = clientId;
                out[written].stack_pos = sp;
                ++written;
            }
        }
    }
    return written;
}

int sessFindItemsInRangeTypes(QtClientMapPos center, int range_x, int range_y, const uint16_t* type_ids,
                              int type_count, QtClientMapItemHit* out, int max_out)
{
    if (!out || max_out <= 0 || range_x < 0 || range_y < 0)
        return 0;
    const Position c = toTipPosition(center);
    if (!c.isValid())
        return 0;

    int written = 0;
    for (int x = c.x - range_x; x <= c.x + range_x && written < max_out; ++x) {
        for (int y = c.y - range_y; y <= c.y + range_y && written < max_out; ++y) {
            tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(Position(x, y, c.z));
            if (!tile)
                continue;
            const int count = tile->getThingCount();
            for (int sp = 0; sp < count && written < max_out; ++sp) {
                if (tile->stackCreatureId(sp) != 0)
                    continue;
                const uint16_t clientId = tile->itemClientIdAt(sp);
                if (clientId == 0)
                    continue;
                if (type_count > 0) {
                    bool match = false;
                    for (int t = 0; t < type_count; ++t) {
                        if (clientId == type_ids[static_cast<size_t>(t)]) {
                            match = true;
                            break;
                        }
                    }
                    if (!match)
                        continue;
                }
                if (!g_things.isValidDatId(clientId, ThingCategoryItem))
                    continue;
                out[written].pos = {x, y, static_cast<int>(c.z)};
                out[written].type_id = clientId;
                out[written].stack_pos = sp;
                ++written;
            }
        }
    }
    return written;
}

int sessSightClear(QtClientMapPos from, QtClientMapPos to)
{
    return tibia::game::isSightClear(toGameMapPos(from), toGameMapPos(to)) ? 1 : 0;
}

int sessWalkable(QtClientMapPos pos, int ignore_creatures)
{
    return tibia::game::isWalkable(toGameMapPos(pos), ignore_creatures != 0) ? 1 : 0;
}

int sessCanTravelPath(QtClientMapPos from, QtClientMapPos to, int max_complexity)
{
    return tibia::game::canTravelPath(toGameMapPos(from), toGameMapPos(to), max_complexity) ? 1 : 0;
}

void sessSetPathAvoid(QtClientMapPos pos, int avoid)
{
    tibia::game::setPathAvoid(toGameMapPos(pos), avoid != 0);
}

void sessClearPathAvoids()
{
    tibia::game::clearPathAvoids();
}

void sessCanTravelPathAsync(QtClientMapPos from, QtClientMapPos to, QtClientTravelPathAsyncCallback cb, void* user)
{
    if (!cb)
        return;
    tibia::game::canTravelPathAsync(
        toGameMapPos(from), toGameMapPos(to),
        [cb, user](const bool ok) { tibia::schedule::post([cb, user, ok] { cb(user, ok ? 1 : 0); }); });
}

void sessSafeLogout() { tibia::game::safeLogout(); }
void sessRequestBless() { tibia::game::requestBless(); }
void sessPartyJoin(uint32_t id) { tibia::game::partyJoin(id); }
void sessSetAttacking(uint32_t id) { tibia::game::setAttackingCreatureId(id); }
uint32_t sessAttackingId() { return tibia::game::attackingCreatureId(); }

int sessCharacterName(char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const std::string name = tibia::game::characterName();
    std::snprintf(buf, buf_len, "%s", name.c_str());
    return static_cast<int>(std::strlen(buf));
}

int sessIsSwitchingChar() { return tibia::game::isSwitchingCharacter() ? 1 : 0; }

void sessQuickLootBw(uint8_t filter, uint16_t size, const uint16_t* listed_items, int listed_count)
{
    if (!listed_items || listed_count < 0)
        return;
    std::vector<uint16_t> v(static_cast<size_t>(listed_count));
    for (int i = 0; i < listed_count; ++i)
        v[static_cast<size_t>(i)] = listed_items[static_cast<size_t>(i)];
    tibia::game::requestQuickLootBlackWhiteList(filter, size, v);
}

void sessOpenQuickLoot(uint8_t action, uint8_t category, QtClientMapPos pos, uint16_t item_id, uint8_t stackpos,
                       int use_main_as_fallback)
{
    tibia::game::openContainerQuickLoot(action, category, toTipPosition(pos), item_id, stackpos,
                                       use_main_as_fallback != 0);
}

void sessStatusBestiary(uint16_t race_id, int status)
{
    tibia::game::sendStatusTrackerBestiary(race_id, status != 0);
}

void sessBestiarySearch(uint16_t race_id) { tibia::game::requestBestiarySearch(race_id); }

void sessFriendPayload(const uint8_t* bytes, size_t len)
{
    if (!bytes && len > 0)
        return;
    tibia::game::sendFriendSystemPayload(std::string(reinterpret_cast<const char*>(bytes), len));
}

void sessOpenPrivate(const char* receiver_utf8)
{
    if (!receiver_utf8)
        return;
    tibia::game::openPrivateChannel(receiver_utf8);
}

void sessTfList() { tibia::game::requestTeamFinderList(); }
void sessTfLeader() { tibia::game::requestTeamFinderLeader(); }
void sessTfJoin(uint32_t id) { tibia::game::joinTeamFinder(id); }
void sessTfLeave(uint32_t id) { tibia::game::leaveTeamFinder(id); }

void sessTfCreate(const QtClientTeamFinderAssemble* a)
{
    tibia::game::createTeamFinder(teamFinderFromAbi(a));
}

void sessTfResetLeader() { tibia::game::resetTeamFinderLeader(); }

void sessTfUpdateMember(uint32_t member_id, uint8_t status)
{
    tibia::game::updateTeamFinderMember(member_id, status);
}

void sessUseInvOnCreature(uint16_t item_id, uint32_t creature_id)
{
    tibia::game::useInventoryItemOnCreature(item_id, creature_id);
}

void sessUseMapObject(QtClientMapPos pos, uint16_t item_id, int stack_pos)
{
    tibia::game::useMapObject(toGameMapPos(pos), item_id, stack_pos);
}

void sessUseInvWithMap(uint16_t item_id, QtClientMapPos to_pos, uint16_t to_item_id, int to_stack_pos)
{
    tibia::game::useInventoryItemWithMapObject(item_id, toGameMapPos(to_pos), to_item_id, to_stack_pos);
}

void sessClearEffectVariants() { tibia::game::clearEffectVariants(); }

void sessRegisterEffectVariant(uint16_t virtual_id, uint16_t base_id, const char* shader_utf8, int under_creatures)
{
    if (!shader_utf8)
        return;
    tibia::game::registerEffectVariant(virtual_id, base_id, std::string(shader_utf8), under_creatures != 0);
}

void sessRegAttachedByThing(uint16_t id, const char* note_utf8, uint16_t base_id, uint8_t category)
{
    if (!note_utf8)
        return;
    tibia::game::registerAttachedEffectByThing(id, std::string_view(note_utf8), base_id, category);
}

void sessRemoveAttached(uint16_t id) { tibia::game::removeAttachedEffect(id); }

int sessCreatureTypeIsSummon(uint8_t t) { return tibia::game::creatureTypeIsSummon(t) ? 1 : 0; }

QtClientHandle sessFindPlayerItem(uint32_t item_id, int sub_type)
{
    g_sessFindPlayerItemLast = tibia::game::findPlayerItem(item_id, sub_type);
    QtClientHandle h{};
    h.ptr = g_sessFindPlayerItemLast.get();
    return h;
}

int sessSetItemThingShape(uint16_t item_id, const char* shape_name_utf8)
{
    if (!shape_name_utf8)
        return 0;
    return tibia::game::setItemThingShape(item_id, std::string_view(shape_name_utf8)) ? 1 : 0;
}

const QtClientSessionApiV1 g_sessionApi = {
    .is_online = &sessIsOnline,
    .is_dead = &sessIsDead,
    .is_scene_frozen = &sessIsFrozen,
    .set_scene_frozen = &sessSetFrozen,
    .local_player_present = &sessPlayerPresent,
    .local_player_map_pos = &sessPlayerMapPos,
    .local_player_name = &sessPlayerName,
    .local_player_id = &sessPlayerId,
    .local_player_level = &sessPlayerLevel,
    .local_player_stop_auto_walk = &sessStopWalk,
    .local_player_is_auto_walking = &sessIsWalking,
    .local_player_auto_walk = &sessAutoWalk,
    .talk = &sessTalk,
    .turn = &sessTurn,
    .millis = &sessMillis,
    .map_sea_floor = &sessSeaFloor,
    .world_light_intensity = &sessWorldLight,
    .map_item_revision = &sessItemRev,
    .cip_feature = &sessCipFeature,
    .enable_feature = &abiEnableFeature,
    .set_custom_os = &abiSetCustomOs,

    .local_player_has_reached_main = &sessHasReachedMain,
    .local_player_inventory_type_id = &sessInvTypeId,
    .local_player_inventory_slot = &sessInvSlot,
    .open_local_player_inventory = &sessOpenInv,

    .local_player_bless_visual_state = &sessBlessVisual,
    .local_player_set_information_channel = &sessSetInfoChannel,
    .local_player_clear_information_channel = &sessClearInfoChannel,
    .local_player_set_soft_dead = &sessSetSoftDead,

    .local_player_health = &sessLpHealth,
    .local_player_max_health = &sessLpMaxHealth,
    .local_player_mana = &sessLpMana,
    .local_player_max_mana = &sessLpMaxMana,
    .local_player_level_percent = &sessLpLevelPct,
    .local_player_stamina = &sessLpStamina,
    .local_player_vocation = &sessLpVocation,
    .local_player_shield = &sessLpShield,
    .local_player_total_money = &sessLpMoney,
    .local_player_free_capacity = &sessLpFreeCap,
    .local_player_inventory_count = &sessLpInvCount,
    .local_player_outfit = &sessLpOutfit,

    .item_name = &sessItemName,
    .item_is_container = &sessItemIsContainer,
    .item_is_pickupable = &sessItemIsPickupable,
    .item_mean_price = &sessItemMeanPrice,
    .item_max_npc_sale_gold_price = &sessItemMaxNpcSale,

    .race_name = &sessRaceName,
    .race_exists = &sessRaceExists,
    .race_is_boss = &sessRaceIsBoss,
    .race_outfit = &sessRaceOutfitAbi,
    .race_ids_by_name = &sessRaceIdsByName,
    .all_race_ids = &sessAllRaceIds,

    .quest_info = &sessQuestInfoAbi,
    .all_quest_infos = &sessAllQuestInfos,

    .creature_at = &sessCreatureAt,
    .creature_at_id = &sessCreatureAtId,
    .spectators_near = &sessSpectatorsNear,
    .aware_range = &sessAwareRange,

    .item_hits = &sessItemHits,
    .top_use_hit = &sessTopUseHit,
    .find_items_in_range = &sessFindItemsInRange,
    .find_items_in_range_types = &sessFindItemsInRangeTypes,

    .is_sight_clear = &sessSightClear,
    .is_walkable = &sessWalkable,
    .can_travel_path = &sessCanTravelPath,
    .can_travel_path_async = &sessCanTravelPathAsync,

    .safe_logout = &sessSafeLogout,
    .request_bless = &sessRequestBless,
    .party_join = &sessPartyJoin,
    .set_attacking_creature_id = &sessSetAttacking,
    .attacking_creature_id = &sessAttackingId,

    .character_name = &sessCharacterName,
    .is_switching_character = &sessIsSwitchingChar,

    .request_quick_loot_black_white_list = &sessQuickLootBw,
    .open_container_quick_loot = &sessOpenQuickLoot,

    .send_status_tracker_bestiary = &sessStatusBestiary,
    .request_bestiary_search = &sessBestiarySearch,

    .send_friend_system_payload = &sessFriendPayload,
    .open_private_channel = &sessOpenPrivate,

    .request_team_finder_list = &sessTfList,
    .request_team_finder_leader = &sessTfLeader,
    .join_team_finder = &sessTfJoin,
    .leave_team_finder = &sessTfLeave,
    .create_team_finder = &sessTfCreate,
    .reset_team_finder_leader = &sessTfResetLeader,
    .update_team_finder_member = &sessTfUpdateMember,

    .use_inventory_item_on_creature = &sessUseInvOnCreature,
    .use_map_object = &sessUseMapObject,
    .use_inventory_item_with_map_object = &sessUseInvWithMap,

    .clear_effect_variants = &sessClearEffectVariants,
    .register_effect_variant = &sessRegisterEffectVariant,
    .register_attached_effect_by_thing = &sessRegAttachedByThing,
    .remove_attached_effect = &sessRemoveAttached,

    .creature_type_is_summon = &sessCreatureTypeIsSummon,

    .find_player_item = &sessFindPlayerItem,

    .set_item_thing_shape = &sessSetItemThingShape,

    .set_path_avoid = &sessSetPathAvoid,
    .clear_path_avoids = &sessClearPathAvoids,
};

// --- Notify nested API ---

struct NotifySimpleSlot {
    QtClientNotifySimpleFn fn = nullptr;
    void* user = nullptr;
};

std::mutex g_notifyMu;
std::unordered_map<int, NotifySimpleSlot> g_notifySimple;

int notifySubscribeSimple(const char* field, QtClientNotifySimpleFn fn, void* user)
{
    if (!field || !fn)
        return 0;
    auto slot = std::make_shared<NotifySimpleSlot>(NotifySimpleSlot{fn, user});
    const int id = tibia::game::TGameNotify::instance()->subscribe(
        field, [slot](const tibia::game::Args&) -> std::any {
            if (slot->fn)
                slot->fn(slot->user);
            return {};
        });
    std::lock_guard lock(g_notifyMu);
    g_notifySimple[id] = *slot;
    return id;
}

int notifySubscribeField(const char* field, QtClientNotifyFieldFn fn, void* user)
{
    if (!field || !fn)
        return 0;
    return tibia::game::TGameNotify::instance()->subscribe(
        field, [fn, user](const tibia::game::Args& args) -> std::any {
            fn(user, const_cast<tibia::game::Args*>(&args));
            return {};
        });
}

void notifyUnsubscribe(int id)
{
    tibia::game::TGameNotify::instance()->unsubscribe(id);
    std::lock_guard lock(g_notifyMu);
    g_notifySimple.erase(id);
}

int notifySubWorldTime(void (*fn)(void* user, int hour, int minute), void* user)
{
    if (!fn)
        return 0;
    return tibia::game::subscribeWorldTime([fn, user](int hour, int minute) { fn(user, hour, minute); });
}

void notifyUnsubWorldTime(int id) { tibia::game::unsubscribeWorldTime(id); }

int notifySubSessionEnd(void (*fn)(void* user), void* user)
{
    if (!fn)
        return 0;
    return tibia::game::subscribeSessionEnd([fn, user]() { fn(user); });
}

void notifyUnsubSessionEnd(int id) { tibia::game::unsubscribeSessionEnd(id); }

int notifyArgString(void* tip_args, size_t index, char* buf, size_t buf_len)
{
    if (!tip_args || !buf || buf_len == 0)
        return -1;
    const auto& args = *static_cast<const tibia::game::Args*>(tip_args);
    const QByteArray utf8 = tibia::game::argString(args, index).toUtf8();
    std::snprintf(buf, buf_len, "%s", utf8.constData());
    return static_cast<int>(std::strlen(buf));
}

int64_t notifyArgInt(void* tip_args, size_t index)
{
    if (!tip_args)
        return 0;
    return tibia::game::argInt(*static_cast<const tibia::game::Args*>(tip_args), index);
}

int notifyArgBool(void* tip_args, size_t index)
{
    if (!tip_args)
        return 0;
    return tibia::game::argBool(*static_cast<const tibia::game::Args*>(tip_args), index) ? 1 : 0;
}

int notifyEmitIntString(const char* field, int64_t a0, const char* a1)
{
    if (!field || !a1)
        return -1;
    tibia::game::notify(field, static_cast<tibia::client::MessageMode>(static_cast<uint8_t>(a0)), std::string(a1));
    return 0;
}

const QtClientNotifyApiV1 g_notifyApi = {
    .subscribe_simple = &notifySubscribeSimple,
    .subscribe_field = &notifySubscribeField,
    .unsubscribe = &notifyUnsubscribe,
    .subscribe_world_time = &notifySubWorldTime,
    .unsubscribe_world_time = &notifyUnsubWorldTime,
    .subscribe_session_end = &notifySubSessionEnd,
    .unsubscribe_session_end = &notifyUnsubSessionEnd,
    .arg_string = &notifyArgString,
    .arg_int = &notifyArgInt,
    .arg_bool = &notifyArgBool,
    .emit_int_string = &notifyEmitIntString,
};

// --- Options nested API ---

char* dupCString(const QByteArray& bytes)
{
    char* p = static_cast<char*>(std::malloc(static_cast<size_t>(bytes.size()) + 1));
    if (!p)
        return nullptr;
    std::memcpy(p, bytes.constData(), static_cast<size_t>(bytes.size()));
    p[bytes.size()] = '\0';
    return p;
}

int optLoadJson(char** out_json)
{
    if (!out_json)
        return -1;
    const QJsonObject opts = tibia::app::ClientOptions::load().options();
    const QByteArray bytes = QJsonDocument(opts).toJson(QJsonDocument::Compact);
    *out_json = dupCString(bytes);
    return *out_json ? 0 : -1;
}

int optStore(const char* name, const char* value_json)
{
    if (!name || !value_json)
        return -1;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(value_json));
    if (doc.isNull() && QByteArray(value_json).trimmed() != QByteArray("null")) {
        // Allow raw JSON strings that are values: try wrap parse of single value via QJsonValue
        QJsonParseError err{};
        const QJsonDocument asArr = QJsonDocument::fromJson(QByteArray("[") + value_json + "]", &err);
        if (err.error != QJsonParseError::NoError || !asArr.isArray() || asArr.array().isEmpty())
            return -1;
        tibia::app::ClientOptions::store(QString::fromUtf8(name), asArr.array().at(0));
        return 0;
    }
    if (doc.isObject())
        tibia::app::ClientOptions::store(QString::fromUtf8(name), doc.object());
    else if (doc.isArray())
        tibia::app::ClientOptions::store(QString::fromUtf8(name), doc.array());
    else
        tibia::app::ClientOptions::store(QString::fromUtf8(name), QJsonValue());
    return 0;
}

void optFree(char* p) { std::free(p); }

struct OptChangedSlot {
    QtClientOptionChangedFn fn = nullptr;
    void* user = nullptr;
    QMetaObject::Connection conn;
};

std::mutex g_optMu;
std::unordered_map<int, OptChangedSlot> g_optSlots;
int g_optNextId = 1;

int optSubscribeChanged(QtClientOptionChangedFn fn, void* user)
{
    if (!fn)
        return 0;
    std::lock_guard lock(g_optMu);
    const int id = g_optNextId++;
    OptChangedSlot slot;
    slot.fn = fn;
    slot.user = user;
    slot.conn = QObject::connect(
        tibia::app::ClientOptionsNotifier::instance(),
        &tibia::app::ClientOptionsNotifier::optionChanged,
        [id](const QString& name) {
            std::lock_guard lock(g_optMu);
            const auto it = g_optSlots.find(id);
            if (it == g_optSlots.end() || !it->second.fn)
                return;
            const QByteArray utf8 = name.toUtf8();
            it->second.fn(it->second.user, utf8.constData());
        });
    g_optSlots.emplace(id, std::move(slot));
    return id;
}

void optUnsubscribeChanged(int id)
{
    std::lock_guard lock(g_optMu);
    const auto it = g_optSlots.find(id);
    if (it == g_optSlots.end())
        return;
    QObject::disconnect(it->second.conn);
    g_optSlots.erase(it);
}

const QtClientOptionsApiV1 g_optionsApi = {
    .load_options_json = &optLoadJson,
    .store = &optStore,
    .free_string = &optFree,
    .subscribe_changed = &optSubscribeChanged,
    .unsubscribe_changed = &optUnsubscribeChanged,
};

const QtClientHostApiV1 g_hostApi = {
    .api_version = QTCLIENT_HOST_API_V1_VERSION,
    .host_build_id = QTCLIENT_HOST_BUILD_ID,
    .register_mod = &abiRegisterMod,
    .register_early_bootstrap = &abiRegisterEarly,
    .set_ensure_mods_hook = &tibia::mods::setEnsureModsHook,
    .set_effect_tint_hook = &tibia::mods::setEffectTintHook,
    .set_effect_variant_tint_hook = &tibia::mods::setEffectVariantTintHook,
    .set_apply_image_tint_hook = &abiSetApplyImageTint,
    .set_mission_backend = &abiSetMission,
    .set_attention_backend = &abiSetAttention,
    .set_instance_backend = &abiSetInstance,
    .set_login_sync_enqueue_hook = &abiSetLoginSync,
    .set_feature_extras_hook = &tibia::mods::setFeatureExtrasHook,
    .set_world_lighting_driver = &abiSetLighting,
    .add_world_overlay_factory = &abiAddOverlay,
    .set_world_frame_hooks = &abiSetFrameHooks,
    .set_weather_driver = &abiSetWeather,
    .set_hd_scaler = &abiSetHdScaler,
    .host_os = &abiHostOs,
    .enable_feature = &abiEnableFeature,
    .set_custom_os = &abiSetCustomOs,
    .session = &g_sessionApi,
    .notify = &g_notifyApi,
    .options = &g_optionsApi,
    .pack_host = tibia::mods::packHostAbiTable(),
};

} // namespace

extern "C" const QtClientHostApiV1* qtclient_host_api_v1(void)
{
    return &g_hostApi;
}
