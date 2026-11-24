#include "proficiency/proficiencycatalogue.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using tibia::proficiency::shapeOptions;

namespace {

std::vector<int> commonIndices()
{
    std::vector<int> indices;
    for (int index = 251; index <= 271; ++index)
        indices.push_back(index);
    for (int index = 281; index <= 287; ++index)
        indices.push_back(index);
    for (int index = 291; index <= 293; ++index)
        indices.push_back(index);
    for (int index = 321; index <= 323; ++index)
        indices.push_back(index);
    return indices;
}

/// The profession's augment block: five rows of columns 1..6, the row's
/// fifth column left out when `skipFifth`.
std::vector<int> augmentIndices(const int profession, const bool skipFifth)
{
    std::vector<int> indices;
    const int base = (profession - 1) * 50;
    for (int row = 0; row < 5; ++row)
        for (int column = 1; column <= 6; ++column)
            if (!skipFifth || column != 5)
                indices.push_back(base + row * 10 + column);
    return indices;
}

std::vector<int> expectedTable(const int profession, const bool skipFifth)
{
    std::vector<int> table = commonIndices();
    const std::vector<int> augments = augmentIndices(profession, skipFifth);
    table.insert(table.end(), augments.begin(), augments.end());
    return table;
}

}

TEST(ProficiencyShapeOptions, FirstTablesHaveTheCommonIndicesThenTheProfessionsAugments)
{
    for (int profession = 1; profession <= 5; ++profession) {
        const std::vector<int> options = shapeOptions(profession, false);
        ASSERT_EQ(options.size(), 64u) << profession;
        EXPECT_EQ(options, expectedTable(profession, false)) << profession;
    }
}

TEST(ProficiencyShapeOptions, AlternateTableOfTheFirstProfessionLeavesOutTheFifthColumn)
{
    const std::vector<int> options = shapeOptions(1, true);
    ASSERT_EQ(options.size(), 59u);
    EXPECT_EQ(options, expectedTable(1, true));
    for (const int column5 : { 5, 15, 25, 35, 45 })
        EXPECT_EQ(std::count(options.begin(), options.end(), column5), 0) << column5;
}

TEST(ProficiencyShapeOptions, OtherAlternateTablesAreTheFirstTables)
{
    for (int profession = 2; profession <= 5; ++profession)
        EXPECT_EQ(shapeOptions(profession, true), shapeOptions(profession, false)) << profession;
}

TEST(ProficiencyShapeOptions, UnknownProfessionsHaveNoTable)
{
    EXPECT_TRUE(shapeOptions(0, false).empty());
    EXPECT_TRUE(shapeOptions(6, true).empty());
}
