#include <gtest/gtest.h>

#include "shared/jsonvalidator.h"

#include <QJsonDocument>

using namespace tibia::shared;

namespace {

QJsonObject object(const char* json) { return QJsonDocument::fromJson(json).object(); }

}

TEST(JsonValidator, ValueTypes)
{
    TJsonObjectValidator validator(object(R"({"i": 3, "d": 1.5, "s": "x", "b": true, "o": {"k": 1}, "a": [1, 2]})"));
    EXPECT_TRUE(validator.value("i").isInt().isValid());
    EXPECT_FALSE(validator.value("d").isInt().isValid());
    EXPECT_TRUE(validator.isValid());
    EXPECT_TRUE(validator.value("d").isDouble().isValid());
    EXPECT_TRUE(validator.value("s").isString().isValid());
    EXPECT_TRUE(validator.value("b").isBool().isValid());
    EXPECT_TRUE(validator.value("o").isObject().isValid());
    EXPECT_TRUE(validator.value("o").value("k").isInt().isValid());
    EXPECT_TRUE(validator.value("a").isArray(QJsonValue::Double).isValid());
    EXPECT_FALSE(validator.value("a").isArray(QJsonValue::String).isValid());
}

TEST(JsonValidator, AMissingMemberInvalidatesTheObject)
{
    TJsonObjectValidator validator(object(R"({"i": 3})"));
    EXPECT_FALSE(validator.value("missing").isValid());
    EXPECT_FALSE(validator.isValid());
    // Once invalid, every later check fails too.
    EXPECT_FALSE(validator.value("i").isInt().isValid());
}

TEST(JsonValidator, NestedValueOfANonObjectIsInvalid)
{
    TJsonObjectValidator validator(object(R"({"i": 3})"));
    EXPECT_FALSE(validator.value("i").value("k").isValid());
}

TEST(JsonValidator, Arrays)
{
    TJsonObjectValidator validator(object(R"({"a": ["x"], "e": [], "n": 1})"));
    EXPECT_TRUE(validator.array("a").first().isString().isValid());
    EXPECT_FALSE(validator.array("e").first().isValid());
    EXPECT_TRUE(validator.isValid());
    EXPECT_FALSE(validator.array("n").isValid());
    EXPECT_FALSE(validator.isValid());
    EXPECT_FALSE(TJsonValueValidator(QJsonArray()).isArray(QJsonValue::String).isValid());
}
