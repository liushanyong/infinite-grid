#include "acge/AcGeMatrix.h"

#include <cassert>
#include <cmath>

namespace
{
bool near(double a, double b, double tolerance = 1.0e-12)
{
    return std::abs(a - b) <= tolerance;
}
} // namespace

int main()
{
    AcGeMatrix3d translation =
        AcGeMatrix3d::setToTranslation({3.0, -2.0, 5.0});
    assert(near(translation(0, 3), 3.0));
    assert(near(translation(1, 3), -2.0));
    assert(near(translation(2, 3), 5.0));
    translation(0, 3) = 4.0;

    const AcGeMatrix3d scale = AcGeMatrix3d::setToScaling(2.0);
    AcGeMatrix3d product;
    product.setToProduct(translation, scale);
    const AcGePoint3d transformed =
        transformBy(AcGePoint3d{1.0, 2.0, 3.0}, product);
    assert(near(transformed.x, 6.0));
    assert(near(transformed.y, 2.0));
    assert(near(transformed.z, 11.0));

    AcGeMatrix3d post = translation;
    post.postMultBy(scale);
    assert(near(post(0, 3), 4.0));
    assert(near(post(0, 0), 2.0));

    AcGeMatrix3d pre = translation;
    pre.preMultBy(scale);
    assert(near(pre(0, 3), 8.0));

    const AcGeMatrix3d roundTrip = product * product.inverted();
    assert(near(roundTrip(0, 0), 1.0));
    assert(near(roundTrip(1, 1), 1.0));
    assert(near(roundTrip(2, 2), 1.0));
    assert(near(roundTrip(0, 3), 0.0));

    AcGeMatrix3d inverse;
    assert(product.inverse(inverse, 1.0e-12));
    const AcGeMatrix3d inverseRoundTrip = product * inverse;
    assert(near(inverseRoundTrip(0, 0), 1.0));
    assert(near(inverseRoundTrip(1, 1), 1.0));
    assert(near(inverseRoundTrip(2, 2), 1.0));
    assert(near(product.scale(), 2.0));

    AcGeMatrix3d singular;
    singular(0, 0) = 0.0;
    AcGeMatrix3d untouched = AcGeMatrix3d::setToTranslation({7.0, 8.0, 9.0});
    assert(!singular.inverse(untouched, 1.0e-12));
    assert(near(untouched(0, 3), 7.0));

    AcGeMatrix3d transposed = translation;
    transposed.transposeIt();
    assert(near(transposed(3, 0), 4.0));
    assert(near(translation.transposed()(3, 0), 4.0));

    AcGeMatrix3d identity;
    identity(0, 0) = 2.0;
    identity.setToIdentity();
    assert(near(identity(0, 0), 1.0));
    assert(near(identity(3, 3), 1.0));
}
