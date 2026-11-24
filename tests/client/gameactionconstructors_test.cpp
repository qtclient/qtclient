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
    EXPECT_EQ(action.storeOpenParameters().premiumDetail, 7);
    EXPECT_TRUE(action.storeOpenParameters().category.isEmpty());
    EXPECT_TRUE(action.storeOpenParameters().subcategory.isEmpty());
    EXPECT_EQ(action.storeOpenParameters().usefulThing, -1);
    EXPECT_EQ(action.storeOpenParameters().offerId, 0u);
    EXPECT_EQ(action.storeOpenParameters().sortOrder, 0);
    EXPECT_EQ(action.storeOpenParameters().serviceType, 0);
}

TEST(GameActionConstructors, OpenStoreKeepsTheWholeOpenParameters)
{
    tibia::store::TStoreOpenParameters parameters;
    parameters.openAction = tibia::store::OPEN_CATEGORY;
    parameters.premiumDetail = 4;
    parameters.category = QStringLiteral("Mounts");
    parameters.subcategory = QStringLiteral("Special");
    parameters.usefulThing = 2;
    parameters.offerId = 77;
    parameters.sortOrder = 3;
    parameters.serviceType = 1;
    const input::TGameActionOpenStore action(parameters);
    EXPECT_EQ(action.gameActionType(), EGameAction::OPEN_STORE);
    const auto& kept = action.storeOpenParameters();
    EXPECT_EQ(kept.openAction, tibia::store::OPEN_CATEGORY);
    EXPECT_EQ(kept.premiumDetail, 4);
    EXPECT_EQ(kept.category, QStringLiteral("Mounts"));
    EXPECT_EQ(kept.subcategory, QStringLiteral("Special"));
    EXPECT_EQ(kept.usefulThing, 2);
    EXPECT_EQ(kept.offerId, 77u);
    EXPECT_EQ(kept.sortOrder, 3);
    EXPECT_EQ(kept.serviceType, 1);
}

TEST(GameActionConstructors, UpdateExivaOptionsCarriesTheFlagsAndBothWhitelists)
{
    tibia::exivaoptions::TExivaOptionsStorage::Options options;
    options.allowOwnParty = true;
    options.allowGuildWhitelist = true;
    const input::TGameActionUpdateExivaOptions action(
        options, { { QStringLiteral("Alice"), QStringLiteral("Bob") }, { QStringLiteral("Red Rose") } });
    EXPECT_EQ(action.gameActionType(), EGameAction::EXIVA_UPADATE_OPTIONS);
    EXPECT_FALSE(action.options().allowAll);
    EXPECT_FALSE(action.options().allowOwnGuild);
    EXPECT_TRUE(action.options().allowOwnParty);
    EXPECT_FALSE(action.options().allowVipList);
    EXPECT_FALSE(action.options().allowPlayerWhitelist);
    EXPECT_TRUE(action.options().allowGuildWhitelist);
    const auto whitelists = action.whitelists();
    EXPECT_EQ(whitelists.characters, (tibia::exivaoptions::TExivaOptionsStorage::Names{ QStringLiteral("Alice"), QStringLiteral("Bob") }));
    EXPECT_EQ(whitelists.guilds, (tibia::exivaoptions::TExivaOptionsStorage::Names{ QStringLiteral("Red Rose") }));
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

TEST(GameActionConstructors, OpenShowImageDialogCopiesTheDialogData)
{
    input::TGameActionOpenShowImageDialog::TData data;
    data.showCrestHeader = true;
    data.caption = QStringLiteral("Level 9");
    data.bannerPath = QStringLiteral("/images/banner.png");
    data.images = { { QStringLiteral("/images/a.png"), QStringLiteral("/images/icon-a.png") },
                    { QStringLiteral("/images/b.png"), QString() } };
    const input::TGameActionOpenShowImageDialog action(data);
    EXPECT_EQ(action.gameActionType(), EGameAction::OPEN_SHOW_IMAGE_DIALOG);
    EXPECT_EQ(action.data(), data);
    EXPECT_FALSE(action.data().showDragonHeader);
    EXPECT_TRUE(action.data().showCrestHeader);
    EXPECT_FALSE(action.data().showCaption);
    ASSERT_EQ(action.data().images.size(), 2u);
    EXPECT_EQ(action.data().images[0].tabIcon, QStringLiteral("/images/icon-a.png"));
}

TEST(GameActionConstructors, SessiondumpStartCarriesTheWholeCatalogEntry)
{
    tibia::sessiondump::TSessiondumpCatalog::TEntry entry;
    entry.url = QUrl(QStringLiteral("https://example.invalid/tutorial.sessiondump"));
    entry.hintsUrl = QUrl(QStringLiteral("https://example.invalid/tutorial.hints"));
    entry.fileSize = 12345;
    entry.durationMilliseconds = 67890;
    entry.hasHints = true;
    const input::TGameActionSessiondumpStart action(entry);
    EXPECT_EQ(action.gameActionType(), EGameAction::SESSIONDUMP_START);
    const auto& kept = action.sessiondumpEntry();
    EXPECT_EQ(kept.url, entry.url);
    EXPECT_EQ(kept.hintsUrl, entry.hintsUrl);
    EXPECT_EQ(kept.fileSize, 12345);
    EXPECT_EQ(kept.durationMilliseconds, 67890);
    EXPECT_TRUE(kept.hasHints);
    EXPECT_EQ(action.url(), entry.url);
}
