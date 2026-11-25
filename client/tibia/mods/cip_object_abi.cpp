// Host API v9: the CipSoft object table (abi/cip_object_api.h). Every class of the vendored
// contract is in the generated table (tools/cip/gen_cip_objects.py -> cip_objects_*.cpp):
// model classes are made as generated delegate subclasses that ask the pack first, host
// classes as the host's implementation (cip_host_classes.h). Calls, properties and signals by
// name go through the meta-object system; JSON carries the values. The game window glue
// (host-class C++ members, set_host_controller) is in cip_object_host.cpp.
#include "mods/abi_internal.h"
#include "mods/cip_object_support.h"

#include <QHash>
#include <QJsonArray>
#include <QMetaEnum>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QPointer>

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace tibia::mods {

namespace cipobjects {

bool cipAskDelegate(CipDelegateMixin* mixin, QObject* object, const char* method, const QJsonArray& args, QJsonValue& result)
{
    if (!mixin || !mixin->hasDelegate())
        return false;
    const QtClientCipObjectDelegateV1& d = mixin->delegate();
    const QByteArray argsJson = QJsonDocument(args).toJson(QJsonDocument::Compact);
    std::vector<char> buf(4096, '\0');
    if (d.invoke(d.user, object, method, argsJson.constData(), buf.data(), buf.size()) == 0)
        return false;
    buf.back() = '\0';
    QJsonValue value;
    result = parseJsonValue(buf.data(), value, false) ? value : QJsonValue();
    return true;
}

void cipDelegateDestroyed(CipDelegateMixin* mixin)
{
    if (!mixin)
        return;
    const QtClientCipObjectDelegateV1 d = mixin->delegate();
    mixin->clearDelegate();
    if (d.destroyed)
        d.destroyed(d.user);
}

void cipEmitNotify(QObject* object, const char* property)
{
    const QMetaObject* meta = object->metaObject();
    const int index = meta->indexOfProperty(property);
    if (index < 0)
        return;
    const QMetaProperty p = meta->property(index);
    if (!p.hasNotifySignal())
        return;
    const QMetaMethod signal = p.notifySignal();
    QVariant value;
    void* argv[2] = { nullptr, nullptr };
    if (signal.parameterCount() >= 1) {
        value = p.read(object);
        argv[1] = value.data();
    }
    QMetaObject::activate(object, signal.enclosingMetaObject(), signal.methodIndex() - signal.enclosingMetaObject()->methodOffset(),
                          argv);
}

} // namespace cipobjects

namespace {

using namespace cipobjects;

struct TClassEntry
{
    const CipClassInfo* info = nullptr;
    const CipHostClass* host = nullptr;
};

struct TClassTable
{
    std::unordered_map<std::string, TClassEntry> byName;
};

const TClassTable& classTable()
{
    static const TClassTable table = [] {
        TClassTable t;
        std::unordered_map<std::string, int> shortCount;
        std::unordered_map<std::string, const CipHostClass*> hosts;
        for (size_t i = 0; i < kHostClassCount; ++i)
            hosts[kHostClasses[i].name] = &kHostClasses[i];
        std::vector<const CipClassInfo*> all;
        for (size_t c = 0; c < kChunkCount; ++c)
            for (size_t i = 0; i < *kChunks[c].size; ++i)
                all.push_back(&kChunks[c].rows[i]);
        for (const CipClassInfo* info : all)
            ++shortCount[info->shortName];
        for (const CipClassInfo* info : all) {
            TClassEntry entry{ info, hosts.contains(info->name) ? hosts[info->name] : nullptr };
            t.byName[info->name] = entry;
            // CipSoft's class name without namespace when it is unique.
            if (shortCount[info->shortName] == 1)
                t.byName[info->shortName] = entry;
        }
        return t;
    }();
    return table;
}

const TClassEntry* classOf(const char* name)
{
    if (!name)
        return nullptr;
    const auto& byName = classTable().byName;
    const auto it = byName.find(name);
    return it == byName.end() ? nullptr : &it->second;
}

/// The objects this table made, with their class (model or host).
struct TMadeObject
{
    const TClassEntry* entry = nullptr;
    bool model = false;
};

QHash<QObject*, TMadeObject>& madeObjects()
{
    static QHash<QObject*, TMadeObject> objects;
    return objects;
}

const TMadeObject* madeOf(QObject* object)
{
    const auto& made = madeObjects();
    const auto it = made.constFind(object);
    return it == made.constEnd() ? nullptr : &it.value();
}

bool isHost(const TClassEntry& e)
{
    return e.host && e.host->exists && e.host->make;
}

int coSupported(const char* class_name)
{
    const TClassEntry* e = classOf(class_name);
    if (!e)
        return 0;
    return isHost(*e) ? 2 : 1;
}

QtClientQObjectPtr coCreate(const char* class_name, QtClientQObjectPtr parent, const QtClientCipObjectDelegateV1* delegate)
{
    const TClassEntry* e = classOf(class_name);
    if (!e)
        return nullptr;
    QtClientCipObjectDelegateV1 d{};
    if (delegate)
        std::memcpy(&d, delegate, std::min(delegate->struct_size ? delegate->struct_size : sizeof(d), sizeof(d)));
    const bool host = isHost(*e);
    QObject* object = host ? e->host->make(static_cast<QObject*>(parent))
                           : e->info->create(static_cast<QObject*>(parent), delegate ? &d : nullptr);
    if (!object)
        return nullptr;
    madeObjects().insert(object, TMadeObject{ e, !host });
    QObject::connect(object, &QObject::destroyed, [object] { madeObjects().remove(object); });
    if (host && d.destroyed)
        QObject::connect(object, &QObject::destroyed, [d] { d.destroyed(d.user); });
    return object;
}

void coDestroy(QtClientQObjectPtr object)
{
    delete static_cast<QObject*>(object);
}

// --- values ---

QVariant variantFor(const QJsonValue& json, const QMetaType type)
{
    if (type == QMetaType::fromType<QJsonValue>())
        return QVariant::fromValue(json);
    if (type.flags() & QMetaType::PointerToQObject)
        return QVariant(type);  // Qt objects travel as null.
    QVariant v = json.toVariant();
    if (type == QMetaType::fromType<QVariant>())
        return v;
    if (v.metaType() != type && !v.convert(type))
        return QVariant(type);
    return v;
}

QJsonValue jsonFor(const QVariant& v)
{
    if (!v.isValid())
        return QJsonValue();
    const QMetaType type = v.metaType();
    if (type.flags() & QMetaType::PointerToQObject)
        return QJsonValue(QJsonValue::Null);
    if (type.flags() & QMetaType::IsEnumeration) {
        QVariant i = v;
        if (i.convert(QMetaType::fromType<qint64>()))
            return QJsonValue(i.toLongLong());
    }
    return QJsonValue::fromVariant(v);
}

bool parseArgs(const char* args_json, QJsonArray& out)
{
    QJsonValue v;
    if (!parseJsonValue(args_json, v, false))
        return false;
    if (v.isNull() || v.isUndefined()) {
        out = {};
        return true;
    }
    if (!v.isArray())
        return false;
    out = v.toArray();
    return true;
}

int writeResult(const QJsonValue& value, char* result, size_t result_len)
{
    // 0 ran (the value as JSON, "" for none, NUL-terminated); the length is not returned.
    const QByteArray text = value.isUndefined() || value.isNull() ? QByteArray() : compactJson(value);
    if (result && result_len > 0)
        writeSizedOut(text, result, result_len);
    return 0;
}

/// Calls a meta method (slot / invokable) by name and argument count through qt_metacall.
int invokeMeta(QObject* o, const QByteArray& method, const QJsonArray& args, QJsonValue& ret)
{
    const QMetaObject* meta = o->metaObject();
    for (int i = meta->methodCount() - 1; i >= 0; --i) {
        const QMetaMethod m = meta->method(i);
        if (m.methodType() == QMetaMethod::Signal || m.name() != method || m.parameterCount() != args.size())
            continue;
        std::vector<QVariant> values;
        values.reserve(static_cast<size_t>(args.size()));
        std::vector<void*> argv(static_cast<size_t>(args.size()) + 1, nullptr);
        for (int p = 0; p < m.parameterCount(); ++p) {
            values.push_back(variantFor(args.at(p), m.parameterMetaType(p)));
            argv[static_cast<size_t>(p) + 1] = values.back().data();
        }
        QVariant result;
        if (m.returnMetaType().isValid() && m.returnMetaType() != QMetaType::fromType<void>()) {
            result = QVariant(m.returnMetaType());
            argv[0] = result.data();
        }
        o->qt_metacall(QMetaObject::InvokeMetaMethod, i, argv.data());
        ret = jsonFor(result);
        return 0;
    }
    // A property setter (virtual on model classes: the delegate runs first).
    if (method.startsWith("set") && method.size() > 3 && args.size() == 1) {
        QByteArray name = method.mid(3);
        name[0] = static_cast<char>(QChar::toLower(static_cast<char32_t>(name[0])));
        const int index = meta->indexOfProperty(name.constData());
        if (index >= 0 && meta->property(index).isWritable()) {
            const QMetaProperty p = meta->property(index);
            ret = QJsonValue();
            return p.write(o, variantFor(args.at(0), p.metaType())) ? 0 : -1;
        }
    }
    // Host classes: C++ members that are no moc methods (cip_object_host.cpp).
    if (cipInvokeHostMethod(o, method, args)) {
        ret = QJsonValue();
        return 0;
    }
    return -1;
}

int coInvoke(QtClientQObjectPtr object, const char* method, const char* args_json, char* result, size_t result_len)
{
    auto* o = static_cast<QObject*>(object);
    QJsonArray args;
    if (!o || !method || !parseArgs(args_json, args))
        return -1;
    QJsonValue ret;
    if (invokeMeta(o, QByteArray(method), args, ret) != 0)
        return -1;
    return writeResult(ret, result, result_len);
}

int coCallBase(QtClientQObjectPtr object, const char* method, const char* args_json, char* result, size_t result_len)
{
    auto* o = static_cast<QObject*>(object);
    QJsonArray args;
    if (!o || !method || !parseArgs(args_json, args))
        return -1;
    const TMadeObject* made = madeOf(o);
    if (made && made->model) {
        QJsonValue ret;
        if (made->entry->info->callBase(o, QByteArrayView(method), args, ret) != 0)
            return -1;
        return writeResult(ret, result, result_len);
    }
    // A host class: its own implementation (no delegate).
    return coInvoke(object, method, args_json, result, result_len);
}

int coSetValue(QtClientQObjectPtr object, const char* property, const char* value_json)
{
    auto* o = static_cast<QObject*>(object);
    QJsonValue value;
    if (!o || !property || !parseJsonValue(value_json, value, false))
        return -1;
    if (const TMadeObject* made = madeOf(o); made && made->model)
        return made->entry->info->setValue(o, QByteArrayView(property), value);
    const int index = o->metaObject()->indexOfProperty(property);
    if (index < 0)
        return -1;
    const QMetaProperty p = o->metaObject()->property(index);
    return p.isWritable() && p.write(o, variantFor(value, p.metaType())) ? 0 : -1;
}

int coGetValue(QtClientQObjectPtr object, const char* property, char* buf, size_t buf_len)
{
    auto* o = static_cast<QObject*>(object);
    if (!o || !property)
        return -1;
    const int index = o->metaObject()->indexOfProperty(property);
    if (index < 0)
        return -1;
    return writeSizedOut(compactJson(jsonFor(o->metaObject()->property(index).read(o))), buf, buf_len);
}

int coSetObject(QtClientQObjectPtr object, const char* property, QtClientQObjectPtr value)
{
    auto* o = static_cast<QObject*>(object);
    if (!o || !property)
        return -1;
    if (const TMadeObject* made = madeOf(o); made && made->model)
        return made->entry->info->setObject(o, QByteArrayView(property), static_cast<QObject*>(value));
    const int index = o->metaObject()->indexOfProperty(property);
    if (index < 0)
        return -1;
    const QMetaProperty p = o->metaObject()->property(index);
    QVariant v = QVariant::fromValue(static_cast<QObject*>(value));
    if (p.metaType() != v.metaType() && !v.convert(p.metaType()))
        return -1;
    return p.isWritable() && p.write(o, v) ? 0 : -1;
}

QtClientQObjectPtr coGetObject(QtClientQObjectPtr object, const char* property)
{
    auto* o = static_cast<QObject*>(object);
    if (!o || !property)
        return nullptr;
    const int index = o->metaObject()->indexOfProperty(property);
    if (index < 0)
        return nullptr;
    const QVariant v = o->metaObject()->property(index).read(o);
    return v.metaType().flags() & QMetaType::PointerToQObject ? qvariant_cast<QObject*>(v) : nullptr;
}

// --- signals ---

int signalIndex(const QObject* o, const QByteArray& name, const qsizetype argc)
{
    const QMetaObject* meta = o->metaObject();
    for (int i = meta->methodCount() - 1; i >= 0; --i) {
        const QMetaMethod m = meta->method(i);
        if (m.methodType() == QMetaMethod::Signal && m.name() == name && (argc < 0 || m.parameterCount() == argc))
            return i;
    }
    return -1;
}

int coEmitSignal(QtClientQObjectPtr object, const char* signal, const char* args_json)
{
    auto* o = static_cast<QObject*>(object);
    QJsonArray args;
    if (!o || !signal || !parseArgs(args_json, args))
        return -1;
    const int index = signalIndex(o, QByteArray(signal), args.size());
    if (index < 0)
        return -1;
    const QMetaMethod m = o->metaObject()->method(index);
    std::vector<QVariant> values;
    values.reserve(static_cast<size_t>(args.size()));
    std::vector<void*> argv(static_cast<size_t>(args.size()) + 1, nullptr);
    for (int p = 0; p < m.parameterCount(); ++p) {
        values.push_back(variantFor(args.at(p), m.parameterMetaType(p)));
        argv[static_cast<size_t>(p) + 1] = values.back().data();
    }
    const QMetaObject* owner = m.enclosingMetaObject();
    // moc lists a class's signals first: the method's local index is its signal index.
    QMetaObject::activate(o, owner, index - owner->methodOffset(), argv.data());
    return 0;
}

/// Receives one signal of one object and hands its arguments to the pack as JSON (a slot of
/// its own: qt_metacall answers the method index past the class's methods).
class TSignalRelay final : public QObject
{
public:
    TSignalRelay(QObject* sender, const QMetaMethod& signal, QtClientCipSignalFn fn, void* user)
        : QObject(sender)
        , m_signal(signal)
        , m_fn(fn)
        , m_user(user)
        , m_name(signal.name())
    {
    }

    bool connectTo(QObject* sender)
    {
        return QMetaObject::connect(sender, m_signal.methodIndex(), this, metaObject()->methodCount(), Qt::DirectConnection);
    }

    int qt_metacall(QMetaObject::Call call, int id, void** argv) override
    {
        id = QObject::qt_metacall(call, id, argv);
        if (id < 0 || call != QMetaObject::InvokeMetaMethod)
            return id;
        if (id == 0) {
            QJsonArray args;
            for (int p = 0; p < m_signal.parameterCount(); ++p)
                args.append(jsonFor(QVariant(m_signal.parameterMetaType(p), argv[p + 1])));
            const QByteArray json = QJsonDocument(args).toJson(QJsonDocument::Compact);
            m_fn(m_user, parent(), m_name.constData(), json.constData());
        }
        return id - 1;
    }

private:
    QMetaMethod m_signal;
    QtClientCipSignalFn m_fn;
    void* m_user;
    QByteArray m_name;
};

std::map<int, QPointer<TSignalRelay>>& relays()
{
    static std::map<int, QPointer<TSignalRelay>> r;
    return r;
}

int coSubscribeSignal(QtClientQObjectPtr object, const char* signal, QtClientCipSignalFn fn, void* user)
{
    auto* o = static_cast<QObject*>(object);
    if (!o || !signal || !fn)
        return 0;
    const int index = signalIndex(o, QByteArray(signal), -1);
    if (index < 0)
        return 0;
    auto* relay = new TSignalRelay(o, o->metaObject()->method(index), fn, user);
    if (!relay->connectTo(o)) {
        delete relay;
        return 0;
    }
    static int nextId = 1;
    const int id = nextId++;
    relays()[id] = relay;
    return id;
}

void coUnsubscribeSignal(int id)
{
    auto& r = relays();
    if (const auto it = r.find(id); it != r.end()) {
        delete it->second.data();
        r.erase(it);
    }
}

int coEnumValue(const char* class_name, const char* enum_name, const char* key)
{
    const TClassEntry* e = classOf(class_name);
    if (!e || !enum_name || !key)
        return -1;
    const QMetaObject* meta = e->info->meta;
    const int index = meta->indexOfEnumerator(enum_name);
    if (index < 0)
        return -1;
    bool ok = false;
    const int value = meta->enumerator(index).keyToValue(key, &ok);
    return ok ? value : -1;
}

int coSetHostController(int slot, QtClientQObjectPtr controller)
{
    return cipSetHostController(slot, static_cast<QObject*>(controller));
}

const QtClientCipObjectApiV1 g_cipObjectApi = {
    .struct_size = sizeof(QtClientCipObjectApiV1),
    .class_supported = &coSupported,
    .create = &coCreate,
    .destroy = &coDestroy,
    .invoke = &coInvoke,
    .call_base = &coCallBase,
    .set_value = &coSetValue,
    .get_value = &coGetValue,
    .set_object = &coSetObject,
    .get_object = &coGetObject,
    .emit_signal = &coEmitSignal,
    .subscribe_signal = &coSubscribeSignal,
    .unsubscribe_signal = &coUnsubscribeSignal,
    .enum_value = &coEnumValue,
    .set_host_controller = &coSetHostController,
};

} // namespace

const QtClientCipObjectApiV1* cipObjectApiV1()
{
    return &g_cipObjectApi;
}

} // namespace tibia::mods
