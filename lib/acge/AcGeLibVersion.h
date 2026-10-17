#pragma once

#include "AcGePort.h"

#include <cstdint>

// Source-level AcGeLibVersion aligned with the ObjectARX reference
// (gelibver.h): a four-byte GeLib version stamp (major, minor, schema,
// corrective) with the documented comparison operators. The historical
// release constants (kRelease0_95 / kReleaseSed / kReleaseTah) map to the
// documented version triples.

class ACGE_PORT AcGeLibVersion
{
public:
    AcGeLibVersion() = default;
    AcGeLibVersion(std::uint8_t major, std::uint8_t minor,
                   std::uint8_t schema, std::uint8_t corrective)
        : m_major(major), m_minor(minor), m_schema(schema),
          m_corrective(corrective)
    {
    }

    // Historical release constants.
    static AcGeLibVersion kRelease0_95()
    {
        return {0, 95, 0, 0};
    }
    static AcGeLibVersion kReleaseSed()
    {
        return {1, 0, 0, 0};
    }
    static AcGeLibVersion kReleaseTah()
    {
        return {2, 0, 0, 0};
    }

    std::uint8_t majorVersion() const { return m_major; }
    std::uint8_t minorVersion() const { return m_minor; }
    std::uint8_t schemaVersion() const { return m_schema; }
    std::uint8_t correctiveVersion() const { return m_corrective; }

    AcGeLibVersion &setMajorVersion(std::uint8_t major)
    {
        m_major = major;
        return *this;
    }
    AcGeLibVersion &setMinorVersion(std::uint8_t minor)
    {
        m_minor = minor;
        return *this;
    }
    AcGeLibVersion &setSchemaVersion(std::uint8_t schema)
    {
        m_schema = schema;
        return *this;
    }
    AcGeLibVersion &setCorrectiveVersion(std::uint8_t corrective)
    {
        m_corrective = corrective;
        return *this;
    }

    bool operator==(const AcGeLibVersion &other) const
    {
        return m_major == other.m_major && m_minor == other.m_minor &&
               m_schema == other.m_schema &&
               m_corrective == other.m_corrective;
    }
    bool operator!=(const AcGeLibVersion &other) const
    {
        return !(*this == other);
    }
    // Lexicographic ordering major -> minor -> schema -> corrective.
    bool operator<(const AcGeLibVersion &other) const
    {
        if (m_major != other.m_major) return m_major < other.m_major;
        if (m_minor != other.m_minor) return m_minor < other.m_minor;
        if (m_schema != other.m_schema) return m_schema < other.m_schema;
        return m_corrective < other.m_corrective;
    }
    bool operator<=(const AcGeLibVersion &other) const
    {
        return *this < other || *this == other;
    }
    bool operator>(const AcGeLibVersion &other) const
    {
        return other < *this;
    }
    bool operator>=(const AcGeLibVersion &other) const
    {
        return other <= *this;
    }

private:
    std::uint8_t m_major = 0;
    std::uint8_t m_minor = 0;
    std::uint8_t m_schema = 0;
    std::uint8_t m_corrective = 0;
};