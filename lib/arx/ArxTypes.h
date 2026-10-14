#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// ObjectARX uses ACHAR for its Unicode-facing character API.
using ACHAR = wchar_t;
using AcString = std::basic_string<ACHAR>;
using AcStringView = std::basic_string_view<ACHAR>;
using AcUtf8String = std::string;

template <typename T>
using AcArray = std::vector<T>;

namespace Adesk
{
using Boolean = bool;
using Int8 = std::int8_t;
using UInt8 = std::uint8_t;
using Int16 = std::int16_t;
using UInt16 = std::uint16_t;
using Int32 = std::int32_t;
using UInt32 = std::uint32_t;
using Int64 = std::int64_t;
using UInt64 = std::uint64_t;
using LongPtr = std::intptr_t;
using ULongPtr = std::uintptr_t;
} // namespace Adesk

namespace arxcompat
{

inline AcUtf8String toUtf8(AcStringView text)
{
    AcUtf8String result;
    result.reserve(text.size());

    for (std::size_t index = 0; index < text.size(); ++index)
    {
        std::uint32_t codePoint = static_cast<std::uint32_t>(text[index]);
        if constexpr (sizeof(ACHAR) == 2)
        {
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF &&
                index + 1 < text.size())
            {
                const std::uint32_t low =
                    static_cast<std::uint32_t>(text[index + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF)
                {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) +
                                (low - 0xDC00);
                    ++index;
                }
                else
                {
                    codePoint = 0xFFFD;
                }
            }
            else if (codePoint >= 0xD800 && codePoint <= 0xDFFF)
            {
                codePoint = 0xFFFD;
            }
        }
        else if (codePoint > 0x10FFFF ||
                 (codePoint >= 0xD800 && codePoint <= 0xDFFF))
        {
            codePoint = 0xFFFD;
        }

        if (codePoint <= 0x7F)
        {
            result.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FF)
        {
            result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint <= 0xFFFF)
        {
            result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            result.push_back(static_cast<char>(
                0x80 | ((codePoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else
        {
            result.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            result.push_back(static_cast<char>(
                0x80 | ((codePoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(
                0x80 | ((codePoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    return result;
}

} // namespace arxcompat
