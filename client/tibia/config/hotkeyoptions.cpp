#include "hotkeyoptions.h"

#include "app/clientoptions.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace tibia::config {

namespace {

const QStringList& useTypeNames()
{
    static const QStringList names = {
        QStringLiteral("Use"),
        QStringLiteral("UseOnYourself"),
        QStringLiteral("UseOnTarget"),
        QStringLiteral("SelectUseTarget"),
        QStringLiteral("Equip"),
        QStringLiteral("UseAtMouseCursor"),
    };
    return names;
}

std::vector<THotkeyBinding> readBindings(const QJsonArray& array)
{
    std::vector<THotkeyBinding> bindings;
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        THotkeyBinding binding;
        binding.keys = QKeySequence::fromString(object.value(QLatin1String("keysequence")).toString(), QKeySequence::PortableText);
        binding.setting = TActionSetting::fromJson(object.value(QLatin1String("actionsetting")).toObject());
        if (!binding.keys.isEmpty() && !binding.setting.isEmpty())
            bindings.push_back(std::move(binding));
    }
    return bindings;
}

QJsonArray writeBindings(const std::vector<THotkeyBinding>& bindings)
{
    QJsonArray array;
    for (const THotkeyBinding& binding : bindings)
        array.append(QJsonObject{
            { QStringLiteral("actionsetting"), binding.setting.toJson() },
            { QStringLiteral("keysequence"), binding.keys.toString(QKeySequence::PortableText) },
        });
    return array;
}

THotkeySet readSet(const QJsonObject& object)
{
    THotkeySet set;
    QJsonObject actionBar = object.value(QLatin1String("actionBarOptions")).toObject();
    for (const QJsonValue& value : actionBar.value(QLatin1String("mappings")).toArray()) {
        const QJsonObject mapping = value.toObject();
        TActionSetting setting = TActionSetting::fromJson(mapping.value(QLatin1String("actionsetting")).toObject());
        if (!setting.isEmpty())
            set.mappings[{ mapping.value(QLatin1String("actionBar")).toInt(), mapping.value(QLatin1String("actionButton")).toInt() }] = std::move(setting);
    }
    actionBar.remove(QLatin1String("mappings"));
    set.actionBarExtra = actionBar;
    set.chatOff = readBindings(object.value(QLatin1String("chatOff")).toArray());
    set.chatOn = readBindings(object.value(QLatin1String("chatOn")).toArray());
    return set;
}

QJsonObject writeSet(const THotkeySet& set)
{
    QJsonArray mappings;
    for (const auto& [key, setting] : set.mappings)
        mappings.append(QJsonObject{
            { QStringLiteral("actionBar"), key.first },
            { QStringLiteral("actionButton"), key.second },
            { QStringLiteral("actionsetting"), setting.toJson() },
        });
    QJsonObject actionBar = set.actionBarExtra;
    actionBar.insert(QStringLiteral("mappings"), mappings);
    return {
        { QStringLiteral("actionBarOptions"), actionBar },
        { QStringLiteral("chatOff"), writeBindings(set.chatOff) },
        { QStringLiteral("chatOn"), writeBindings(set.chatOn) },
    };
}

