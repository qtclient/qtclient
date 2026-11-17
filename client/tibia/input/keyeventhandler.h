#pragma once

#include <cip_gen.h>

#include <memory>

namespace tibia {
namespace input {

class TKeyboardStatusStorage;

/// The application key filter (installed by TEventFilterManager) that feeds
/// the key status: presses and non-repeated releases, the arrow keys without
/// their keypad flag. A Num Lock press forgets the held keypad keys. Never
/// consumes an event.
class TKeyEventHandler final : public cip::tibia::input::TKeyEventHandler
{
    Q_OBJECT
public:
    explicit TKeyEventHandler(std::shared_ptr<TKeyboardStatusStorage> pKeyboardStatusStorage);

    TKeyboardStatusStorage* keyboardStatusStorage() const { return m_pKeyboardStatusStorage.get(); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::shared_ptr<TKeyboardStatusStorage> m_pKeyboardStatusStorage;
};

}
}
