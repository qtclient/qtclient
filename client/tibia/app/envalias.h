#pragma once

#include <QByteArray>
#include <QString>

namespace tibia::app {

/// Read `primary`, optionally falling back to a second name (pack-local aliases).
inline QString envAlias(const char* primary, const char* legacy = nullptr)
{
    QByteArray v = qgetenv(primary);
    if (v.isEmpty() && legacy)
        v = qgetenv(legacy);
    return QString::fromUtf8(v);
}

inline bool envAliasIsSet(const char* primary, const char* legacy = nullptr)
{
    if (!qEnvironmentVariableIsEmpty(primary))
        return true;
    return legacy && !qEnvironmentVariableIsEmpty(legacy);
}

inline int envAliasInt(const char* primary, const char* legacy = nullptr, int def = 0)
{
    if (!qEnvironmentVariableIsEmpty(primary))
        return qEnvironmentVariableIntValue(primary);
    if (legacy && !qEnvironmentVariableIsEmpty(legacy))
        return qEnvironmentVariableIntValue(legacy);
    return def;
}

} // namespace tibia::app
