#include <gtest/gtest.h>

#include "input/gameaction.h"
#include "input/gameactioncontext.h"

#include <memory>

namespace input = tibia::input;
using input::EGameAction;

namespace {

input::TGameActionContextPtr containerSlot()
{
    return std::make_shared<input::TLocationContainerWithSlotGameActionContext>(3, 7);
}

input::TObjectGameActionContext object()
{
    return { containerSlot(), std::make_shared<input::TObjectApperanceTypeGameActionContext>(3031) };
}

}

TEST(GameActionConstructors, BuyHirelingNameOrSexChangeOpensThePremiumPageAtDetailSeven)
{
    const input::TGameActionBuyHirelingNameOrSexChange action;
    EXPECT_EQ(action.gameActionType(), EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE);
    EXPECT_EQ(action.storeOpenParameters().openAction, 1);
    EXPECT_EQ(action.storeOpenParameters().detail, 7);
    EXPECT_TRUE(action.storeOpenParameters().text.isEmpty());
}

TEST(GameActionConstructors, InspectAndOpenParentContainerKeepOnlyTheLocation)
{
    const input::TGameActionContextPtr location = containerSlot();
    const input::TGameActionInspectObject inspect(location);
    EXPECT_EQ(inspect.gameActionType(), EGameAction::INSPECT_OBJECT);
    EXPECT_EQ(inspect.location(), location);
    const input::TGameActionOpenParentContainer parent(location);
    EXPECT_EQ(parent.gameActionType(), EGameAction::OPEN_PARENT_CONTAINER);
    EXPECT_EQ(parent.location(), location);
}

TEST(GameActionConstructors, MoveUpCountDefaultsToAllBitsSet)
{
    EXPECT_EQ(input::TGameActionMoveUp(object()).count(), 0xff);
    EXPECT_EQ(input::TGameActionMoveUp(object(), 4).count(), 4);
}

TEST(GameActionConstructors, StashStowsStartUnconfirmedAndCountInAByte)
{
    const input::TGameActionStashStowContainerContent content(object());
    EXPECT_FALSE(content.isStashConfirmed());
    const input::TGameActionStashStowStack stack(object(), 100);
    EXPECT_EQ(stack.amount(), 100);
}

TEST(GameActionConstructors, TargetedSpellsTakeTheirWordsAndTalkModeAfterConstruction)
{
    input::TGameActionCrossHairSpell spell(23);
    EXPECT_EQ(spell.gameActionType(), EGameAction::CROSSHAIR_SPELL);
    EXPECT_EQ(spell.spellId(), 23);
    EXPECT_TRUE(spell.words().isEmpty());
    EXPECT_EQ(spell.talkMode(), 0);
    EXPECT_FALSE(spell.target().has_value());
    spell.setWords(QStringLiteral("exori vis"));
    spell.setTalkMode(1);
    EXPECT_EQ(spell.words(), QStringLiteral("exori vis"));
    EXPECT_EQ(spell.talkMode(), 1);

    input::TGameActionSpellAtMouseCursor atCursor(5);
    EXPECT_EQ(atCursor.gameActionType(), EGameAction::SPELL_AT_MOUSE_CURSOR);
    EXPECT_EQ(atCursor.spellId(), 5);
}

TEST(GameActionConstructors, ChatInputTextSpellFlagsAreSetSeparately)
{
    input::TGameActionSetCurrentChatChannelInputText text(QStringLiteral("exura"), true);
    EXPECT_FALSE(text.isSpell());
    EXPECT_EQ(text.spellCastOptionalTargetType(), 0);
    text.setIsSpell(true);
    EXPECT_TRUE(text.isSpell());
    EXPECT_EQ(text.spellCastOptionalTargetType(), 0);
    text.setSpellCastOptionalTargetType(2);
    EXPECT_EQ(text.spellCastOptionalTargetType(), 2);
}
