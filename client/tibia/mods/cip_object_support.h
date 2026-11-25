#pragma once

// Host-internal support of the generated CipSoft object delegates (host API v9
// cip_object, tools/cip/gen_cip_objects.py): the JSON bridge of argument values, the
// pack delegate a generated subclass asks first, and the class table the ABI reads.

#include "qtclient/abi/cip_object_api.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QColor>
#include <QFlags>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>

#include <cstddef>
#include <type_traits>

namespace tibia::mods::cipobjects {

/// The pack side a generated delegate carries (QtClientCipObjectDelegateV1, copied).
class CipDelegateMixin
{
public:
    explicit CipDelegateMixin(const QtClientCipObjectDelegateV1* delegate)
    {
        if (delegate)
            m_delegate = *delegate;
    }
    virtual ~CipDelegateMixin() = default;

    const QtClientCipObjectDelegateV1& delegate() const { return m_delegate; }
    bool hasDelegate() const { return m_delegate.invoke != nullptr; }
    void clearDelegate() { m_delegate = {}; }

private:
    QtClientCipObjectDelegateV1 m_delegate{};
};

/// Asks the pack's delegate to handle `method` (mods/cip_object_abi.cpp). True when it did
/// (`result` then holds its return value).
bool cipAskDelegate(CipDelegateMixin* mixin, QObject* object, const char* method, const QJsonArray& args,
                    QJsonValue& result);
/// The delegate's `destroyed` (the last call with its user).
void cipDelegateDestroyed(CipDelegateMixin* mixin);
/// Emits property `property`'s NOTIFY signal on `object` (with the value when the signal
/// takes one).
void cipEmitNotify(QObject* object, const char* property);
/// A host class's C++ member that is no moc method, by name and argument count
/// (mods/cip_object_host.cpp). True when one ran.
bool cipInvokeHostMethod(QObject* object, const QByteArray& method, const QJsonArray& args);
/// cip_object set_host_controller (mods/cip_object_host.cpp): nonzero when the slot took it.
int cipSetHostController(int slot, QObject* controller);

template <class T>
struct IsQFlags : std::false_type
{};
template <class E>
struct IsQFlags<QFlags<E>> : std::true_type
{};

template <class T>
QJsonValue cipToJson(const T& v)
{
    if constexpr (std::is_same_v<T, bool>)
        return QJsonValue(v);
    else if constexpr (std::is_enum_v<T>)
        return QJsonValue(static_cast<qint64>(v));
    else if constexpr (IsQFlags<T>::value)
        return QJsonValue(static_cast<qint64>(v.toInt()));
    else if constexpr (std::is_integral_v<T>)
        return QJsonValue(static_cast<qint64>(v));
    else if constexpr (std::is_floating_point_v<T>)
        return QJsonValue(static_cast<double>(v));
    else if constexpr (std::is_pointer_v<T>)
        return QJsonValue(QJsonValue::Null);
    else if constexpr (std::is_same_v<T, QString>)
        return QJsonValue(v);
    else if constexpr (std::is_same_v<T, QJsonValue>)
        return v;
    else if constexpr (std::is_same_v<T, QJsonObject> || std::is_same_v<T, QJsonArray>)
        return QJsonValue(v);
    else if constexpr (std::is_same_v<T, QList<QObject*>>) {
        QJsonArray a;
        for (qsizetype i = 0; i < v.size(); ++i)
            a.append(QJsonValue(QJsonValue::Null));
        return a;
    } else if constexpr (std::is_same_v<T, QColor>)
        return QJsonValue(v.name(QColor::HexArgb));
    else if constexpr (std::is_same_v<T, QUrl>)
        return QJsonValue(v.toString());
    else
        return QJsonValue::fromVariant(QVariant::fromValue(v));
}

template <class T>
T cipFromJson(const QJsonValue& j)
{
    if constexpr (std::is_same_v<T, bool>)
        return j.isBool() ? j.toBool() : j.toInteger() != 0;
    else if constexpr (std::is_enum_v<T>)
        return static_cast<T>(j.toInteger());
    else if constexpr (IsQFlags<T>::value)
        return T::fromInt(static_cast<int>(j.toInteger()));
    else if constexpr (std::is_integral_v<T>)
        return static_cast<T>(j.isDouble() ? static_cast<qint64>(j.toDouble()) : j.toInteger());
    else if constexpr (std::is_floating_point_v<T>)
        return static_cast<T>(j.toDouble());
    else if constexpr (std::is_pointer_v<T>)
        return nullptr;
    else if constexpr (std::is_same_v<T, QString>)
        return j.toString();
    else if constexpr (std::is_same_v<T, QJsonValue>)
        return j;
    else if constexpr (std::is_same_v<T, QJsonObject>)
        return j.toObject();
    else if constexpr (std::is_same_v<T, QJsonArray>)
        return j.toArray();
    else if constexpr (std::is_same_v<T, QList<QObject*>>)
        return {};
    else if constexpr (std::is_same_v<T, QColor>)
        return QColor(j.toString());
    else if constexpr (std::is_same_v<T, QUrl>)
        return QUrl(j.toString());
    else {
        QVariant v = j.toVariant();
        if (v.metaType() != QMetaType::fromType<T>() && !v.convert(QMetaType::fromType<T>()))
            return T{};
        return v.template value<T>();
    }
}

/// Whether `j` can be read as a T (set_value refuses a mismatching type).
template <class T>
bool cipJsonFits(const QJsonValue& j)
{
    if constexpr (std::is_same_v<T, bool>)
        return j.isBool() || j.isDouble();
    else if constexpr (std::is_enum_v<T> || IsQFlags<T>::value || std::is_arithmetic_v<T>)
        return j.isDouble() || j.isBool();
    else if constexpr (std::is_pointer_v<T>)
        return j.isNull();
    else if constexpr (std::is_same_v<T, QString> || std::is_same_v<T, QColor> || std::is_same_v<T, QUrl>)
        return j.isString() || j.isNull();
    else if constexpr (std::is_same_v<T, QJsonObject> || std::is_same_v<T, QVariantMap>)
        return j.isObject() || j.isNull();
    else if constexpr (std::is_same_v<T, QJsonArray> || std::is_same_v<T, QVariantList> || std::is_same_v<T, QStringList>)
        return j.isArray() || j.isNull();
    else
        return true;
}

/// One generated CipSoft class (cip_objects_NN.cpp).
struct CipClassInfo
{
    /// "tibia::gamewindow::TFoo" (the contract's name) and "TFoo".
    const char* name;
    const char* shortName;
    const QMetaObject* meta;
    /// A delegate instance (model class).
    QObject* (*create)(QObject* parent, const QtClientCipObjectDelegateV1* delegate);
    /// The generated (CipSoft default) implementation of `name`; -1 unknown.
    int (*callBase)(QObject* object, QByteArrayView name, const QJsonArray& args, QJsonValue& ret);
    /// Writes a property's storage; -1 unknown property or bad value.
    int (*setValue)(QObject* object, QByteArrayView name, const QJsonValue& value);
    int (*setObject)(QObject* object, QByteArrayView name, QObject* value);
};

struct CipClassChunk
{
    const CipClassInfo* rows;
    const size_t* size;
};

/// A host implementation of a CipSoft class (cip_objects_host.cpp); `make` null when none.
struct CipHostClass
{
    const char* name;
    QObject* (*make)(QObject* parent);
    bool exists;
};

extern const CipClassChunk kChunks[];
extern const size_t kChunkCount;
extern const CipHostClass kHostClasses[];
extern const size_t kHostClassCount;

} // namespace tibia::mods::cipobjects
