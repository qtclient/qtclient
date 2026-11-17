// CIP input::TKeyboardStatusStorage: only tracked keys count, the last press
// wins, any release of a held key empties the status.
#include "input/keyboardstatusstorage.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

using tibia::input::TKeyboardStatusStorage;
using TKey = TKeyboardStatusStorage::TKey;

namespace {

TKeyboardStatusStorage* trackedStorage()
{
    auto* storage = new TKeyboardStatusStorage;
    storage->setTrackedKeys({ { Qt::Key_Up, false }, { Qt::Key_Left, false }, { Qt::Key_Home, true }, { Qt::Key_Control, false } });
    return storage;
}

}

TEST(KeyboardStatusStorage, IgnoresUntrackedKeys)
{
    std::unique_ptr<TKeyboardStatusStorage> storage(trackedStorage());
    QSignalSpy changed(storage.get(), &TKeyboardStatusStorage::changed);
    storage->keyPressed({ Qt::Key_A, false }, Qt::NoModifier);
    EXPECT_FALSE(storage->isPressed({ Qt::Key_A, false }));
    EXPECT_EQ(changed.count(), 0);
}

TEST(KeyboardStatusStorage, LastPressWinsAndAddsHeldModifiers)
{
    std::unique_ptr<TKeyboardStatusStorage> storage(trackedStorage());
    storage->keyPressed({ Qt::Key_Up, false }, Qt::NoModifier);
    storage->keyPressed({ Qt::Key_Left, false }, Qt::ControlModifier | Qt::ShiftModifier);
    EXPECT_FALSE(storage->isPressed({ Qt::Key_Up, false }));
    EXPECT_TRUE(storage->isPressed({ Qt::Key_Left, false }));
    EXPECT_TRUE(storage->isPressed({ Qt::Key_Control, false }));
    EXPECT_TRUE(storage->isPressed({ Qt::Key_Shift, false }));
    const auto recent = storage->mostRecentlyPressed({ { Qt::Key_Up, false }, { Qt::Key_Left, false } });
    ASSERT_TRUE(recent.has_value());
    EXPECT_EQ(recent->key, Qt::Key_Left);
}

TEST(KeyboardStatusStorage, ModifierPressHoldsOnlyModifiers)
{
    std::unique_ptr<TKeyboardStatusStorage> storage(trackedStorage());
    storage->keyPressed({ Qt::Key_Control, false }, Qt::ControlModifier);
    EXPECT_TRUE(storage->isPressed({ Qt::Key_Control, false }));
    EXPECT_FALSE(storage->mostRecentlyPressed({ { Qt::Key_Up, false } }).has_value());
}

TEST(KeyboardStatusStorage, ReleaseOfAHeldKeyEmptiesTheStatus)
{
    std::unique_ptr<TKeyboardStatusStorage> storage(trackedStorage());
    storage->keyPressed({ Qt::Key_Left, false }, Qt::ControlModifier);
    QSignalSpy changed(storage.get(), &TKeyboardStatusStorage::changed);
    storage->keyReleased({ Qt::Key_Control, false });
    EXPECT_FALSE(storage->isPressed({ Qt::Key_Left, false }));
    EXPECT_EQ(changed.count(), 1);
}

TEST(KeyboardStatusStorage, ReleasingKeypadKeysKeepsTheOthers)
{
    std::unique_ptr<TKeyboardStatusStorage> storage(trackedStorage());
    storage->keyPressed({ Qt::Key_Home, true }, Qt::ControlModifier);
    storage->releaseKeys(true);
    EXPECT_FALSE(storage->isPressed({ Qt::Key_Home, true }));
    EXPECT_TRUE(storage->isPressed({ Qt::Key_Control, false }));
}

TEST(KeyboardStatusStorage, InactiveApplicationForgetsAndIgnoresPresses)
{
    std::unique_ptr<TKeyboardStatusStorage> storage(trackedStorage());
    storage->keyPressed({ Qt::Key_Up, false }, Qt::NoModifier);
    storage->onApplicationStateChanged(Qt::ApplicationInactive);
    EXPECT_FALSE(storage->isPressed({ Qt::Key_Up, false }));
    storage->keyPressed({ Qt::Key_Up, false }, Qt::NoModifier);
    EXPECT_FALSE(storage->isPressed({ Qt::Key_Up, false }));
    storage->onApplicationStateChanged(Qt::ApplicationActive);
    storage->keyPressed({ Qt::Key_Up, false }, Qt::NoModifier);
    EXPECT_TRUE(storage->isPressed({ Qt::Key_Up, false }));
}

TEST(KeyboardStatusStorage, NewTrackedKeysForgetTheHeldKeys)
{
    std::unique_ptr<TKeyboardStatusStorage> storage(trackedStorage());
    storage->keyPressed({ Qt::Key_Up, false }, Qt::NoModifier);
    storage->setTrackedKeys({ { Qt::Key_Up, false } });
    EXPECT_FALSE(storage->isPressed({ Qt::Key_Up, false }));
}
