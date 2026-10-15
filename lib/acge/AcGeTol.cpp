#include "acge/AcGeTol.h"

double AcGeTol::equalPoint() const
{
    return m_equalPoint;
}

double AcGeTol::equalVector() const
{
    return m_equalVector;
}

void AcGeTol::setEqualPoint(double value)
{
    m_equalPoint = value;
}

void AcGeTol::setEqualVector(double value)
{
    m_equalVector = value;
}
