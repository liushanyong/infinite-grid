#pragma once

// AcGeTol source-level API aligned with the ObjectARX reference.
class AcGeTol
{
    double m_equalPoint = 1.0e-10;
    double m_equalVector = 1.0e-10;

public:
    double equalPoint() const;
    double equalVector() const;
    void setEqualPoint(double value);
    void setEqualVector(double value);
};
