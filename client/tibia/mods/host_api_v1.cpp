// Host API v9: the root table (abi/host_api_v1.h) a pack's qtclient_mod_register gets:
// bootstraps, render / sprite hooks, and the domain tables (each in its <domain>_abi.cpp).
#include "qtclient/abi/host_api_v1.h"
#include "qtclient/abi/const.h"

#include "mods/abi_internal.h"
#include "mods/host_api.h"
#include "mods/registry.h"
#include "mods/textureatlashint.h"
#include "mods/worldview.h"
#include "sprites/hdscale.h"

#include "input/keyboardshortcuthandler.h"
#include "gamewindow/buttonbarcontroller.h"
#include "creatures/creaturedecoration.h"
#include "effects/effectremap.h"
#include "renderer/worldframeview.h"
#include "sound/ambiencehold.h"
#include "runtime/host.h"

#include <QImage>
#include <QKeySequence>
#include <QQuickWindow>
#include <QSGNode>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

extern "C" const char qtclient_host_build_id[];

namespace {

QtClientImageView imageViewFromQImage(QImage& img)
{
    QtClientImageView v{};
    if (img.isNull())
        return v;
    // Lighting grids are authored as ARGB32_Premultiplied (TLightMap::toImage). The pack ABI
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

std::vector<std::unique_ptr<AbiOverlayAdapter>> g_abiOverlays;

constexpr int kMaxBootstraps = 64;
QtClientModBootstrapFn g_bootFns[kMaxBootstraps] = {};
int g_bootCount = 0;
QtClientModBootstrapFn g_earlyFns[kMaxBootstraps] = {};
int g_earlyCount = 0;

using TipBootstrapFn = void (*)(const tibia::mods::ModContext&);

/// A pack bootstrap with the game window context (ctx->game); the pack's ids are the
/// loader's (register time). Exceptions never cross the C boundary (packs catch theirs).
void runBootstrap(const QtClientModBootstrapFn fn, const tibia::mods::ModContext& ctx)
{
    if (!fn)
        return;
    QtClientModLoadCtx load{};
    load.host_build_id = qtclient_host_build_id;
    load.api_version = QTCLIENT_HOST_API_V1_VERSION;
    load.game = tibia::mods::gameContextFor(ctx);
    fn(&load);
}

template <int I>
void bootTrampoline(const tibia::mods::ModContext& ctx)
{
    runBootstrap(g_bootFns[I], ctx);
}

template <int I>
void earlyTrampoline(const tibia::mods::ModContext& ctx)
{
    runBootstrap(g_earlyFns[I], ctx);
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
    tibia::mods::setWorldLightingDriverAbi(driver);
}

void abiSetFrameHooks(QtClientWorldFrameHooks hooks)
{
    tibia::mods::setWorldFrameHooksAbi(hooks);
}

QtClientEffectRemapFn g_abiEffectRemap = nullptr;
void* g_abiEffectRemapUser = nullptr;

bool tipEffectRemap(void*, const uint16_t effectId, tibia::effects::TEffectRemap& out)
{
    QtClientEffectRemap remap{};
    if (!g_abiEffectRemap || !g_abiEffectRemap(g_abiEffectRemapUser, effectId, &remap))
        return false;
    out.drawId = remap.draw_id;
    out.tint = remap.tint;
    out.underCreatures = remap.under_creatures != 0;
    return true;
}

void abiSetEffectRemap(QtClientEffectRemapFn fn, void* user)
{
    g_abiEffectRemap = fn;
    g_abiEffectRemapUser = fn ? user : nullptr;
    tibia::effects::setEffectRemapHook(fn ? &tipEffectRemap : nullptr, nullptr);
}

QtClientCreatureDecorationFn g_abiDecoration = nullptr;
void* g_abiDecorationUser = nullptr;

size_t tipCreatureDecoration(void*, const uint32_t creatureId, const bool overOutfit,
                             tibia::creatures::TCreatureDecoration* out, const size_t cap)
{
    if (!g_abiDecoration || !out || cap == 0)
        return 0;
    constexpr size_t MaxDecorations = 16;
    QtClientCreatureDecoration decorations[MaxDecorations] = {};
    const size_t limit = std::min(cap, MaxDecorations);
    const size_t count = std::min(
        g_abiDecoration(g_abiDecorationUser, creatureId,
                        overOutfit ? QTCLIENT_CREATURE_LAYER_OVER : QTCLIENT_CREATURE_LAYER_UNDER, decorations, limit),
        limit);
    for (size_t i = 0; i < count; ++i) {
        const QtClientCreatureDecoration& d = decorations[i];
        out[i].appearanceId = d.appearance_id;
        out[i].category = d.category;
        out[i].tint = d.tint;
        out[i].offsetX = d.offset_x;
        out[i].offsetY = d.offset_y;
        out[i].phase = d.phase;
        out[i].patternX = d.pattern_x;
        out[i].opacity = d.opacity;
    }
    return count;
}

void abiSetCreatureDecoration(QtClientCreatureDecorationFn fn, void* user)
{
    g_abiDecoration = fn;
    g_abiDecorationUser = fn ? user : nullptr;
    tibia::creatures::setCreatureDecorationHook(fn ? &tipCreatureDecoration : nullptr, nullptr);
}

void abiRegisterButtonBarShortcut(const char* id, const char* label, const char* image, const int defaultEnabled,
                                  void (*run)(void*), void* user)
{
    if (!id || !run)
        return;
    tibia::gamewindow::TButtonBarController::addExtensionShortcut(
        QString::fromUtf8(id), label ? QString::fromUtf8(label) : QString(), [run, user] { run(user); },
        defaultEnabled != 0, image ? QString::fromUtf8(image) : QString());
}

void abiRegisterHotkeyAction(const char* name, const char* defaultKeys, const char* description,
                             void (*run)(void*), void* user)
{
    if (!name || !run)
        return;
    tibia::input::TKeyboardShortcutHandler::addExtensionAction(
        QString::fromUtf8(name),
        defaultKeys ? QKeySequence(QString::fromUtf8(defaultKeys), QKeySequence::PortableText) : QKeySequence(),
        [run, user] { run(user); }, description ? QString::fromUtf8(description) : QString());
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

QtClientHdScaleFn g_abiHdScale = nullptr;
void* g_abiHdScaleUser = nullptr;

/// The pack's scaler over a sprite (RGBA8888): the pack mallocs `out->pixels`; the host
/// copies the image and frees them.
QImage tipHdScale(const QImage& src)
{
    if (!g_abiHdScale)
        return {};
    QImage input = src.format() == QImage::Format_RGBA8888 ? src : src.convertToFormat(QImage::Format_RGBA8888);
    QtClientImageView in{};
    in.pixels = const_cast<uint8_t*>(input.constBits());
    in.width = input.width();
    in.height = input.height();
    in.bytes_per_line = static_cast<int>(input.bytesPerLine());
    in.format = QTCLIENT_IMAGE_FORMAT_RGBA8888;
    QtClientImageView out{};
    const int ok = g_abiHdScale(&in, &out, g_abiHdScaleUser);
    QImage result;
    if (ok && out.pixels && out.width > 0 && out.height > 0 && out.bytes_per_line > 0) {
        const QImage::Format format = out.format == QTCLIENT_IMAGE_FORMAT_ARGB32_PREMULTIPLIED ? QImage::Format_ARGB32_Premultiplied
                                                                                              : QImage::Format_RGBA8888;
        result = QImage(out.pixels, out.width, out.height, out.bytes_per_line, format).copy();
        if (format != QImage::Format_RGBA8888)
            result = result.convertToFormat(QImage::Format_RGBA8888);
    }
    std::free(out.pixels);
    return result;
}

void abiSetHdScaler(int factor, QtClientHdScaleFn fn, void* user)
{
    g_abiHdScale = fn;
    g_abiHdScaleUser = fn ? user : nullptr;
    if (!fn) {
        tibia::sprites::setHdScaler(1, nullptr);
        return;
    }
    tibia::sprites::setHdScaler(std::max(1, factor), &tipHdScale);
}

void abiHostOs(char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return;
    const std::string os = tibia::host::app().getOs();
    std::snprintf(buf, buf_len, "%s", os.c_str());
}

const QtClientHostApiV1 g_hostApi = {
    .api_version = QTCLIENT_HOST_API_V1_VERSION,
    .host_build_id = qtclient_host_build_id,
    .struct_size = sizeof(QtClientHostApiV1),
    .qt_version = QT_VERSION_STR,
    .register_mod = &abiRegisterMod,
    .register_early_bootstrap = &abiRegisterEarly,
    .set_apply_image_tint_hook = &abiSetApplyImageTint,
    .set_login_sync_enqueue_hook = &abiSetLoginSync,
    .set_world_lighting_driver = &abiSetLighting,
    .add_world_overlay_factory = &abiAddOverlay,
    .set_world_frame_hooks = &abiSetFrameHooks,
    .set_hd_scaler = &abiSetHdScaler,
    .host_os = &abiHostOs,
    .session = tibia::mods::sessionApiV1(),
    .notify = tibia::mods::notifyApiV1(),
    .options = tibia::mods::optionsApiV1(),
    .pack_host = tibia::mods::packHostAbiTable(),
    .set_walk_unreachable_hook = &tibia::mods::setWalkUnreachableHook,
    .set_effect_remap_hook = &abiSetEffectRemap,
    .set_creature_decoration_hook = &abiSetCreatureDecoration,
    .world_field_stack = &tibia::qmlcomponents::worldFieldStack,
    .set_texture_atlas_hint = &tibia::mods::setTextureAtlasHint,
    .sound = tibia::sound::soundApiV1(),
    .register_button_bar_shortcut = &abiRegisterButtonBarShortcut,
    .register_hotkey_action = &abiRegisterHotkeyAction,
    .game_host = tibia::mods::gameHostApiV1(),
    .opcode = tibia::mods::opcodeApiV1(),
    .dialog = tibia::mods::dialogApiV1(),
    .action = tibia::mods::actionApiV1(),
    .chat = tibia::mods::chatApiV1(),
    .cooldown = tibia::mods::cooldownApiV1(),
    .game_data = tibia::mods::gameDataApiV1(),
    .ui = tibia::mods::uiApiV1(),
    .backends = tibia::mods::backendApiV1(),
    .cip_object = tibia::mods::cipObjectApiV1(),
};

} // namespace

extern "C" const QtClientHostApiV1* qtclient_host_api_v1(void)
{
    // The qt_version entry is the running Qt (qVersion()), not the one the host was built
    // with: a MODULE compares it with its own QT_VERSION_STR.
    static const QtClientHostApiV1 api = [] {
        QtClientHostApiV1 a = g_hostApi;
        a.qt_version = qVersion();
        return a;
    }();
    return &api;
}
