#include "hotkeyoptions.h"

#include "app/clientoptions.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

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

/// Binds keys in one chat mode: CIP keeps a map keyed by the key sequence, so a
/// key bound again replaces its setting and the bindings stay in key order.
void insertBinding(std::vector<THotkeyBinding>& bindings, const QKeySequence& keys, const TActionSetting& setting)
{
    const auto it = std::lower_bound(bindings.begin(), bindings.end(), keys,
                                     [](const THotkeyBinding& binding, const QKeySequence& k) { return binding.keys < k; });
    if (it != bindings.end() && !(keys < it->keys))
        it->setting = setting;
    else
        bindings.insert(it, { keys, setting });
}

/// One chat mode's bindings: entries that are not objects, or lack a string
/// keysequence or an object actionsetting, are skipped.
std::vector<THotkeyBinding> readBindings(const QJsonArray& array)
{
    std::vector<THotkeyBinding> bindings;
    for (const QJsonValue& value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        if (!object.value(QLatin1String("keysequence")).isString() || !object.value(QLatin1String("actionsetting")).isObject())
            continue;
        insertBinding(bindings, QKeySequence(object.value(QLatin1String("keysequence")).toString()),
                      TActionSetting::fromJson(object.value(QLatin1String("actionsetting")).toObject()));
    }
    return bindings;
}

/// Bindings with an empty setting are not written.
QJsonArray writeBindings(const std::vector<THotkeyBinding>& bindings)
{
    QJsonArray array;
    for (const THotkeyBinding& binding : bindings) {
        if (binding.setting.isEmpty())
            continue;
        array.append(QJsonObject{
            { QStringLiteral("keysequence"), binding.keys.toString(QKeySequence::PortableText) },
            { QStringLiteral("actionsetting"), binding.setting.toJson() },
        });
    }
    return array;
}

/// actionBarOptions.mappings: entries need a numeric actionBar and actionButton
/// and an object actionsetting.
std::map<std::pair<int, int>, TActionSetting> readMappings(const QJsonArray& array)
{
    std::map<std::pair<int, int>, TActionSetting> mappings;
    for (const QJsonValue& value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject mapping = value.toObject();
        if (!mapping.value(QLatin1String("actionBar")).isDouble() || !mapping.value(QLatin1String("actionButton")).isDouble()
            || !mapping.value(QLatin1String("actionsetting")).isObject())
            continue;
        const int bar = quint8(mapping.value(QLatin1String("actionBar")).toInt());
        const int button = quint8(mapping.value(QLatin1String("actionButton")).toInt());
        mappings[{ bar, button }] = TActionSetting::fromJson(mapping.value(QLatin1String("actionsetting")).toObject());
    }
    return mappings;
}

/// Mappings with an empty setting are not written.
QJsonArray writeMappings(const std::map<std::pair<int, int>, TActionSetting>& mappings)
{
    QJsonArray array;
    for (const auto& [key, setting] : mappings) {
        if (setting.isEmpty())
            continue;
        array.append(QJsonObject{
            { QStringLiteral("actionBar"), key.first },
            { QStringLiteral("actionButton"), key.second },
            { QStringLiteral("actionsetting"), setting.toJson() },
        });
    }
    return array;
}

THotkeySet readSet(const QJsonObject& object)
{
    THotkeySet set;
    set.chatOff = readBindings(object.value(QLatin1String("chatOff")).toArray());
    set.chatOn = readBindings(object.value(QLatin1String("chatOn")).toArray());
    if (object.value(QLatin1String("actionBarOptions")).isObject()) {
        QJsonObject actionBar = object.value(QLatin1String("actionBarOptions")).toObject();
        if (actionBar.value(QLatin1String("mappings")).isArray())
            set.mappings = readMappings(actionBar.value(QLatin1String("mappings")).toArray());
        actionBar.remove(QLatin1String("mappings"));
        set.actionBarExtra = actionBar;
    }
    return set;
}

QJsonObject writeSet(const THotkeySet& set)
{
    QJsonObject actionBar = set.actionBarExtra;
    actionBar.insert(QStringLiteral("mappings"), writeMappings(set.mappings));
    return {
        { QStringLiteral("chatOff"), writeBindings(set.chatOff) },
        { QStringLiteral("chatOn"), writeBindings(set.chatOn) },
        { QStringLiteral("actionBarOptions"), actionBar },
    };
}

}

