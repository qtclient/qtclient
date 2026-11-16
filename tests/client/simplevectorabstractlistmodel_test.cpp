#include <gtest/gtest.h>

#include "shared/simplevectorabstractlistmodel.h"

#include <utility>
#include <vector>

namespace {

class TIntModel final : public tibia::shared::TSimpleVectorAbstractListModel<int>
{
protected:
    QVariant itemData(const int& item, const int role) const override { return role == Qt::DisplayRole ? item : QVariant(); }
    QVariant defaultData(int) const override { return QStringLiteral("none"); }
};

struct TSignals
{
    std::vector<std::pair<int, int>> inserted;
    std::vector<std::pair<int, int>> removed;
    std::vector<std::pair<int, int>> changed;

    explicit TSignals(TIntModel& model)
    {
        QObject::connect(&model, &QAbstractItemModel::rowsInserted,
                         [this](const QModelIndex&, int first, int last) { inserted.emplace_back(first, last); });
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved,
                         [this](const QModelIndex&, int first, int last) { removed.emplace_back(first, last); });
        QObject::connect(&model, &QAbstractItemModel::dataChanged,
                         [this](const QModelIndex& from, const QModelIndex& to) { changed.emplace_back(from.row(), to.row()); });
    }
};

}

TEST(SimpleVectorAbstractListModel, GrowingInsertsAndReportsOnlyTheOldRowsChanged)
{
    TIntModel model;
    model.setItems({ 1, 2 });
    TSignals s(model);
    model.setItems({ 3, 4, 5, 6 });
    ASSERT_EQ(s.inserted.size(), 1u);
    EXPECT_EQ(s.inserted[0], std::make_pair(2, 3));
    ASSERT_EQ(s.changed.size(), 1u);
    EXPECT_EQ(s.changed[0], std::make_pair(0, 1));
    EXPECT_EQ(model.rowCount(), 4);
}

TEST(SimpleVectorAbstractListModel, ShrinkingRemovesAndReportsTheRemainingRowsChanged)
{
    TIntModel model;
    model.setItems({ 1, 2, 3 });
    TSignals s(model);
    model.setItems({ 9 });
    ASSERT_EQ(s.removed.size(), 1u);
    EXPECT_EQ(s.removed[0], std::make_pair(1, 2));
    ASSERT_EQ(s.changed.size(), 1u);
    EXPECT_EQ(s.changed[0], std::make_pair(0, 0));
    model.clear();
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(s.changed.size(), 1u);
}

TEST(SimpleVectorAbstractListModel, DataOutsideTheRowsIsTheDefault)
{
    TIntModel model;
    model.setItems({ 7 });
    EXPECT_EQ(model.data(model.index(0, 0), Qt::DisplayRole).toInt(), 7);
    EXPECT_EQ(model.data(QModelIndex(), Qt::DisplayRole).toString(), QStringLiteral("none"));
    EXPECT_EQ(model.items(), std::vector<int>{ 7 });
    // The parent is not looked at.
    EXPECT_EQ(model.rowCount(model.index(0, 0)), 1);
}
