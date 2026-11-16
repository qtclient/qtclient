#include "shared/jsonvalidator.h"

namespace tibia::shared {

TJsonValueValidator::TJsonValueValidator(const QJsonValue& value)
    : m_value(value)
    , m_valid(true)
{
}

TJsonValueValidator::TJsonValueValidator(const QJsonValue& value, const bool valid)
    : m_value(value)
    , m_valid(valid)
{
}

TJsonValueValidator::~TJsonValueValidator() = default;

TJsonValueValidator& TJsonValueValidator::isBool() { return isType(QJsonValue::Bool); }

TJsonValueValidator& TJsonValueValidator::isDouble() { return isType(QJsonValue::Double); }

TJsonValueValidator& TJsonValueValidator::isInt()
{
    if (!m_valid) {
        m_valid = false;
        return *this;
    }
    m_valid = m_value.type() == QJsonValue::Double;
    if (!m_valid)
        return *this;
    const double number = m_value.toDouble(0.0);
    m_valid = number == double(int(number)) && m_valid;
    return *this;
}

TJsonValueValidator& TJsonValueValidator::isString() { return isType(QJsonValue::String); }

TJsonValueValidator& TJsonValueValidator::isObject() { return isType(QJsonValue::Object); }

TJsonValueValidator& TJsonValueValidator::isArray(const QJsonValue::Type elementType)
{
    m_valid = m_valid && m_value.type() == QJsonValue::Array;
    if (elementType != QJsonValue::Undefined) {
        const TJsonArrayValidator array(m_value.toArray(), m_valid);
        if (!m_valid) {
            m_valid = false;
        } else {
            TJsonValueValidator element = array.first();
            element.isType(elementType);
            m_valid = element.isValid();
        }
    }
    return *this;
}

TJsonValueValidator& TJsonValueValidator::isType(const QJsonValue::Type type)
{
    m_valid = m_valid && m_value.type() == type;
    return *this;
}

TJsonValueValidator TJsonValueValidator::value(const QString& key)
{
    if (m_valid) {
        m_valid = m_value.type() == QJsonValue::Object;
        if (m_valid)
            return TJsonValueValidator(m_value.toObject()[key], m_valid);
    } else {
        m_valid = false;
    }
    return TJsonValueValidator(QJsonValue(QJsonValue::Undefined), m_valid);
}

bool TJsonValueValidator::isValid() const { return m_valid; }

TJsonObjectValidator::TJsonObjectValidator(const QJsonObject& object)
    : m_object(object)
    , m_valid(true)
{
}

TJsonObjectValidator::TJsonObjectValidator(const QJsonObject& object, const bool valid)
    : m_object(object)
    , m_valid(valid)
{
}

TJsonObjectValidator::~TJsonObjectValidator() = default;

TJsonValueValidator TJsonObjectValidator::value(const QString& key)
{
    m_valid = m_valid && m_object.contains(key);
    return TJsonValueValidator(m_object[key], m_valid);
}

TJsonArrayValidator TJsonObjectValidator::array(const QString& key)
{
    if (m_valid) {
        m_valid = m_object.contains(key);
        TJsonValueValidator member(m_object[key], m_valid);
        member.isArray();
        m_valid = member.isValid();
    } else {
        m_valid = false;
    }
    return TJsonArrayValidator(m_object[key].toArray(), m_valid);
}

bool TJsonObjectValidator::isValid() const { return m_valid; }

TJsonArrayValidator::TJsonArrayValidator(const QJsonArray& array)
    : m_array(array)
    , m_valid(true)
{
}

TJsonArrayValidator::TJsonArrayValidator(const QJsonArray& array, const bool valid)
    : m_array(array)
    , m_valid(valid)
{
}

TJsonArrayValidator::~TJsonArrayValidator() = default;

TJsonValueValidator TJsonArrayValidator::first() const
{
    if (m_valid && !m_array.isEmpty())
        return TJsonValueValidator(m_array.at(0), m_valid);
    return TJsonValueValidator(QJsonValue(QJsonValue::Undefined), false);
}

bool TJsonArrayValidator::isValid() const { return m_valid; }

}
