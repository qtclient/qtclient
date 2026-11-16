#include <gtest/gtest.h>

#include "shared/sortedvectorlistmodel.h"

#include <QAbstractListModel>

#include <utility>
#include <vector>

namespace {

struct TRow
{
    int key = 0;
    int value = 0;
};

class TRowModel final : public tibia::shared::TSortedVectorListModel<QAbstractListModel>
{
public:
    int rowCount(const QModelIndex& = {}) const override { return static_cast<int>(m_rows.size()); }
    QVariant data(const QModelIndex& index, int) const override { return m_rows[static_cast<size_t>(index.row())].key; }

    void set(std::vector<TRow> rows)
    {
        mergeSortedItems(
            m_rows, std::move(rows), [](const TRow& a, const TRow& b) { return a.key < b.key; },
            [](const TRow& a, const TRow& b) { return a.key == b.key; },
            [](const TRow& a, const TRow& b) { return a.key == b.key && a.value == b.value; });
    }

    std::vector<int> keys() const
    {
        std::vector<int> keys;
        for (const TRow& row : m_rows)
            keys.push_back(row.key);
        return keys;
    }

    std::vector<TRow> m_rows;
};

struct TSignals
{
    std::vector<std::pair<int, int>> inserted;
    std::vector<std::pair<int, int>> removed;
    std::vector<std::pair<int, int>> changed;
    std::vector<char> order;

    explicit TSignals(TRowModel& model)
    {
        QObject::connect(&model, &QAbstractItemModel::rowsInserted, [this](const QModelIndex&, int first, int last) {
            inserted.emplace_back(first, last);
            order.push_back('i');
        });
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, [this](const QModelIndex&, int first, int last) {
            removed.emplace_back(first, last);
            order.push_back('r');
        });
        QObject::connect(&model, &QAbstractItemModel::dataChanged, [this](const QModelIndex& from, const QModelIndex& to) {
            changed.emplace_back(from.row(), to.row());
            order.push_back('c');
        });
    }
};

using Pairs = std::vector<std::pair<int, int>>;

}

TEST(SortedVectorListModel, FirstItemsAreInsertedAtOnceAndSorted)
{
    TRowModel model;
    TSignals s(model);
    model.set({ { 3 }, { 1 }, { 2 } });
    EXPECT_EQ(s.inserted, (Pairs{ { 0, 2 } }));
    EXPECT_EQ(model.keys(), (std::vector<int>{ 1, 2, 3 }));
}

TEST(SortedVectorListModel, EmptyListRemovesEveryRow)
{
    TRowModel model;
    model.set({ { 1 }, { 2 } });
    TSignals s(model);
    model.set({});
    EXPECT_EQ(s.removed, (Pairs{ { 0, 1 } }));
    EXPECT_TRUE(model.keys().empty());
}

TEST(SortedVectorListModel, EqualListSendsNothing)
{
    TRowModel model;
    model.set({ { 1 }, { 2 } });
    TSignals s(model);
    model.set({ { 1 }, { 2 } });
    EXPECT_TRUE(s.order.empty());
}

TEST(SortedVectorListModel, MergesRemovalsInsertionsAndChangesInKeyOrder)
{
    TRowModel model;
    model.set({ { 1 }, { 2 }, { 3 }, { 5 }, { 8 } });
    TSignals s(model);
    // 2 and 3 go, 4 comes, 5 changes, 8 stays, 9 is appended.
    model.set({ { 9 }, { 1 }, { 4 }, { 5, 7 }, { 8 } });
    EXPECT_EQ(s.removed, (Pairs{ { 1, 2 } }));
    EXPECT_EQ(s.inserted, (Pairs{ { 1, 1 }, { 4, 4 } }));
    EXPECT_EQ(s.changed, (Pairs{ { 2, 2 } }));
    EXPECT_EQ(s.order, (std::vector<char>{ 'r', 'i', 'c', 'i' }));
    EXPECT_EQ(model.keys(), (std::vector<int>{ 1, 4, 5, 8, 9 }));
    EXPECT_EQ(model.m_rows[2].value, 7);
}

TEST(SortedVectorListModel, TrailingRowsAreRemovedAtTheEnd)
{
    TRowModel model;
    model.set({ { 1 }, { 2 }, { 3 } });
    TSignals s(model);
    model.set({ { 1 } });
    EXPECT_EQ(s.removed, (Pairs{ { 1, 2 } }));
    EXPECT_EQ(model.keys(), (std::vector<int>{ 1 }));
}
