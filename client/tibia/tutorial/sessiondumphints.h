#pragma once

#include "tutorial/sessiondumphintbase.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cstdint>
#include <memory>
#include <vector>

namespace tibia {
namespace tutorial {

/// The look of the tutorial character in a hints file.
struct TSessiondumpOutfit
{
    uint16_t lookType = 0;
    uint8_t head = 0;
    uint8_t torso = 0;
    uint8_t legs = 0;
    uint8_t detail = 0;
    bool addon1 = false;
    bool addon2 = false;
};

/// CIP `tutorial::TSessiondumpHints`: the hints of a tutorial dump (the JSON
/// next to it), sorted by timestamp. Malformed files throw std::invalid_argument.
class TSessiondumpHints
{
public:
    using THintPtr = std::shared_ptr<TSessiondumpHintBase>;

    explicit TSessiondumpHints(const QJsonDocument& document);
    virtual ~TSessiondumpHints();

    /// The outfit of the male or the female tutorial character.
    TSessiondumpOutfit outfit(bool male) const;
    virtual std::vector<THintPtr> hints() const;
    /// Adds a hint, keeping the hints sorted by timestamp (stable).
    virtual void addHint(const THintPtr& hint);
    virtual THintPtr firstUnfinishedHint() const;
    /// Marks every unfinished hint before `timestamp` done.
    virtual void skipHintsBefore(const int64_t& timestamp);

    /// One hint from its JSON object; null for a type it does not know.
    static THintPtr createHint(const QJsonObject& object, const int64_t& timestamp);

private:
    static TSessiondumpOutfit parseOutfit(const QJsonObject& object);
    static THintPtr createSkipToHint(const QJsonObject& object);
    static THintPtr createSwitchChannelHint(const QJsonObject& object);
    static THintPtr createConditionHint(const QJsonObject& object);
    static THintPtr createTutorialProgressHint(const QJsonObject& object);
    void sortHints();

    TSessiondumpOutfit m_maleOutfit;
    TSessiondumpOutfit m_femaleOutfit;
    std::vector<THintPtr> m_hints;
};

}
}
