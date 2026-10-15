#pragma once

#include "AcDbPort.h"

// acdb::json — minimal JSON value tree + parser + writer for the AcDb
// document store.  Entities serialize their payloads through this (the
// store keeps class identity and handles in dedicated SQL columns; JSON
// carries geometry/strings/arrays).  Numbers are IEEE doubles: exact
// for every payload field (handles live in INTEGER columns, never in
// JSON, so the 2^53 concern does not arise).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace acdb
{
namespace json
{

class Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

class Value
{
public:
    Value() = default;
    Value(bool v) : value_(v) {}
    Value(double v) : value_(v) {}
    Value(int v) : value_(double(v)) {}
    Value(const char *v) : value_(std::string(v)) {}
    Value(std::string v) : value_(std::move(v)) {}
    Value(Array v) : value_(std::move(v)) {}
    Value(Object v) : value_(std::move(v)) {}

    bool isNull() const { return std::holds_alternative<std::monostate>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }
    bool isNumber() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isArray() const { return std::holds_alternative<Array>(value_); }
    bool isObject() const { return std::holds_alternative<Object>(value_); }

    bool asBool(bool fallback = false) const
    {
        return isBool() ? std::get<bool>(value_) : fallback;
    }
    double asNumber(double fallback = 0.0) const
    {
        return isNumber() ? std::get<double>(value_) : fallback;
    }
    const std::string &asString() const;
    const Array &asArray() const;
    const Object &asObject() const;

    // Object member access; nullptr when absent or not an object.
    const Value *find(const std::string &key) const;
    void set(const std::string &key, Value v);
    void push(Value v);

    std::string dump() const;

private:
    static void writeValue(std::string &out, std::monostate);
    static void writeValue(std::string &out, bool b);
    static void writeValue(std::string &out, double number);
    static void writeValue(std::string &out, const std::string &text);
    static void writeValue(std::string &out, const Array &items);
    static void writeValue(std::string &out, const Object &members);

    void dumpInto(std::string &out) const;

    std::variant<std::monostate, bool, double, std::string, Array,
                 Object>
        value_;
};

// Recursive-descent parser; nullptr-valued optional on syntax error.
std::optional<Value> parse(const std::string &text);

} // namespace json
} // namespace acdb
