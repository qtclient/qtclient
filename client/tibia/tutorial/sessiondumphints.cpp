#include "tutorial/sessiondumphints.h"

#include "chat/chatchannel.h"
#include "shared/jsonvalidator.h"
#include "tutorial/sessiondumphintpublishgameaction.h"

#include <QJsonArray>

#include <algorithm>
#include <stdexcept>

namespace tibia::tutorial {

namespace {

constexpr const char* InvalidDocument = "Invalid sessiondump hints json document";
constexpr const char* InvalidHint = "Invalid sessiondump hint json object";

/// The server log's channel id in hints files.
constexpr int ServerLogChannelId = 0x1fffe;

/// An outfit colour index as the outfit palette keeps it (134 entries).
uint8_t outfitColor(const int index) { return uint8_t(index >= 0 && index < 134 ? index : 0); }

}

TSessiondumpHints::TSessiondumpHints(const QJsonDocument& document)
{
    shared::TJsonObjectValidator validator(document.object());
    if (!validator.value(QStringLiteral("outfit")).isObject().isValid())
        throw std::invalid_argument(InvalidDocument);
    if (!validator.value(QStringLiteral("outfit")).value(QStringLiteral("male")).isObject().isValid())
        throw std::invalid_argument(InvalidDocument);
    if (!validator.value(QStringLiteral("outfit")).value(QStringLiteral("female")).isObject().isValid())
        throw std::invalid_argument(InvalidDocument);
    if (!validator.value(QStringLiteral("actions")).isObject().isValid())
        throw std::invalid_argument(InvalidDocument);

    const QJsonObject root = document.object();
    m_maleOutfit = parseOutfit(root[QStringLiteral("outfit")].toObject()[QStringLiteral("male")].toObject());
    m_femaleOutfit = parseOutfit(root[QStringLiteral("outfit")].toObject()[QStringLiteral("female")].toObject());

    const QJsonObject actions = root[QStringLiteral("actions")].toObject();
    for (const QString& key : actions.keys()) {
        bool ok = false;
        const int64_t timestamp = key.toUInt(&ok, 10);
        if (!ok)
            throw std::invalid_argument("Invalid timeststamp in sessiondump hints json document");
        shared::TJsonObjectValidator actionsValidator(actions);
        if (!actionsValidator.value(key).isArray().isValid())
            throw std::invalid_argument(InvalidDocument);
        const QJsonArray list = actions[key].toArray();
        for (const QJsonValue& entry : list) {
            if (entry.type() != QJsonValue::Object)
                throw std::invalid_argument(InvalidDocument);
            const THintPtr hint = createHint(entry.toObject(), timestamp);
            if (hint) {
                m_hints.push_back(hint);
                sortHints();
            }
        }
    }
}

TSessiondumpHints::~TSessiondumpHints() = default;

TSessiondumpOutfit TSessiondumpHints::outfit(const bool male) const { return male ? m_maleOutfit : m_femaleOutfit; }

std::vector<TSessiondumpHints::THintPtr> TSessiondumpHints::hints() const { return m_hints; }

void TSessiondumpHints::addHint(const THintPtr& hint)
{
    m_hints.push_back(hint);
    sortHints();
}

TSessiondumpHints::THintPtr TSessiondumpHints::firstUnfinishedHint() const
{
    for (const THintPtr& hint : m_hints) {
        if (!hint->isDone())
            return hint;
    }
    return nullptr;
}

void TSessiondumpHints::skipHintsBefore(const int64_t& timestamp)
{
    for (THintPtr hint = firstUnfinishedHint(); hint && hint->timestamp() < timestamp; hint = firstUnfinishedHint())
        hint->setDone();
}

void TSessiondumpHints::sortHints()
{
    std::stable_sort(m_hints.begin(), m_hints.end(),
                     [](const THintPtr& left, const THintPtr& right) { return left->timestamp() < right->timestamp(); });
}

TSessiondumpOutfit TSessiondumpHints::parseOutfit(const QJsonObject& object)
{
    shared::TJsonObjectValidator validator(object);
    for (const char* key : { "id", "head", "torso", "legs", "detail", "addons" }) {
        if (!validator.value(QString::fromLatin1(key)).isInt().isValid())
            throw std::invalid_argument(InvalidDocument);
    }
    TSessiondumpOutfit outfit;
    outfit.lookType = uint16_t(object[QStringLiteral("id")].toInt());
    outfit.head = outfitColor(object[QStringLiteral("head")].toInt());
    outfit.torso = outfitColor(object[QStringLiteral("torso")].toInt());
    outfit.legs = outfitColor(object[QStringLiteral("legs")].toInt());
    outfit.detail = outfitColor(object[QStringLiteral("detail")].toInt());
    const int addons = object[QStringLiteral("addons")].toInt();
    outfit.addon1 = (addons & 1) != 0;
    outfit.addon2 = ((addons >> 1) & 1) != 0;
    return outfit;
}

TSessiondumpHints::THintPtr TSessiondumpHints::createHint(const QJsonObject& object, const int64_t& timestamp)
{
    shared::TJsonObjectValidator validator(object);
    if (!validator.value(QStringLiteral("type")).isString().isValid())
        throw std::invalid_argument(InvalidHint);
    THintPtr hint;
    const QString type = object[QStringLiteral("type")].toString();
    if (type == QLatin1String("SKIP_TO"))
        hint = createSkipToHint(object);
    if (type == QLatin1String("SWITCH_CHANNEL"))
        hint = createSwitchChannelHint(object);
    else if (type == QLatin1String("CONDITION"))
        hint = createConditionHint(object);
    else if (type == QLatin1String("TUTORIAL_PROGRESS"))
        hint = createTutorialProgressHint(object);
    else if (type == QLatin1String("CLEAR_OLDEST_MESSAGE"))
        hint = std::make_shared<TSessiondumpHintClearOldestMessage>();
    if (hint)
        hint->setTimestamp(timestamp);
    return hint;
}

TSessiondumpHints::THintPtr TSessiondumpHints::createSkipToHint(const QJsonObject& object)
{
    shared::TJsonObjectValidator validator(object);
    if (!validator.value(QStringLiteral("skiptotimestamp")).isInt().isValid())
        throw std::invalid_argument(InvalidHint);
    const int64_t skipTo = object[QStringLiteral("skiptotimestamp")].toInt();
    return std::make_shared<TSessiondumpHintSkipTo>(skipTo);
}

TSessiondumpHints::THintPtr TSessiondumpHints::createSwitchChannelHint(const QJsonObject& object)
{
    shared::TJsonObjectValidator validator(object);
    if (!validator.value(QStringLiteral("channelid")).isInt().isValid())
        throw std::invalid_argument(InvalidHint);
    const int channelId = object[QStringLiteral("channelid")].toInt();
    int channelType = chat::TChatChannelIdentifier::Local;
    if (channelId == ServerLogChannelId)
        channelType = chat::TChatChannelIdentifier::ServerLog;
    return std::make_shared<TSessiondumpHintSwitchChannel>(channelType);
}

TSessiondumpHints::THintPtr TSessiondumpHints::createConditionHint(const QJsonObject&)
{
    // Conditions (pause the replay until the player does what the hint shows)
    // build CipSoft game action contexts and GUI tutorial markers, which
    // qtclient does not model yet; they are left out.
    return nullptr;
}

TSessiondumpHints::THintPtr TSessiondumpHints::createTutorialProgressHint(const QJsonObject& object)
{
    shared::TJsonObjectValidator validator(object);
    if (!validator.value(QStringLiteral("tutorialprogress")).isString().isValid())
        throw std::invalid_argument(InvalidHint);
    return std::make_shared<TSessiondumpHintTutorialProgress>(object[QStringLiteral("tutorialprogress")].toString());
}

}