/// CIP FUN_100e90fe0: the built-in sets, each with GlobalShortcuts' keys.
QJsonObject builtInSection()
{
    QFile file(QStringLiteral(":/config/defaulthotkeys.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonObject sets = QJsonDocument::fromJson(file.readAll()).object().value(QLatin1String("hotkeySets")).toObject();
    const QJsonObject global = sets.take(QLatin1String("GlobalShortcuts")).toObject();
    for (auto it = sets.begin(); it != sets.end(); ++it) {
        QJsonObject set = it->toObject();
        for (const QLatin1String mode : { QLatin1String("chatOff"), QLatin1String("chatOn") }) {
            QJsonArray keys = global.value(mode).toArray();
            for (const QJsonValue& value : set.value(mode).toArray())
                keys.append(value);
            set.insert(mode, keys);
        }
        *it = set;
    }
    return {
        { QStringLiteral("autoSwitchHotkeyPreset"), true },
        { QStringLiteral("currentHotkeySetName"), sets.isEmpty() ? QString() : sets.begin().key() },
        { QStringLiteral("hotkeySets"), sets },
        { QStringLiteral("showStaticHotkeysFirst"), true },
    };
}

}

TActionSetting TActionSetting::fromJson(const QJsonObject& json)
{
    TActionSetting setting;
    if (json.contains(QLatin1String("chatText"))) {
        setting.kind = TEXT;
        setting.text = json.value(QLatin1String("chatText")).toString();
        setting.sendAutomatically = json.value(QLatin1String("sendAutomatically")).toBool();
        setting.spellCastType = json.value(QLatin1String("spellCastType")).toInt();
    } else if (json.contains(QLatin1String("useObject"))) {
        setting.kind = OBJECT;
        setting.objectId = json.value(QLatin1String("useObject")).toInt();
        setting.useType = EUseType(std::max<qsizetype>(0, useTypeNames().indexOf(json.value(QLatin1String("useType")).toString())));
        if (json.contains(QLatin1String("upgradeTier")))
            setting.upgradeTier = json.value(QLatin1String("upgradeTier")).toInt();
        if (json.contains(QLatin1String("liquidType")))
            setting.liquidType = json.value(QLatin1String("liquidType")).toInt();
        setting.useEquipSmartMode = json.value(QLatin1String("useEquipSmartMode")).toBool();
    } else if (json.contains(QLatin1String("passiveAbility"))) {
        setting.kind = PASSIVE_ABILITY;
        setting.passiveAbility = json.value(QLatin1String("passiveAbility")).toInt();
    } else if (json.contains(QLatin1String("multiActions"))) {
        setting.kind = MULTI_ACTION;
        for (const QJsonValue& value : json.value(QLatin1String("multiActions")).toArray())
            setting.multiActions.push_back(fromJson(value.toObject()));
    } else if (json.contains(QLatin1String("action"))) {
        setting.kind = ACTION;
        setting.action = json.value(QLatin1String("action")).toString();
    }
    return setting;
}

QJsonObject TActionSetting::toJson() const
{
    QJsonObject json;
    switch (kind) {
        case TEXT:
            json.insert(QStringLiteral("chatText"), text);
            json.insert(QStringLiteral("sendAutomatically"), sendAutomatically);
            json.insert(QStringLiteral("spellCastType"), spellCastType);
            break;
        case OBJECT:
            json.insert(QStringLiteral("useObject"), objectId);
            json.insert(QStringLiteral("useType"), useTypeNames().value(int(useType)));
            if (upgradeTier)
                json.insert(QStringLiteral("upgradeTier"), *upgradeTier);
            if (liquidType)
                json.insert(QStringLiteral("liquidType"), *liquidType);
            if (useType == EUseType::Equip)
                json.insert(QStringLiteral("useEquipSmartMode"), useEquipSmartMode);
            break;
        case PASSIVE_ABILITY:
            json.insert(QStringLiteral("passiveAbility"), passiveAbility);
            break;
        case MULTI_ACTION: {
            QJsonArray actions;
            for (const TActionSetting& action : multiActions)
                actions.append(action.toJson());
            json.insert(QStringLiteral("multiActions"), actions);
            break;
        }
        case ACTION:
            json.insert(QStringLiteral("action"), action);
            break;
        case NONE:
            break;
    }
    return json;
}

bool TActionSetting::operator==(const TActionSetting& other) const
{
    return toJson() == other.toJson();
}

THotkeyOptions* THotkeyOptions::instance()
{
    static THotkeyOptions* options = [] {
        auto* o = new THotkeyOptions;
        o->load();
        return o;
    }();
    return options;
}

void THotkeyOptions::load()
{
    QJsonObject section = app::ClientOptions::load().root().value(QLatin1String("hotkeyOptions")).toObject();
    if (section.value(QLatin1String("hotkeySets")).toObject().isEmpty())
        section = builtInSection();
    readFrom(section);
    m_loaded = true;
}

void THotkeyOptions::readFrom(const QJsonObject& section)
{
    m_sets.clear();
    const QJsonObject sets = section.value(QLatin1String("hotkeySets")).toObject();
    for (auto it = sets.begin(); it != sets.end(); ++it)
        m_sets.emplace(it.key(), readSet(it->toObject()));
    m_current = section.value(QLatin1String("currentHotkeySetName")).toString();
    if (!m_sets.contains(m_current))
        m_current = m_sets.empty() ? QString() : m_sets.begin()->first;
    m_autoSwitch = section.value(QLatin1String("autoSwitchHotkeyPreset")).toBool();
    m_showStaticFirst = section.value(QLatin1String("showStaticHotkeysFirst")).toBool(true);
}

QJsonObject THotkeyOptions::toJson() const
{
    QJsonObject sets;
    for (const auto& [name, set] : m_sets)
        sets.insert(name, writeSet(set));
    return {
        { QStringLiteral("autoSwitchHotkeyPreset"), m_autoSwitch },
        { QStringLiteral("currentHotkeySetName"), m_current },
        { QStringLiteral("hotkeySets"), sets },
        { QStringLiteral("showStaticHotkeysFirst"), m_showStaticFirst },
    };
}

void THotkeyOptions::save()
{
    app::ClientOptions::storeAll({ { QStringLiteral("hotkeyOptions"), toJson() } });
}

void THotkeyOptions::replaceAll(const QJsonObject& section)
{
    readFrom(section);
    save();
    Q_EMIT currentSetChanged();
    Q_EMIT mappingsChanged();
    Q_EMIT keysChanged();
}

void THotkeyOptions::setAutoSwitchHotkeyPreset(const bool enabled)
{
    if (m_autoSwitch == enabled)
        return;
    m_autoSwitch = enabled;
    save();
}

void THotkeyOptions::setShowStaticHotkeysFirst(const bool enabled)
{
    if (m_showStaticFirst == enabled)
        return;
    m_showStaticFirst = enabled;
    save();
}

QStringList THotkeyOptions::setNames() const
{
    QStringList names;
    for (const auto& [name, set] : m_sets)
        names << name;
    return names;
}

THotkeySet& THotkeyOptions::currentSet()
{
    return m_sets[m_current];
}

const THotkeySet* THotkeyOptions::set(const QString& name) const
{
    const auto it = m_sets.find(name);
    return it == m_sets.end() ? nullptr : &it->second;
}

TActionSetting THotkeyOptions::mapping(const int actionBarId, const int buttonId) const
{
    const THotkeySet* current = set(m_current);
    if (!current)
        return {};
    const auto it = current->mappings.find({ actionBarId, buttonId });
    return it == current->mappings.end() ? TActionSetting{} : it->second;
}

void THotkeyOptions::setMapping(const int actionBarId, const int buttonId, const TActionSetting& setting)
{
    auto& mappings = currentSet().mappings;
    if (setting.isEmpty())
        mappings.erase({ actionBarId, buttonId });
    else
        mappings[{ actionBarId, buttonId }] = setting;
    save();
    Q_EMIT mappingsChanged();
}

int THotkeyOptions::firstVisibleButton(const int actionBarId) const
{
    const auto it = m_sets.find(m_current);
    if (it == m_sets.end())
        return 0;
    return it->second.actionBarExtra.value(QLatin1String("firstVisibleButtons")).toObject().value(QString::number(actionBarId)).toInt(0);
}

void THotkeyOptions::setFirstVisibleButton(const int actionBarId, const int index)
{
    if (firstVisibleButton(actionBarId) == index)
        return;
    QJsonObject& extra = currentSet().actionBarExtra;
    QJsonObject buttons = extra.value(QLatin1String("firstVisibleButtons")).toObject();
    buttons.insert(QString::number(actionBarId), index);
    extra.insert(QStringLiteral("firstVisibleButtons"), buttons);
    save();
}

QString THotkeyOptions::triggerActionButtonName(const int actionBarId, const int buttonId)
{
    return QStringLiteral("TriggerActionButton_%1.%2").arg(actionBarId).arg(buttonId);
}

QKeySequence THotkeyOptions::buttonKeys(const int actionBarId, const int buttonId, const bool chatOn) const
{
    const THotkeySet* current = set(m_current);
    if (!current)
        return {};
    const QString name = triggerActionButtonName(actionBarId, buttonId);
    for (const THotkeyBinding& binding : chatOn ? current->chatOn : current->chatOff)
        if (binding.setting.kind == TActionSetting::ACTION && binding.setting.action == name)
            return binding.keys;
    return {};
}

QList<QKeySequence> THotkeyOptions::allButtonKeys(const int actionBarId, const int buttonId, const bool chatOn) const
{
    QList<QKeySequence> keys;
    const THotkeySet* current = set(m_current);
    if (!current)
        return keys;
    const QString name = triggerActionButtonName(actionBarId, buttonId);
    for (const THotkeyBinding& binding : chatOn ? current->chatOn : current->chatOff)
        if (binding.setting.kind == TActionSetting::ACTION && binding.setting.action == name)
            keys.push_back(binding.keys);
    return keys;
}

void THotkeyOptions::setButtonKeys(const int actionBarId, const int buttonId, const bool chatOn, const QKeySequence& keys)
{
    QList<QKeySequence> list;
    if (!keys.isEmpty())
        list << keys;
    setActionKeys(triggerActionButtonName(actionBarId, buttonId), chatOn, list);
}

QList<QKeySequence> THotkeyOptions::actionKeys(const QString& action, const bool chatOn) const
{
    QList<QKeySequence> keys;
    const THotkeySet* current = set(m_current);
    if (!current || action.isEmpty())
        return keys;
    for (const THotkeyBinding& binding : chatOn ? current->chatOn : current->chatOff)
        if (binding.setting.kind == TActionSetting::ACTION && binding.setting.action == action)
            keys.push_back(binding.keys);
    return keys;
}

void THotkeyOptions::setActionKeys(const QString& action, const bool chatOn, const QList<QKeySequence>& keys)
{
    if (action.isEmpty())
        return;
    THotkeySet& current = currentSet();
    auto& bindings = chatOn ? current.chatOn : current.chatOff;
    std::erase_if(bindings, [&](const THotkeyBinding& binding) {
        if (binding.setting.kind == TActionSetting::ACTION && binding.setting.action == action)
            return true;
        for (const QKeySequence& key : keys)
            if (!key.isEmpty() && binding.keys == key)
                return true;
        return false;
    });
    for (const QKeySequence& key : keys) {
        if (key.isEmpty())
            continue;
        TActionSetting setting;
        setting.kind = TActionSetting::ACTION;
        setting.action = action;
        bindings.push_back({ key, setting });
    }
    save();
    Q_EMIT keysChanged();
}

const TActionSetting* THotkeyOptions::binding(const QKeySequence& keys, const bool chatOn) const
{
    const THotkeySet* current = set(m_current);
    if (!current)
        return nullptr;
    for (const THotkeyBinding& binding : chatOn ? current->chatOn : current->chatOff)
        if (binding.keys == keys)
            return &binding.setting;
    return nullptr;
}

void THotkeyOptions::switchSet(const int delta)
{
    if (m_sets.size() < 2)
        return;
    auto it = m_sets.find(m_current);
    if (it == m_sets.end())
        it = m_sets.begin();
    if (delta > 0) {
        if (++it == m_sets.end())
            it = m_sets.begin();
    } else {
        if (it == m_sets.begin())
            it = m_sets.end();
        --it;
    }
    selectSet(it->first);
}

void THotkeyOptions::selectSet(const QString& name)
{
    if (name == m_current || !m_sets.contains(name))
        return;
    m_current = name;
    save();
    Q_EMIT currentSetChanged();
    Q_EMIT mappingsChanged();
    Q_EMIT keysChanged();
}

void THotkeyOptions::onCharacterLoggedIn()
{
    if (!m_loaded)
        load();
}

QString THotkeyOptions::selfCheckRoundTrip()
{
    THotkeyOptions* hotkeys = instance();
    hotkeys->load();
    const QJsonObject snapshot = hotkeys->toJson();
    const auto restore = [hotkeys, &snapshot] { hotkeys->replaceAll(snapshot); };

    const QStringList names = hotkeys->setNames();
    if (names.size() < 2) {
        restore();
        return QStringLiteral("need at least two hotkey sets");
    }

    const QString first = names.at(0);
    const QString second = names.at(1);
    hotkeys->selectSet(first);

    TActionSetting probe;
    probe.kind = TActionSetting::TEXT;
    probe.text = QStringLiteral("exura selfcheck");
    probe.sendAutomatically = true;
    const TActionSetting previous = hotkeys->mapping(1, 1);
    hotkeys->setMapping(1, 1, probe);

    hotkeys->switchSet(1);
    if (hotkeys->currentSetName() != second) {
        restore();
        return QStringLiteral("switchSet did not select %1 (got %2)").arg(second, hotkeys->currentSetName());
    }

    hotkeys->selectSet(first);
    hotkeys->load();
    if (hotkeys->currentSetName() != first) {
        restore();
        return QStringLiteral("reload lost currentHotkeySetName (got %1)").arg(hotkeys->currentSetName());
    }

    const TActionSetting roundTrip = hotkeys->mapping(1, 1);
    if (roundTrip.kind != TActionSetting::TEXT || roundTrip.text != probe.text || !roundTrip.sendAutomatically) {
        restore();
        return QStringLiteral("reload lost action-bar mapping for bar 1 button 1");
    }

    // Mapping round-trip ok; put the prior button back before restoring the full snapshot.
    hotkeys->setMapping(1, 1, previous);
    restore();
    return {};
}

}
