// Host API v9 cip_object table (client/tibia/mods/cip_object_abi.cpp over the generated
// delegates of tools/cip/gen_cip_objects.py): classes by name, the pack delegate first, the
// CipSoft base as fallback, property storage writes with NOTIFY.
#include "mods/abi_internal.h"
#include "mods/cip_object_support.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QObject>

#include <string>
#include <vector>

// The game window glue (cip_object_host.cpp) and the host-class factories
// (cip_objects_host.cpp) need the whole client; the table here has model classes only.
namespace tibia::mods::cipobjects {
const CipHostClass kHostClasses[] = { { "", nullptr, false } };
const size_t kHostClassCount = 0;
bool cipInvokeHostMethod(QObject*, const QByteArray&, const QJsonArray&)
{
    return false;
}
int cipSetHostController(int, QObject*)
{
    return 0;
}
} // namespace tibia::mods::cipobjects

namespace {

const QtClientCipObjectApiV1* api()
{
    return tibia::mods::cipObjectApiV1();
}

enum class EMode { Base, Handle, CallBase };

struct TPackSide
{
    EMode mode = EMode::Base;
    std::vector<std::string> methods;
    std::vector<std::string> args;
    int destroyed = 0;
};

int packInvoke(void* user, QtClientQObjectPtr object, const char* method, const char* argsJson, char* result,
               size_t resultLen)
{
    auto* pack = static_cast<TPackSide*>(user);
    pack->methods.emplace_back(method);
    pack->args.emplace_back(argsJson);
    if (result && resultLen)
        result[0] = '\0';
    switch (pack->mode) {
    case EMode::Base:
        return 0;
    case EMode::Handle:
        return 1;
    case EMode::CallBase:
        return api()->call_base(object, method, argsJson, nullptr, 0) == 0 ? 1 : 0;
    }
    return 0;
}

void packDestroyed(void* user)
{
    ++static_cast<TPackSide*>(user)->destroyed;
}

QtClientCipObjectDelegateV1 delegateFor(TPackSide& pack)
{
    QtClientCipObjectDelegateV1 d{};
    d.struct_size = sizeof(d);
    d.user = &pack;
    d.invoke = &packInvoke;
    d.destroyed = &packDestroyed;
    return d;
}

std::string valueOf(QtClientQObjectPtr object, const char* property)
{
    char buf[256] = {};
    if (api()->get_value(object, property, buf, sizeof(buf)) < 0)
        return "<unknown>";
    return buf;
}

struct TSignalCount
{
    int count = 0;
    std::string lastSignal;
};

void countSignal(void* user, QtClientQObjectPtr, const char* signal, const char*)
{
    auto* c = static_cast<TSignalCount*>(user);
    ++c->count;
    c->lastSignal = signal;
}

constexpr const char* kBadges = "TFriendsBadgesPageController";

} // namespace

TEST(CipObjectTable, ClassesByName)
{
    EXPECT_EQ(api()->class_supported(kBadges), 1);
    EXPECT_EQ(api()->class_supported("tibia::gamewindow::TFriendsBadgesPageController"), 1);
    EXPECT_EQ(api()->class_supported("TSocialButtonController"), 1);
    EXPECT_EQ(api()->class_supported("TNoSuchCipSoftClass"), 0);
    EXPECT_EQ(api()->create("TNoSuchCipSoftClass", nullptr, nullptr), nullptr);

    QtClientQObjectPtr o = api()->create(kBadges, nullptr, nullptr);
    ASSERT_NE(o, nullptr);
    EXPECT_TRUE(static_cast<QObject*>(o)->inherits("cip::tibia::gamewindow::TFriendsBadgesPageController"));
    api()->destroy(o);
}

