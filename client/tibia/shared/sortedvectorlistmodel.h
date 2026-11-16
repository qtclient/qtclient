#pragma once

#include <QAbstractItemModel>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace tibia {
namespace shared {

/// The item update of CIP's sorted vector list models (TAppearanceTypeListModel,
/// TKillTrackerWidgetController::TTaskMonsterModel): the stored items are kept
/// in key order and a new item list is merged into them with the fewest row
/// signals. `Base` is the QAbstractListModel the concrete model derives from.
template<typename Base>
class TSortedVectorListModel : public Base
{
public:
    using Base::Base;

protected:
    /// Merges `next` into `items`.
    /// - no items yet: all of `next` is inserted at once, then sorted;
    /// - `next` empty: every row is removed;
    /// - same size and every item `equal` at the same position: nothing;
    /// - else both are walked in key order: an item of `items` whose key is
    ///   below the next new key is removed, a new key below the current item
    ///   is inserted, an item with the same key is kept, and copied (reported
    ///   changed) unless `equal`. Runs of one kind are announced together
    ///   when the kind changes; rows left over at the end are removed.
    template<typename T, typename Less, typename SameKey, typename Equal>
    void mergeSortedItems(std::vector<T>& items, std::vector<T> next, Less less, SameKey sameKey, Equal equal)
    {
        if (items.empty()) {
            if (next.empty())
                return;
            this->beginInsertRows({}, 0, static_cast<int>(next.size()) - 1);
            items = std::move(next);
            std::sort(items.begin(), items.end(), less);
            this->endInsertRows();
            return;
        }
        if (next.empty()) {
            this->beginRemoveRows({}, 0, static_cast<int>(items.size()) - 1);
            items.clear();
            this->endRemoveRows();
            return;
        }
        if (items.size() == next.size() && std::equal(items.begin(), items.end(), next.begin(), equal))
            return;

        std::vector<size_t> order(next.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&next, &less](const size_t a, const size_t b) { return less(next[a], next[b]); });

        enum EPending
        {
            NONE,
            CHANGE,
            INSERT,
            REMOVE,
        };
        EPending pending = NONE;
        size_t cursor = 0;
        int changeFirst = -1;
        int changeLast = -1;
        int insertAt = -1;
        std::vector<size_t> inserts;
        int removeFirst = -1;
        int removeLast = -1;
        const auto switchTo = [&](const EPending kind) {
            if (pending == kind)
                return;
            if (pending == REMOVE && removeFirst != -1) {
                this->beginRemoveRows({}, removeFirst, removeLast);
                items.erase(items.begin() + removeFirst, items.begin() + removeLast + 1);
                this->endRemoveRows();
                cursor = static_cast<size_t>(removeFirst);
                removeFirst = removeLast = -1;
            } else if (pending == INSERT && insertAt != -1) {
                this->beginInsertRows({}, insertAt, insertAt + static_cast<int>(inserts.size()) - 1);
                for (size_t i = 0; i < inserts.size(); ++i)
                    items.insert(items.begin() + insertAt + static_cast<std::ptrdiff_t>(i), next[inserts[i]]);
                this->endInsertRows();
                cursor += inserts.size();
                insertAt = -1;
                inserts.clear();
            } else if (pending == CHANGE && changeFirst != -1) {
                Q_EMIT this->dataChanged(this->index(changeFirst, 0), this->index(changeLast, 0));
                changeFirst = changeLast = -1;
            }
            pending = kind;
        };

        for (size_t i = 0; i < order.size();) {
            const T& item = next[order[i]];
            if (cursor < items.size()) {
                const T& current = items[cursor];
                if (sameKey(current, item)) {
                    if (equal(current, item)) {
                        switchTo(NONE);
                    } else {
                        // A flush moves the rows: the row is read again after it.
                        switchTo(CHANGE);
                        items[cursor] = item;
                        changeLast = static_cast<int>(cursor);
                        if (changeFirst == -1)
                            changeFirst = changeLast;
                    }
                    ++cursor;
                    ++i;
                } else if (less(current, item)) {
                    switchTo(REMOVE);
                    removeLast = static_cast<int>(cursor);
                    if (removeFirst == -1)
                        removeFirst = removeLast;
                    ++cursor;
                } else {
                    switchTo(INSERT);
                    if (insertAt == -1)
                        insertAt = static_cast<int>(cursor);
                    inserts.push_back(order[i]);
                    ++i;
                }
            } else {
                switchTo(INSERT);
                if (insertAt == -1)
                    insertAt = static_cast<int>(i);
                inserts.push_back(order[i]);
                ++i;
            }
        }
        switchTo(NONE);
        if (cursor < items.size()) {
            this->beginRemoveRows({}, static_cast<int>(cursor), static_cast<int>(items.size()) - 1);
            items.erase(items.begin() + static_cast<std::ptrdiff_t>(cursor), items.end());
            this->endRemoveRows();
        }
    }
};

}
}