THotkeyOptionsData THotkeyOptionsData::fromJson(const QJsonObject& section)
{
    THotkeyOptionsData data;
    const QJsonObject sets = section.value(QLatin1String("hotkeySets")).toObject();
    for (auto it = sets.begin(); it != sets.end(); ++it)
        data.hotkeySets.emplace(it.key(), readSet(it->toObject()));
    data.currentHotkeySetName = section.value(QLatin1String("currentHotkeySetName")).toString();
    if (!data.hotkeySets.contains(data.currentHotkeySetName))
        data.currentHotkeySetName = data.hotkeySets.empty() ? QString() : data.hotkeySets.begin()->first;
    data.autoSwitchHotkeyPreset = section.value(QLatin1String("autoSwitchHotkeyPreset")).toBool();
    data.showStaticHotkeysFirst = section.value(QLatin1String("showStaticHotkeysFirst")).toBool(true);
    return data;
}

QJsonObject THotkeyOptionsData::toJson() const
{
    QJsonObject sets;
    for (const auto& [name, set] : hotkeySets)
        sets.insert(name, writeSet(set));
    return {
        { QStringLiteral("autoSwitchHotkeyPreset"), autoSwitchHotkeyPreset },
        { QStringLiteral("currentHotkeySetName"), currentHotkeySetName },
        { QStringLiteral("hotkeySets"), sets },
        { QStringLiteral("showStaticHotkeysFirst"), showStaticHotkeysFirst },
    };
}

THotkeySet* THotkeyOptionsData::set(const QString& name)
{
    const auto it = hotkeySets.find(name);
    return it == hotkeySets.end() ? nullptr : &it->second;
}

void THotkeyOptionsData::setCurrentSet(const QString& name)
{
    if (hotkeySets.contains(name))
        currentHotkeySetName = name;
}

QStringList THotkeyOptionsData::hotkeySetNames() const
{
    QStringList names;
    for (const auto& [name, set] : hotkeySets)
        names << name;
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return QString::compare(a, b, Qt::CaseSensitive) < 0; });
    return names;
}

int THotkeyOptionsData::currentHotkeySetIndex() const
{
    return int(hotkeySetNames().indexOf(currentHotkeySetName));
}

void THotkeyOptionsData::setCurrentHotkeySetIndex(const int index)
{
    const QStringList names = hotkeySetNames();
    if (index >= 0 && index < names.size())
        setCurrentSet(names.at(index));
}

void THotkeyOptionsData::addSet(const QString& name, const THotkeySet& set)
{
    hotkeySets.emplace(name, set);
}

void THotkeyOptionsData::renameSet(const QString& from, const QString& to)
{
    auto node = hotkeySets.extract(from);
    if (node.empty() || hotkeySets.contains(to)) {
        if (!node.empty())
            hotkeySets.insert(std::move(node));
        return;
    }
    node.key() = to;
    hotkeySets.insert(std::move(node));
    if (currentHotkeySetName == from)
        currentHotkeySetName = to;
}

void THotkeyOptionsData::removeSet(const QString& name)
{
    hotkeySets.erase(name);
    if (currentHotkeySetName == name)
        currentHotkeySetName.clear();
}

void THotkeyOptionsData::clearBindings(const QString& setName, const bool chatOn)
{
    // An unknown name gets an empty set (CipSoft indexes the set map).
    THotkeySet& hotkeySet = hotkeySets[setName];
    (chatOn ? hotkeySet.chatOn : hotkeySet.chatOff).clear();
}

void THotkeyOptionsData::addBinding(const QString& setName, const bool chatOn, const QKeySequence& keys, const TActionSetting& setting)
{
    if (THotkeySet* hotkeySet = set(setName))
        insertBinding(chatOn ? hotkeySet->chatOn : hotkeySet->chatOff, keys, setting);
}

void THotkeyOptionsData::setBindings(const QString& setName, const bool chatOn, const std::vector<THotkeyBinding>& bindings)
{
    if (THotkeySet* hotkeySet = set(setName))
        (chatOn ? hotkeySet->chatOn : hotkeySet->chatOff) = bindings;
}