TEST(CipObjectTable, DelegateHandlesThenBaseRuns)
{
    TPackSide pack;
    const QtClientCipObjectDelegateV1 d = delegateFor(pack);
    QtClientQObjectPtr o = api()->create(kBadges, nullptr, &d);
    ASSERT_NE(o, nullptr);
    const int showHidden = api()->enum_value(kBadges, "EBadgeFilter", "ShowHidden");
    EXPECT_EQ(showHidden, 4);
    EXPECT_EQ(api()->enum_value(kBadges, "EBadgeFilter", "NoSuchKey"), -1);
    const std::string arg = "[" + std::to_string(showHidden) + "]";

    // The pack handles the virtual setter: CipSoft's storage stays as it was.
    pack.mode = EMode::Handle;
    char result[64] = {};
    EXPECT_EQ(api()->invoke(o, "setBadgeFilter", arg.c_str(), result, sizeof(result)), 0);
    ASSERT_EQ(pack.methods.size(), 1u);
    EXPECT_EQ(pack.methods.back(), "setBadgeFilter");
    EXPECT_EQ(pack.args.back(), arg);
    EXPECT_EQ(valueOf(o, "badgeFilter"), "0");

    // The pack passes (returns 0): CipSoft's base setter runs.
    pack.mode = EMode::Base;
    EXPECT_EQ(api()->invoke(o, "setBadgeFilter", arg.c_str(), result, sizeof(result)), 0);
    EXPECT_EQ(pack.methods.size(), 2u);
    EXPECT_EQ(valueOf(o, "badgeFilter"), std::to_string(showHidden));

    // Unknown methods / bad arguments.
    EXPECT_EQ(api()->invoke(o, "noSuchMethod", "[]", result, sizeof(result)), -1);
    EXPECT_EQ(api()->invoke(o, "setBadgeFilter", "{not json", result, sizeof(result)), -1);

    api()->destroy(o);
    EXPECT_EQ(pack.destroyed, 1);
}

TEST(CipObjectTable, CallBaseFromTheDelegate)
{
    TPackSide pack;
    pack.mode = EMode::CallBase;
    const QtClientCipObjectDelegateV1 d = delegateFor(pack);
    QtClientQObjectPtr o = api()->create(kBadges, nullptr, &d);
    ASSERT_NE(o, nullptr);
    TSignalCount changed;
    const int sub = api()->subscribe_signal(o, "badgeFilterChanged", &countSignal, &changed);
    EXPECT_GT(sub, 0);

    EXPECT_EQ(api()->invoke(o, "setBadgeFilter", "[2]", nullptr, 0), 0);
    EXPECT_EQ(pack.methods.size(), 1u);
    EXPECT_EQ(valueOf(o, "badgeFilter"), "2");
    EXPECT_EQ(changed.count, 1);
    EXPECT_EQ(changed.lastSignal, "badgeFilterChanged");

    // call_base skips the delegate.
    EXPECT_EQ(api()->call_base(o, "setBadgeFilter", "[3]", nullptr, 0), 0);
    EXPECT_EQ(pack.methods.size(), 1u);
    EXPECT_EQ(valueOf(o, "badgeFilter"), "3");
    EXPECT_EQ(changed.count, 2);
    EXPECT_EQ(api()->call_base(o, "noSuchMethod", "[]", nullptr, 0), -1);

    api()->unsubscribe_signal(sub);
    EXPECT_EQ(api()->call_base(o, "setBadgeFilter", "[1]", nullptr, 0), 0);
    EXPECT_EQ(changed.count, 2);
    api()->destroy(o);
}

TEST(CipObjectTable, SetValueWritesStorageAndNotifies)
{
    QObject parent;
    QtClientQObjectPtr o = api()->create("TSocialButtonController", &parent, nullptr);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(static_cast<QObject*>(o)->parent(), &parent);
    TSignalCount changed;
    api()->subscribe_signal(o, "changed", &countSignal, &changed);

    EXPECT_EQ(valueOf(o, "highlighted"), "false");
    EXPECT_EQ(api()->set_value(o, "highlighted", "true"), 0);
    EXPECT_EQ(valueOf(o, "highlighted"), "true");
    EXPECT_EQ(changed.count, 1);
    // Unchanged: no NOTIFY.
    EXPECT_EQ(api()->set_value(o, "highlighted", "true"), 0);
    EXPECT_EQ(changed.count, 1);
    // Unknown property / a value of the wrong type.
    EXPECT_EQ(api()->set_value(o, "noSuchProperty", "1"), -1);
    EXPECT_EQ(api()->set_value(o, "highlighted", "\"yes\""), -1);
    EXPECT_EQ(valueOf(o, "noSuchProperty"), "<unknown>");

    // A signal emitted by name reaches the subscribers.
    EXPECT_EQ(api()->emit_signal(o, "changed", "[]"), 0);
    EXPECT_EQ(changed.count, 2);
    EXPECT_EQ(api()->emit_signal(o, "noSuchSignal", "[]"), -1);
    // Deleted with the Qt parent.
}
