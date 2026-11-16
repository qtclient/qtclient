#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace tibia {
namespace shared {

class TJsonObjectValidator;
class TJsonArrayValidator;

/// CIP `shared::TJsonValueValidator`: a JSON value and whether every check on
/// the way to it passed. Checks narrow the validity in place; once invalid it
/// stays invalid.
class TJsonValueValidator
{
public:
    explicit TJsonValueValidator(const QJsonValue& value);
    TJsonValueValidator(const QJsonValue& value, bool valid);
    virtual ~TJsonValueValidator();

    TJsonValueValidator& isBool();
    TJsonValueValidator& isDouble();
    /// A number without a fractional part.
    TJsonValueValidator& isInt();
    TJsonValueValidator& isString();
    TJsonValueValidator& isObject();
    /// An array; unless `elementType` is Undefined, a non-empty one whose first
    /// element has that type.
    TJsonValueValidator& isArray(QJsonValue::Type elementType = QJsonValue::Undefined);
    TJsonValueValidator& isType(QJsonValue::Type type);
    /// The member `key` of this value, which must be an object.
    TJsonValueValidator value(const QString& key);
    bool isValid() const;

private:
    QJsonValue m_value;
    bool m_valid = true;
};

/// CIP `shared::TJsonObjectValidator`.
class TJsonObjectValidator
{
public:
    explicit TJsonObjectValidator(const QJsonObject& object);
    TJsonObjectValidator(const QJsonObject& object, bool valid);
    virtual ~TJsonObjectValidator();

    /// The member `key`; the object stays valid only if it has the member.
    TJsonValueValidator value(const QString& key);
    /// The member `key` as an array; the object stays valid only if it is one.
    TJsonArrayValidator array(const QString& key);
    bool isValid() const;

private:
    QJsonObject m_object;
    bool m_valid = true;
};

/// CIP `shared::TJsonArrayValidator`.
class TJsonArrayValidator
{
public:
    explicit TJsonArrayValidator(const QJsonArray& array);
    TJsonArrayValidator(const QJsonArray& array, bool valid);
    virtual ~TJsonArrayValidator();

    /// The first element; invalid for an empty array.
    TJsonValueValidator first() const;
    bool isValid() const;

private:
    QJsonArray m_array;
    bool m_valid = true;
};

}
}