void THotkeyOptionsData::addGlobalShortcuts(const bool rotateWithCtrl, const bool rotateWithShift, const bool rotateWithAlt)
{
    static const QStringList movementActions = {
        QStringLiteral("GoNorth"), QStringLiteral("GoWest"), QStringLiteral("GoSouth"), QStringLiteral("GoEast"),
    };
    static const char* const arrowKeys[] = { "Left", "Right", "Up", "Down" };
    const std::pair<bool, QLatin1String> rotations[] = {
        { rotateWithCtrl, QLatin1String("Ctrl+") },
        { rotateWithShift, QLatin1String("Shift+") },
        { rotateWithAlt, QLatin1String("Alt+") },
    };
    const auto addRotations = [&](std::set<QKeySequence>& keys, const QString& portable) {
        for (const auto& [enabled, modifier] : rotations)
            if (enabled)
                keys.insert(QKeySequence(modifier + portable));
    };

    if (hotkeySets.empty()) {
        for (const QString& vocation : THotkeyOptions::vocationSetNames())
            hotkeySets.emplace(vocation, THotkeyOptions::builtInSet(vocation));
        currentHotkeySetName = QStringLiteral("Knight");
        autoSwitchHotkeyPreset = true;
        return;
    }

    const THotkeySet global = THotkeyOptions::builtInSet(QStringLiteral("GlobalShortcuts"));
    for (auto& [name, set] : hotkeySets) {
        for (const bool chatOn : { false, true }) {
            std::vector<THotkeyBinding>& bindings = chatOn ? set.chatOn : set.chatOff;
            std::set<QKeySequence> usedKeys;
            std::set<QString> usedActions;
            for (const THotkeyBinding& binding : bindings) {
                usedKeys.insert(binding.keys);
                const QString action = binding.setting.kind == TActionSetting::ACTION ? binding.setting.action : QString();
                usedActions.insert(action);
                if (movementActions.contains(action))
                    addRotations(usedKeys, binding.keys.toString(QKeySequence::PortableText));
            }
            for (const char* key : arrowKeys)
                addRotations(usedKeys, QLatin1String(key));
            for (const THotkeyBinding& binding : chatOn ? global.chatOn : global.chatOff) {
                if (binding.setting.kind != TActionSetting::ACTION)
                    continue;
                if (usedActions.contains(binding.setting.action) || usedKeys.contains(binding.keys))
                    continue;
                insertBinding(bindings, binding.keys, binding.setting);
            }
        }
    }
}

