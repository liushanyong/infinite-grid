#include "acge/AcGeContext.h"
#include "acge/AcGeLibVersion.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
void contextGlobals()
{
    // Default tolerance object is usable and positive.
    assert(AcGeContext::gTol.equalPoint() > 0.0);
    assert(AcGeContext::gTol.equalVector() > 0.0);

    // Default ortho-vector callback returns a unit orthogonal vector.
    AcGeVector3d ortho;
    AcGeContext::gOrthoVector(AcGeVector3d{0.0, 0.0, 1.0}, ortho);
    assert(std::abs(ortho.length() - 1.0) < 1e-12);
    assert(std::abs(ortho.dotProduct(AcGeVector3d{0.0, 0.0, 1.0})) < 1e-12);

    // The accessor mirrors ARX usage as a default tolerance.
    assert(&AcGeContext::gTolerance() == &AcGeContext::gTol);
    assert(AcGeContext::gErrorFunc == nullptr);
    assert(AcGeContext::gAllocMem == nullptr);

    // Callers may replace the hooks.
    AcGeContext::gErrorFunc = [] {};
    assert(AcGeContext::gErrorFunc != nullptr);
    AcGeContext::gErrorFunc = nullptr;
}

void libVersion()
{
    const AcGeLibVersion version(1, 2, 3, 4);
    assert(version.majorVersion() == 1);
    assert(version.minorVersion() == 2);
    assert(version.schemaVersion() == 3);
    assert(version.correctiveVersion() == 4);

    AcGeLibVersion mutableVersion;
    mutableVersion.setMajorVersion(5).setMinorVersion(6)
        .setSchemaVersion(7).setCorrectiveVersion(8);
    assert(mutableVersion.majorVersion() == 5);
    assert(mutableVersion.minorVersion() == 6);
    assert(mutableVersion.schemaVersion() == 7);
    assert(mutableVersion.correctiveVersion() == 8);

    assert(version == AcGeLibVersion(1, 2, 3, 4));
    assert(version != AcGeLibVersion(1, 2, 3, 5));
    assert(AcGeLibVersion(1, 0, 0, 0) < AcGeLibVersion(1, 1, 0, 0));
    assert(AcGeLibVersion(1, 1, 0, 0) < AcGeLibVersion(1, 1, 1, 0));
    assert(AcGeLibVersion(1, 1, 1, 0) < AcGeLibVersion(1, 1, 1, 1));
    assert(AcGeLibVersion(2, 0, 0, 0) >= AcGeLibVersion(1, 9, 9, 9));

    // Historical release constants.
    assert(AcGeLibVersion::kRelease0_95() == AcGeLibVersion(0, 95, 0, 0));
    assert(AcGeLibVersion::kReleaseSed() < AcGeLibVersion::kReleaseTah());
}
} // namespace

int main()
{
    contextGlobals();
    libVersion();
    std::cout << "AcGeContextTests passed\n";
    return 0;
}