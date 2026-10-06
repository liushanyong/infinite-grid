#pragma once

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
    const std::string &asString() const
    {
        static const std::string kEmpty;
        return isString() ? std::get<std::string>(value_) : kEmpty;
    }
    const Array &asArray() const
    {
        static const Array kEmpty;
        return isArray() ? std::get<Array>(value_) : kEmpty;
    }
    const Object &asObject() const
    {
        static const Object kEmpty;
        return isObject() ? std::get<Object>(value_) : kEmpty;
    }

    // Object member access; nullptr when absent or not an object.
    const Value *find(const std::string &key) const
    {
        if (!isObject())
            return nullptr;
        for (const auto &member : std::get<Object>(value_))
            if (member.first == key)
                return &member.second;
        return nullptr;
    }
    void set(const std::string &key, Value v)
    {
        if (!isObject())
            value_ = Object{};
        auto &members = std::get<Object>(value_);
        for (auto &member : members)
            if (member.first == key)
            {
                member.second = std::move(v);
                return;
            }
        members.emplace_back(key, std::move(v));
    }
    void push(Value v)
    {
        if (!isArray())
            value_ = Array{};
        std::get<Array>(value_).push_back(std::move(v));
    }

    std::string dump() const
    {
        std::string out;
        std::visit(
            [&](const auto &alternative) { writeValue(out, alternative); },
            value_);
        return out;
    }

private:
    static void writeValue(std::string &out, std::monostate)
    {
        out += "null";
    }
    static void writeValue(std::string &out, bool b)
    {
        out += b ? "true" : "false";
    }
    static void writeValue(std::string &out, double number)
    {
        char buffer[64];
        if (std::isfinite(number))
            std::snprintf(buffer, sizeof(buffer), "%.17g", number);
        else
            std::snprintf(buffer, sizeof(buffer), "0");
        out += buffer;
    }
    static void writeValue(std::string &out, const std::string &text)
    {
        out += '"';
        for (char c : text)
        {
            switch (c)
            {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char escape[8];
                    std::snprintf(escape, sizeof(escape), "\\u%04x",
                                  static_cast<unsigned>(c) & 0xff);
                    out += escape;
                }
                else
                    out += c;
            }
        }
        out += '"';
    }
    static void writeValue(std::string &out, const Array &items)
    {
        out += '[';
        bool first = true;
        for (const Value &item : items)
        {
            if (!first)
                out += ',';
            first = false;
            item.dumpInto(out);
        }
        out += ']';
    }
    static void writeValue(std::string &out, const Object &members)
    {
        out += '{';
        bool first = true;
        for (const auto &member : members)
        {
            if (!first)
                out += ',';
            first = false;
            writeValue(out, member.first);
            out += ':';
            member.second.dumpInto(out);
        }
        out += '}';
    }

    void dumpInto(std::string &out) const
    {
        std::visit(
            [&](const auto &alternative) { writeValue(out, alternative); },
            value_);
    }

    std::variant<std::monostate, bool, double, std::string, Array,
                 Object>
        value_;
};

// Recursive-descent parser; nullptr-valued optional on syntax error.
inline std::optional<Value> parse(const std::string &text)
{
    std::size_t pos = 0;
    auto skip = [&] {
        while (pos < text.size() &&
               (text[pos] == ' ' || text[pos] == '\t' ||
                text[pos] == '\n' || text[pos] == '\r'))
            ++pos;
    };
    std::function<bool(Value &)> parseValue = [&](Value &out) -> bool {
        skip();
        if (pos >= text.size())
            return false;
        const char c = text[pos];
        if (c == '{')
        {
            ++pos;
            out = Object{};
            skip();
            if (pos < text.size() && text[pos] == '}')
            {
                ++pos;
                return true;
            }
            while (true)
            {
                skip();
                Value key;
                if (pos >= text.size() || text[pos] != '"' ||
                    !parseValue(key))
                    return false;
                skip();
                if (pos >= text.size() || text[pos] != ':')
                    return false;
                ++pos;
                Value member;
                if (!parseValue(member))
                    return false;
                out.set(key.asString(), std::move(member));
                skip();
                if (pos < text.size() && text[pos] == ',')
                {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text[pos] == '}')
                {
                    ++pos;
                    return true;
                }
                return false;
            }
        }
        if (c == '[')
        {
            ++pos;
            out = Array{};
            skip();
            if (pos < text.size() && text[pos] == ']')
            {
                ++pos;
                return true;
            }
            while (true)
            {
                Value item;
                if (!parseValue(item))
                    return false;
                out.push(std::move(item));
                skip();
                if (pos < text.size() && text[pos] == ',')
                {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text[pos] == ']')
                {
                    ++pos;
                    return true;
                }
                return false;
            }
        }
        if (c == '"')
        {
            ++pos;
            std::string text2;
            while (pos < text.size() && text[pos] != '"')
            {
                if (text[pos] == '\\' && pos + 1 < text.size())
                {
                    ++pos;
                    switch (text[pos])
                    {
                    case '"': text2 += '"'; break;
                    case '\\': text2 += '\\'; break;
                    case '/': text2 += '/'; break;
                    case 'n': text2 += '\n'; break;
                    case 'r': text2 += '\r'; break;
                    case 't': text2 += '\t'; break;
                    case 'u':
                    {
                        if (pos + 4 >= text.size())
                            return false;
                        const unsigned code =
                            std::strtoul(text.substr(pos + 1, 4).c_str(),
                                         nullptr, 16);
                        text2 += static_cast<char>(code & 0xff);
                        pos += 4;
                        break;
                    }
                    default: return false;
                    }
                    ++pos;
                }
                else
                    text2 += text[pos++];
            }
            if (pos >= text.size())
                return false;
            ++pos;
            out = Value(std::move(text2));
            return true;
        }
        if (text.compare(pos, 4, "true") == 0)
        {
            pos += 4;
            out = Value(true);
            return true;
        }
        if (text.compare(pos, 5, "false") == 0)
        {
            pos += 5;
            out = Value(false);
            return true;
        }
        if (text.compare(pos, 4, "null") == 0)
        {
            pos += 4;
            out = Value();
            return true;
        }
        // Number.
        const std::size_t start = pos;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+'))
            ++pos;
        while (pos < text.size() &&
               ((text[pos] >= '0' && text[pos] <= '9') ||
                text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' ||
                text[pos] == '-' || text[pos] == '+'))
            ++pos;
        if (pos == start)
            return false;
        out = Value(std::strtod(text.substr(start, pos - start).c_str(),
                                nullptr));
        return true;
    };

    Value root;
    if (!parseValue(root))
        return std::nullopt;
    return root;
}

} // namespace json
} // namespace acdb