TActionSetting TActionSetting::fromJson(const QJsonObject& json)
{
    // The first key present decides the kind: chatText, action, useObject,
    // passiveAbility, multiActions. Anything else is an empty setting.
    TActionSetting setting;
    if (json.contains(QLatin1String("chatText"))) {
        setting.kind = TEXT;
        setting.text = json.value(QLatin1String("chatText")).toString();
        setting.sendAutomatically = json.value(QLatin1String("sendAutomatically")).toBool(false);
        setting.spellCastType = json.value(QLatin1String("spellCastType")).toInt();
        if (setting.text.size() >= MaxChatTextLength)
            throw std::runtime_error("Hotkey chat text is too long.");
    } else if (json.contains(QLatin1String("action"))) {
        setting.kind = ACTION;
        setting.action = json.value(QLatin1String("action")).toString();
    } else if (json.contains(QLatin1String("useObject"))) {
        setting.kind = OBJECT;
        const QJsonValue objectId = json.value(QLatin1String("useObject"));
        setting.objectId = objectId.isDouble() ? std::clamp(objectId.toInt(), 0, std::numeric_limits<int>::max()) : 0;
        // A liquid type wins over an upgrade tier; an object has at most one.
        if (json.contains(QLatin1String("liquidType")))
            setting.liquidType = json.value(QLatin1String("liquidType")).toInt();
        else if (json.contains(QLatin1String("upgradeTier")))
            setting.upgradeTier = json.value(QLatin1String("upgradeTier")).toInt();
        const qsizetype useType = useTypeNames().indexOf(json.value(QLatin1String("useType")).toString());
        if (useType >= 0)
            setting.useType = EUseType(useType);
        if (setting.useType == EUseType::Equip && json.contains(QLatin1String("useEquipSmartMode")))
            setting.useEquipSmartMode = json.value(QLatin1String("useEquipSmartMode")).toBool();
    } else if (json.contains(QLatin1String("passiveAbility"))) {
        setting.kind = PASSIVE_ABILITY;
        setting.passiveAbility = json.value(QLatin1String("passiveAbility")).toInt();
    } else if (json.contains(QLatin1String("multiActions"))) {
        // At most MaxMultiActions actions are read.
        const QJsonArray actions = json.value(QLatin1String("multiActions")).toArray();
        const qsizetype count = std::min<qsizetype>(actions.size(), MaxMultiActions);
        setting.setMultiActionCount(static_cast<int>(count));
        for (qsizetype i = 0; i < count; ++i)
            setting.setMultiAction(i, fromJson(actions.at(i).toObject()));
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

void TActionSetting::setChatText(const QString& chatText)
{
    text = chatText;
    if (text.isEmpty()) {
        if (kind == TEXT)
            kind = NONE;
        return;
    }
    kind = TEXT;
    objectId = 0;
    liquidType.reset();
    multiActions.clear();
}

void TActionSetting::setUseObject(const int objectClientId)
{
    objectId = objectClientId;
    liquidType.reset();
    upgradeTier.reset();
    passiveAbility = 0;
    multiActions.clear();
    if (objectId == 0) {
        if (kind == OBJECT || kind == PASSIVE_ABILITY)
            kind = NONE;
        return;
    }
    kind = OBJECT;
    text.clear();
}

void TActionSetting::setPassiveAbility(const int ability)
{
    passiveAbility = ability;
    if (ability == 0) {
        if (kind == PASSIVE_ABILITY)
            kind = NONE;
        return;
    }
    kind = PASSIVE_ABILITY;
    text.clear();
    objectId = 0;
    liquidType.reset();
    multiActions.clear();
}

void TActionSetting::setMultiActionCount(const int count)
{
    const TActionSetting previous = *this;
    text.clear();
    objectId = 0;
    liquidType.reset();
    passiveAbility = 0;
    multiActions.clear();
    for (int i = 0; i < count; ++i)
        multiActions.push_back(i == 0 && previous.passiveAbility == 0 ? previous : TActionSetting{});
    kind = count > 0 ? MULTI_ACTION : NONE;
}

void TActionSetting::setMultiAction(const qsizetype index, TActionSetting multiAction)
{
    if (multiActions.empty())
        throw std::runtime_error("TTibiaActionSetting::setMultiAction: Called for non multi action");
    multiActions[static_cast<size_t>(index)] = std::move(multiAction);
}

void TActionSetting::setUseObject(const int objectClientId, const int liquid)
{
    setUseObject(objectClientId);
    liquidType = liquid;
}

void TActionSetting::setUseObjectWithTier(const int objectClientId, const int tier)
{
    setUseObject(objectClientId);
    upgradeTier = tier;
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

namespace {

/// CIP's built-in hotkey sets: the embedded defaults read once, each vocation
/// preset then given the GlobalShortcuts bindings whose keys it does not bind.
const THotkeyOptionsData& builtInOptions()
{
    static const THotkeyOptionsData options = [] {
        QFile file(QStringLiteral(":/config/defaulthotkeys.json"));
        if (!file.open(QIODevice::ReadOnly))
            return THotkeyOptionsData{};
        THotkeyOptionsData data = THotkeyOptionsData::fromJson(QJsonDocument::fromJson(file.readAll()).object());
        const auto global = data.hotkeySets.find(QStringLiteral("GlobalShortcuts"));
        if (global == data.hotkeySets.end())
            return data;
        for (const QString& vocation : THotkeyOptions::vocationSetNames()) {
            THotkeySet* set = data.set(vocation);
            if (!set)
                continue;
            for (const bool chatOn : { false, true }) {
                std::vector<THotkeyBinding>& bindings = chatOn ? set->chatOn : set->chatOff;
                for (const THotkeyBinding& binding : chatOn ? global->second.chatOn : global->second.chatOff) {
                    const bool bound = std::any_of(bindings.begin(), bindings.end(),
                                                   [&](const THotkeyBinding& own) { return own.keys == binding.keys; });
                    if (!bound)
                        insertBinding(bindings, binding.keys, binding.setting);
                }
            }
        }
        return data;
    }();
    return options;
}

}

const QStringList& THotkeyOptions::vocationSetNames()
{
    static const QStringList names = {
        QStringLiteral("Knight"), QStringLiteral("Paladin"), QStringLiteral("Sorcerer"), QStringLiteral("Druid"), QStringLiteral("Monk"),
    };
    return names;
}

QJsonObject THotkeyOptions::builtInSets()
{
    return builtInOptions().toJson().value(QLatin1String("hotkeySets")).toObject();
}

QJsonObject THotkeyOptions::builtInSection()
{
    THotkeyOptionsData data;
    data.addGlobalShortcuts(false, false, false);
    return data.toJson();
}

THotkeySet THotkeyOptions::builtInSet(const QString& name)
{
    const auto it = builtInOptions().hotkeySets.find(name);
    return it == builtInOptions().hotkeySets.end() ? THotkeySet{} : it->second;
}

void THotkeyOptions::load()
{
    // TClientOptions' load already merged the GlobalShortcuts in.
    THotkeyOptionsData data;
    try {
        data = THotkeyOptionsData::fromJson(app::ClientOptions::load().root().value(QLatin1String("hotkeyOptions")).toObject());
    } catch (const std::runtime_error&) {
        data = {};
    }
    m_sets = std::move(data.hotkeySets);
    m_current = data.currentHotkeySetName;
    m_autoSwitch = data.autoSwitchHotkeyPreset;
    m_showStaticFirst = data.showStaticHotkeysFirst;
    m_loaded = true;
}

void THotkeyOptions::readFrom(const QJsonObject& section)
{
    THotkeyOptionsData data = THotkeyOptionsData::fromJson(section);
    m_sets = std::move(data.hotkeySets);
    m_current = data.currentHotkeySetName;
    m_autoSwitch = data.autoSwitchHotkeyPreset;
    m_showStaticFirst = data.showStaticHotkeysFirst;
}

QJsonObject THotkeyOptions::toJson() const
{
    return THotkeyOptionsData{ m_current, m_autoSwitch, m_showStaticFirst, m_sets }.toJson();
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
