#pragma once

// AcGeTol source-level API aligned with the ObjectARX reference.
struct AcGeTol
{
    double m_equalPoint = 1.0e-10;
    double m_equalVector = 1.0e-10;

    double equalPoint() const { return m_equalPoint; }
    double equalVector() const { return m_equalVector; }
    void setEqualPoint(double value) { m_equalPoint = value; }
    void setEqualVector(double value) { m_equalVector = value; }
};
