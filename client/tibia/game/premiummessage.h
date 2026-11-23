#pragma once

#include <cip_gen.h>

#include <QString>

#include <cstdint>

namespace tibia {
namespace game {

/// CIP game::TPremiumMessage: one premium feature line of the premium panel,
/// its translated text and its icon (the 20x20 or the 32x32 strip cell of the
/// feature).
class TPremiumMessage : public cip::tibia::game::TPremiumMessage
{
    Q_OBJECT
public:
    /// `feature` picks the text (an unknown feature has none); `largeIcon`
    /// picks the 32x32 icon strip instead of the 20x20 one.
    TPremiumMessage(uint8_t feature, bool largeIcon, QObject* parent = nullptr);

    virtual uint8_t feature() const { return m_feature; }
    QString message() const override { return m_message; }
    QString imageUrl() const override { return m_imageUrl; }

    /// The translation id of a premium feature, null for an unknown one.
    static const char* messageId(uint8_t feature);
    static QString smallIconUrl(uint8_t feature);
    static QString largeIconUrl(uint8_t feature);

private:
    uint8_t m_feature = 0;
};

}
}
