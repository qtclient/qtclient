#include "common/plural.h"
#include "common/pluralexception.h"

#include <gtest/gtest.h>

using tibia::common::plural;
using tibia::common::TPluralException;

TEST(Plural, EmptyNameThrows)
{
    EXPECT_THROW(plural("", 2), TPluralException);
}

TEST(Plural, CountOneAndSingleLetterAreUnchanged)
{
    EXPECT_EQ(plural("a sword", 1), "a sword");
    EXPECT_EQ(plural("x", 5), "x");
}

TEST(Plural, NegativeCountOnlyDropsTheArticle)
{
    EXPECT_EQ(plural("a sword", -1), "sword");
    EXPECT_EQ(plural("an axe", -1), "axe");
    EXPECT_EQ(plural("a ", -1), "a ");
    EXPECT_EQ(plural("dragon lord", -1), "dragon lord");
}

TEST(Plural, SuffixRules)
{
    EXPECT_EQ(plural("a sword", 0), "swords");
    EXPECT_EQ(plural("a torch", 0), "torches");
    EXPECT_EQ(plural("a fish bush", 0), "fish bush"); // "fish" never changes
    EXPECT_EQ(plural("a moth", 0), "moths");
    EXPECT_EQ(plural("a box", 0), "boxes");
    EXPECT_EQ(plural("a buzz", 0), "buzzes");
    EXPECT_EQ(plural("a glass", 0), "glasses");
    EXPECT_EQ(plural("a fly", 0), "flies");
    EXPECT_EQ(plural("a key", 0), "keys");
    EXPECT_EQ(plural("a wolf", 0), "wolves");
    EXPECT_EQ(plural("a potato", 0), "potatoes");
    EXPECT_EQ(plural("a demo", 0), "demos");
}

TEST(Plural, CountPrefix)
{
    EXPECT_EQ(plural("a sword", 3), "3 swords");
    EXPECT_EQ(plural("an orc", 2), "2 orcs");
}

TEST(Plural, IrregularNames)
{
    EXPECT_EQ(plural("an orc spearman", 0), "orc spearmen");
    EXPECT_EQ(plural("a gingerbread man", 2), "2 gingerbread men");
    EXPECT_EQ(plural("a cyclops", 0), "cyclopes");
    EXPECT_EQ(plural("a medusa", 0), "medusae");
    EXPECT_EQ(plural("a knife", 0), "knives");
    EXPECT_EQ(plural("a giant foot", 0), "giant feet");
    EXPECT_EQ(plural("a tooth", 0), "teeth");
    EXPECT_EQ(plural("a hoof", 0), "hooves");
    EXPECT_EQ(plural("a fungus", 0), "fungi");
    EXPECT_EQ(plural("a hero", 0), "heroes");
    EXPECT_EQ(plural("a master thief", 0), "master thieves");
}

TEST(Plural, UnchangingNames)
{
    EXPECT_EQ(plural("a sheep", 0), "sheep");
    EXPECT_EQ(plural("a white deer", 4), "4 white deer");
    EXPECT_EQ(plural("an efreet", 0), "efreet");
    EXPECT_EQ(plural("a jellyfish", 0), "jellyfish");
}

TEST(Plural, OfKeepsTheRestUnchanged)
{
    EXPECT_EQ(plural("a pair of boots", 0), "pairs of boots");
    EXPECT_EQ(plural("a flask of rust remover", 2), "2 flasks of rust remover");
    // Only a lone "of" splits; "roof" stays in the head.
    EXPECT_EQ(plural("a roof tile", 0), "roof tiles");
}

TEST(Plural, WithPluralisesBothSides)
{
    EXPECT_EQ(plural("a dwarf with a sword", 0), "dwarfs with swords");
    EXPECT_EQ(plural("a dwarf with a sword", 2), "2 dwarfs with swords");
    EXPECT_EQ(plural("a cake with cream", 0), "cakes with cream");
}

TEST(Plural, WordsAreJoinedBySingleSpaces)
{
    EXPECT_EQ(plural("a big   rat", 0), "big rats");
}
