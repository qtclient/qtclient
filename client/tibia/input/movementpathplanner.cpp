#include "input/movementpathplanner.h"

#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

namespace tibia::input {

namespace {

constexpr int Range = 110;
constexpr int Side = 2 * Range + 1;
constexpr int Infinite = std::numeric_limits<int>::max();
constexpr quint8 Blocked = 0xff;

struct Node
{
    int x = 0;
    int y = 0;
    int cost = 0;
    int g = Infinite;
    int h = Infinite;
    Node* next = nullptr;
    bool blocked = true;
    /// Position in the open heap, 0 when not in it.
    int heapIndex = 0;
};

int manhattan(const Node& n)
{
    return std::abs(n.x) + std::abs(n.y);
}

/// A 1-based binary min-heap of (key, node) that keeps each node's position,
/// so a cheaper key moves an entry up instead of adding a second one.
class OpenHeap
{
public:
    bool empty() const { return m_count == 0; }

    void push(const int key, Node* node)
    {
        ++m_count;
        if (static_cast<int>(m_items.size()) <= m_count)
            m_items.resize(static_cast<size_t>(m_count) + 1);
        m_items[static_cast<size_t>(m_count)] = { key, node };
        node->heapIndex = m_count;
        siftUp(m_count);
    }

    void decreaseKey(const int index, const int key)
    {
        m_items[static_cast<size_t>(index)].first = key;
        siftUp(index);
    }

    Node* pop()
    {
        Node* top = m_items[1].second;
        int hole = 1;
        if (2 < m_count) {
            int child = 2;
            do {
                int pick = child;
                if ((child | 1) < m_count && m_items[static_cast<size_t>(child | 1)].first < m_items[static_cast<size_t>(child)].first)
                    pick = child | 1;
                if (m_items[static_cast<size_t>(m_count)].first <= m_items[static_cast<size_t>(pick)].first)
                    break;
                place(hole, m_items[static_cast<size_t>(pick)]);
                hole = pick;
                child = pick * 2;
            } while (child < m_count);
        }
        place(hole, m_items[static_cast<size_t>(m_count)]);
        --m_count;
        top->heapIndex = 0;
        return top;
    }

private:
    using Item = std::pair<int, Node*>;

    void place(const int index, const Item item)
    {
        m_items[static_cast<size_t>(index)] = item;
        item.second->heapIndex = index;
    }

    void siftUp(int index)
    {
        while (index != 1) {
            const int parent = index / 2;
            if (m_items[static_cast<size_t>(parent)].first <= m_items[static_cast<size_t>(index)].first)
                return;
            const Item item = m_items[static_cast<size_t>(index)];
            place(index, m_items[static_cast<size_t>(parent)]);
            place(parent, item);
            index = parent;
        }
    }

    std::vector<Item> m_items = std::vector<Item>(1);
    int m_count = 0;
};

creatures::EMovementDirection stepBetween(const int fromX, const int fromY, const int toX, const int toY)
{
    return creatures::directionOfStep(worldmap::TWorldMapExtent(toX - fromX, toY - fromY, 0));
}

}

int planMovementPath(const worldmap::TWorldMapCoordinate& from, const worldmap::TWorldMapCoordinate& goal,
                     const bool enterUnwalkableGoal, const bool allowDiagonalStep, const WaypointCost& cost,
                     const std::vector<worldmap::TWorldMapCoordinate>& blocked,
                     std::vector<creatures::EMovementDirection>& path)
{
    path.clear();
    if (goal.z < from.z)
        return PathGoUpstairs;
    if (goal.z > from.z)
        return PathGoDownstairs;
    const int goalX = goal.x - from.x;
    const int goalY = goal.y - from.y;
    if (std::abs(goalX) > Range || std::abs(goalY) > Range)
        return PathOutOfRange;
    if (goalX == 0 && goalY == 0)
        return PathAlreadyThere;
    if (std::abs(goalX) + std::abs(goalY) == 1
        || (allowDiagonalStep && std::abs(goalX) == 1 && std::abs(goalY) == 1)) {
        path.push_back(stepBetween(0, 0, goalX, goalY));
        return PathComplete;
    }

    std::vector<Node> grid(static_cast<size_t>(Side) * Side);
    const auto at = [&](const int x, const int y) -> Node* {
        if (std::abs(x) > Range || std::abs(y) > Range)
            return nullptr;
        return &grid[static_cast<size_t>(y + Range) * Side + static_cast<size_t>(x + Range)];
    };
    int cheapest = Infinite;
    for (int y = -Range; y <= Range; ++y) {
        for (int x = -Range; x <= Range; ++x) {
            Node& n = *at(x, y);
            n.x = x;
            n.y = y;
            const int fieldX = from.x + x;
            const int fieldY = from.y + y;
            n.cost = (fieldX < 0 || fieldY < 0) ? 1 : cost(fieldX, fieldY, from.z);
            n.blocked = n.cost == Blocked;
            cheapest = std::min(cheapest, n.cost);
        }
    }
    // Blocked fields other than the start and the goal, inside the range.
    for (const worldmap::TWorldMapCoordinate& c : blocked) {
        if (c == from || c == goal || c.z != from.z)
            continue;
        const int dx = c.x - from.x;
        const int dy = c.y - from.y;
        if (std::abs(dx) < Range && std::abs(dy) < Range) {
            Node* n = at(dx, dy);
            n->cost = Blocked;
            n->blocked = true;
        }
    }

    Node* origin = at(0, 0);
    Node* target = at(goalX, goalY);
    target->g = 0;

    // Searched backwards from the goal. Neighbours are visited column by
    // column (west to east), each from north to south.
    OpenHeap open;
    open.push((manhattan(*target) - 1) * cheapest + target->cost, target);
    const auto relax = [&](Node* n, Node* m, const int g) {
        if (!m || m->g <= g)
            return;
        m->g = g;
        m->next = n;
        if (m == origin || m->blocked)
            return;
        if (m->h == Infinite)
            m->h = (manhattan(*m) - 1) * cheapest + m->cost;
        if (m->heapIndex != 0)
            open.decreaseKey(m->heapIndex, m->h + g);
        else
            open.push(m->h + g, m);
    };
    while (!open.empty()) {
        Node* n = open.pop();
        const int base = n->g + n->cost;
        if (base >= origin->g)
            continue;
        const int diagonal = base + 2 * n->cost;
        for (int dx = -1; dx <= 1; ++dx) {
            relax(n, at(n->x + dx, n->y - 1), dx != 0 ? diagonal : base);
            if (dx != 0)
                relax(n, at(n->x + dx, n->y), base);
            relax(n, at(n->x + dx, n->y + 1), dx != 0 ? diagonal : base);
        }
    }
    if (origin->g == Infinite)
        return PathNoWay;

    int result = PathPartial;
    int prevX = 0;
    int prevY = 0;
    for (Node* step = origin->next;; step = step->next) {
        if (!step) {
            result = PathComplete;
            break;
        }
        if (std::abs(step->x) > 8 || std::abs(step->y) > 6 || path.size() >= 10)
            break;
        if (!enterUnwalkableGoal && step == target && step->cost == Blocked) {
            result = PathComplete;
            break;
        }
        path.push_back(stepBetween(prevX, prevY, step->x, step->y));
        prevX = step->x;
        prevY = step->y;
    }
    return path.empty() ? PathAlreadyThere : result;
}

}
