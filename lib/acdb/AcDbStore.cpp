#include "acdb/AcDbStore.h"

#include <SQLiteCpp/SQLiteCpp.h>

#include "acdb/AcDbJson.h"

namespace acdb
{
namespace
{

using json::Value;

// ---- small JSON helpers -------------------------------------------------

Value point2ToJson(const AcGePoint2d &p)
{
    return Value(json::Array{Value(p.x), Value(p.y)});
}

Value point3ToJson(const AcGePoint3d &p)
{
    return Value(json::Array{Value(p.x), Value(p.y), Value(p.z)});
}

Value vec3ToJson(const AcGeVector3d &v)
{
    return Value(json::Array{Value(v.x), Value(v.y), Value(v.z)});
}

AcGePoint3d jsonToPoint3(const Value &v, const AcGePoint3d &fallback = {})
{
    const json::Array &a = v.asArray();
    if (a.size() < 3)
        return fallback;
    return AcGePoint3d(a[0].asNumber(), a[1].asNumber(),
                       a[2].asNumber());
}

AcGeVector3d jsonToVec3(const Value &v,
                        const AcGeVector3d &fallback = {0.0, 0.0, 1.0})
{
    const json::Array &a = v.asArray();
    if (a.size() < 3)
        return fallback;
    return AcGeVector3d(a[0].asNumber(), a[1].asNumber(),
                        a[2].asNumber());
}

Value numbersToJson(const double *values, std::size_t count)
{
    Value out(json::Array{});
    for (std::size_t i = 0; i < count; ++i)
        out.push(Value(values[i]));
    return out;
}

Value colorToJson(const glm::vec4 &color)
{
    return Value(json::Array{Value(color.r), Value(color.g),
                             Value(color.b), Value(color.a)});
}

glm::vec4 jsonToColor(const Value &v)
{
    const json::Array &a = v.asArray();
    if (a.size() < 4)
        return glm::vec4(1.0f);
    return glm::vec4(float(a[0].asNumber(1.0)), float(a[1].asNumber(1.0)),
                     float(a[2].asNumber(1.0)), float(a[3].asNumber(1.0)));
}

Value pointsToJson(const std::vector<AcGePoint3d> &points)
{
    Value out(json::Array{});
    for (const AcGePoint3d &p : points)
        out.push(point3ToJson(p));
    return out;
}

std::vector<AcGePoint3d> jsonToPoints3(const Value &v)
{
    std::vector<AcGePoint3d> out;
    for (const Value &item : v.asArray())
        out.push_back(jsonToPoint3(item));
    return out;
}

Value doublesToJson(const std::vector<double> &values)
{
    Value out(json::Array{});
    for (double d : values)
        out.push(Value(d));
    return out;
}

std::vector<double> jsonToDoubles(const Value &v)
{
    std::vector<double> out;
    for (const Value &item : v.asArray())
        out.push_back(item.asNumber());
    return out;
}

// ---- common properties ---------------------------------------------------

Value commonToJson(const AcDbEntity &entity)
{
    Value out(json::Object{});
    out.set("name", entity.name);
    out.set("layer", entity.layer);
    out.set("color", colorToJson(entity.color));
    out.set("lineType", entity.lineType);
    out.set("lineWeight", entity.lineWeight);
    out.set("visible", entity.visible);
    return out;
}

void commonFromJson(const Value &value, AcDbEntity &entity)
{
    if (const Value *v = value.find("name"))
        entity.name = v->asString();
    if (const Value *v = value.find("layer"))
        entity.layer = v->asString();
    if (const Value *v = value.find("color"))
        entity.color = jsonToColor(*v);
    if (const Value *v = value.find("lineType"))
        entity.lineType = v->asString();
    if (const Value *v = value.find("lineWeight"))
        entity.lineWeight = v->asNumber();
    if (const Value *v = value.find("visible"))
        entity.visible = v->asBool(true);
}

// ---- entity payload codec (one branch per variant alternative) ----------

Value encodePayload(const AcDbLine &e)
{
    Value out(json::Object{});
    out.set("start", point3ToJson(e.start));
    out.set("end", point3ToJson(e.end));
    out.set("thickness", e.thickness);
    out.set("extrusion", vec3ToJson(e.extrusion));
    return out;
}

Value encodePayload(const AcDbArc &e)
{
    Value out(json::Object{});
    out.set("center", point3ToJson(e.center));
    out.set("radius", e.radius);
    out.set("startAngle", e.startAngle);
    out.set("endAngle", e.endAngle);
    out.set("normal", vec3ToJson(e.normal));
    out.set("thickness", e.thickness);
    return out;
}

Value encodePayload(const AcDbCircle &e)
{
    Value out(json::Object{});
    out.set("center", point3ToJson(e.center));
    out.set("radius", e.radius);
    out.set("normal", vec3ToJson(e.normal));
    out.set("thickness", e.thickness);
    return out;
}

Value encodePayload(const AcDbEllipse &e)
{
    Value out(json::Object{});
    out.set("center", point3ToJson(e.center));
    out.set("majorAxis", vec3ToJson(e.majorAxis));
    out.set("radiusRatio", e.radiusRatio);
    out.set("startParameter", e.startParameter);
    out.set("endParameter", e.endParameter);
    out.set("normal", vec3ToJson(e.normal));
    return out;
}

Value encodePayload(const AcDbPoint &e)
{
    Value out(json::Object{});
    out.set("location", point3ToJson(e.location));
    return out;
}

Value encodePayload(const AcDbRay &e)
{
    Value out(json::Object{});
    out.set("start", point3ToJson(e.start));
    out.set("direction", vec3ToJson(e.direction));
    return out;
}

Value encodePayload(const AcDbXline &e)
{
    Value out(json::Object{});
    out.set("point", point3ToJson(e.point));
    out.set("direction", vec3ToJson(e.direction));
    return out;
}

Value encodePayload(const AcDbSolid &e)
{
    Value out(json::Object{});
    out.set("firstCorner", point3ToJson(e.firstCorner));
    out.set("secondCorner", point3ToJson(e.secondCorner));
    out.set("thirdCorner", point3ToJson(e.thirdCorner));
    out.set("fourthCorner", point3ToJson(e.fourthCorner));
    return out;
}

Value encodePayload(const AcDbHatch &e)
{
    Value out(json::Object{});
    out.set("outerLoop", pointsToJson(e.outerLoop));
    Value inner(json::Array{});
    for (const std::vector<AcGePoint3d> &loop : e.innerLoops)
        inner.push(pointsToJson(loop));
    out.set("innerLoops", std::move(inner));
    out.set("solidFill", e.solidFill);
    out.set("patternName", e.patternName);
    out.set("patternScale", e.patternScale);
    out.set("patternAngle", e.patternAngle);
    return out;
}

Value encodePayload(const AcDb3dPolyline &e)
{
    Value out(json::Object{});
    out.set("vertices", pointsToJson(e.vertices));
    out.set("bulges", doublesToJson(e.bulges));
    out.set("closed", e.closed);
    out.set("normal", vec3ToJson(e.normal));
    out.set("thickness", e.thickness);
    return out;
}

Value encodePayload(const AcDbPolyline &e)
{
    Value out(json::Object{});
    Value vertices(json::Array{});
    for (const AcGePoint2d &p : e.vertices)
        vertices.push(point2ToJson(p));
    out.set("vertices", std::move(vertices));
    out.set("bulges", doublesToJson(e.bulges));
    out.set("elevation", e.elevation);
    out.set("thickness", e.thickness);
    out.set("closed", e.closed);
    out.set("normal", vec3ToJson(e.normal));
    return out;
}

Value encodePayload(const AcDbSpline &e)
{
    Value out(json::Object{});
    out.set("degree", e.degree);
    out.set("closed", e.closed);
    out.set("controlPoints", pointsToJson(e.controlPoints));
    out.set("fitPoints", pointsToJson(e.fitPoints));
    out.set("knots", doublesToJson(e.knots));
    return out;
}

Value encodePayload(const AcDbText &e)
{
    Value out(json::Object{});
    out.set("insertion", point3ToJson(e.insertion));
    out.set("height", e.height);
    out.set("rotation", e.rotation);
    out.set("text", e.text);
    out.set("styleName", e.styleName);
    return out;
}

Value encodePayload(const AcDbMText &e)
{
    Value out(json::Object{});
    out.set("insertion", point3ToJson(e.insertion));
    out.set("direction", vec3ToJson(e.direction));
    out.set("width", e.width);
    out.set("height", e.height);
    out.set("text", e.text);
    return out;
}

Value encodePayload(const AcDbMline &e)
{
    Value out(json::Object{});
    out.set("vertices", pointsToJson(e.vertices));
    out.set("scale", vec3ToJson(e.scale));
    out.set("closed", e.closed);
    return out;
}

Value encodePayload(const AcDbPolyFaceMesh &e)
{
    Value out(json::Object{});
    Value positions(json::Array{});
    for (const glm::dvec3 &p : e.geometry.positions)
        positions.push(Value(json::Array{Value(p.x), Value(p.y),
                                         Value(p.z)}));
    out.set("positions", std::move(positions));
    Value normals(json::Array{});
    for (const glm::vec3 &n : e.geometry.normals)
        normals.push(Value(json::Array{Value(n.x), Value(n.y),
                                       Value(n.z)}));
    out.set("normals", std::move(normals));
    Value uvs(json::Array{});
    for (const glm::vec2 &uv : e.geometry.uvs)
        uvs.push(Value(json::Array{Value(uv.x), Value(uv.y)}));
    out.set("uvs", std::move(uvs));
    out.set("indices", doublesToJson({e.geometry.indices.begin(),
                                      e.geometry.indices.end()}));
    out.set("style", int(e.style));
    out.set("material",
            colorToJson(e.material.baseColorFactor));
    return out;
}

Value encodePayload(const AcDb3dSolid &e)
{
    Value out(json::Object{});
    out.set("vertices", pointsToJson(e.vertices));
    out.set("indices", doublesToJson({e.indices.begin(),
                                      e.indices.end()}));
    out.set("renderClass", int(e.renderClass));
    return out;
}

Value encodePayload(const AcDbLight &e)
{
    Value out(json::Object{});
    out.set("type", int(e.type));
    out.set("position", point3ToJson(e.position));
    out.set("target", vec3ToJson(e.target));
    out.set("color", Value(json::Array{Value(e.color.r), Value(e.color.g),
                                       Value(e.color.b)}));
    out.set("intensity", e.intensity);
    out.set("range", e.range);
    out.set("innerConeAngle", e.innerConeAngle);
    out.set("outerConeAngle", e.outerConeAngle);
    return out;
}

Value encodePayload(const AcDbBlockReference &e)
{
    Value out(json::Object{});
    out.set("blockTableRecordName", e.blockTableRecordName);
    out.set("position", Value(json::Array{Value(e.position.x),
                                          Value(e.position.y),
                                          Value(e.position.z)}));
    out.set("rotation", e.rotation);
    out.set("scale", Value(json::Array{Value(e.scale.x), Value(e.scale.y),
                                       Value(e.scale.z)}));
    out.set("normal", Value(json::Array{Value(e.normal.x),
                                        Value(e.normal.y),
                                        Value(e.normal.z)}));
    return out;
}

struct PayloadEncoder
{
    Value operator()(const AcDbLine &e) const { return encodePayload(e); }
    Value operator()(const AcDbArc &e) const { return encodePayload(e); }
    Value operator()(const AcDbCircle &e) const
    {
        return encodePayload(e);
    }
    Value operator()(const AcDbEllipse &e) const
    {
        return encodePayload(e);
    }
    Value operator()(const AcDbPoint &e) const { return encodePayload(e); }
    Value operator()(const AcDbRay &e) const { return encodePayload(e); }
    Value operator()(const AcDbXline &e) const { return encodePayload(e); }
    Value operator()(const AcDbSolid &e) const { return encodePayload(e); }
    Value operator()(const AcDbHatch &e) const { return encodePayload(e); }
    Value operator()(const AcDb3dPolyline &e) const
    {
        return encodePayload(e);
    }
    Value operator()(const AcDbPolyline &e) const
    {
        return encodePayload(e);
    }
    Value operator()(const AcDbSpline &e) const { return encodePayload(e); }
    Value operator()(const AcDbText &e) const { return encodePayload(e); }
    Value operator()(const AcDbMText &e) const { return encodePayload(e); }
    Value operator()(const AcDbMline &e) const { return encodePayload(e); }
    Value operator()(const AcDbPolyFaceMesh &e) const
    {
        return encodePayload(e);
    }
    Value operator()(const AcDb3dSolid &e) const
    {
        return encodePayload(e);
    }
    Value operator()(const AcDbLight &e) const { return encodePayload(e); }
    Value operator()(const AcDbBlockReference &e) const
    {
        return encodePayload(e);
    }
};

const char *variantClassName(const AcDbEntityVariant &payload)
{
    switch (payload.index())
    {
    case 0: return "AcDbLine";
    case 1: return "AcDbArc";
    case 2: return "AcDbCircle";
    case 3: return "AcDbEllipse";
    case 4: return "AcDbPoint";
    case 5: return "AcDbRay";
    case 6: return "AcDbXline";
    case 7: return "AcDbSolid";
    case 8: return "AcDbHatch";
    case 9: return "AcDb3dPolyline";
    case 10: return "AcDbPolyline";
    case 11: return "AcDbSpline";
    case 12: return "AcDbText";
    case 13: return "AcDbMText";
    case 14: return "AcDbMline";
    case 15: return "AcDbPolyFaceMesh";
    case 16: return "AcDb3dSolid";
    case 17: return "AcDbLight";
    case 18: return "AcDbBlockReference";
    }
    return "AcDbUnknown";
}

template <typename Payload>
bool decodeInto(const Value &commonData, const Value &data,
                AcDbEntityVariant &out)
{
    Payload payload;
    commonFromJson(commonData, payload.common);
    if constexpr (std::is_same_v<Payload, AcDbLine>)
    {
        payload.start = jsonToPoint3(data.find("start") ? *data.find("start")
                                                        : Value());
        payload.end = jsonToPoint3(data.find("end") ? *data.find("end")
                                                    : Value());
        if (const Value *v = data.find("thickness"))
            payload.thickness = v->asNumber();
        if (const Value *v = data.find("extrusion"))
            payload.extrusion = jsonToVec3(*v);
    }
    else if constexpr (std::is_same_v<Payload, AcDbArc>)
    {
        payload.center = jsonToPoint3(data.find("center") ? *data.find("center") : Value());
        if (const Value *v = data.find("radius"))
            payload.radius = v->asNumber();
        if (const Value *v = data.find("startAngle"))
            payload.startAngle = v->asNumber();
        if (const Value *v = data.find("endAngle"))
            payload.endAngle = v->asNumber();
        if (const Value *v = data.find("normal"))
            payload.normal = jsonToVec3(*v);
        if (const Value *v = data.find("thickness"))
            payload.thickness = v->asNumber();
    }
    else if constexpr (std::is_same_v<Payload, AcDbCircle>)
    {
        payload.center = jsonToPoint3(data.find("center") ? *data.find("center") : Value());
        if (const Value *v = data.find("radius"))
            payload.radius = v->asNumber();
        if (const Value *v = data.find("normal"))
            payload.normal = jsonToVec3(*v);
        if (const Value *v = data.find("thickness"))
            payload.thickness = v->asNumber();
    }
    else if constexpr (std::is_same_v<Payload, AcDbEllipse>)
    {
        payload.center = jsonToPoint3(data.find("center") ? *data.find("center") : Value());
        if (const Value *v = data.find("majorAxis"))
            payload.majorAxis = jsonToVec3(*v, {1.0, 0.0, 0.0});
        if (const Value *v = data.find("radiusRatio"))
            payload.radiusRatio = v->asNumber(1.0);
        if (const Value *v = data.find("startParameter"))
            payload.startParameter = v->asNumber();
        if (const Value *v = data.find("endParameter"))
            payload.endParameter = v->asNumber();
        if (const Value *v = data.find("normal"))
            payload.normal = jsonToVec3(*v);
    }
    else if constexpr (std::is_same_v<Payload, AcDbPoint>)
    {
        payload.location = jsonToPoint3(data.find("location") ? *data.find("location") : Value());
    }
    else if constexpr (std::is_same_v<Payload, AcDbRay>)
    {
        payload.start = jsonToPoint3(data.find("start") ? *data.find("start") : Value());
        if (const Value *v = data.find("direction"))
            payload.direction = jsonToVec3(*v, {1.0, 0.0, 0.0});
    }
    else if constexpr (std::is_same_v<Payload, AcDbXline>)
    {
        payload.point = jsonToPoint3(data.find("point") ? *data.find("point") : Value());
        if (const Value *v = data.find("direction"))
            payload.direction = jsonToVec3(*v, {1.0, 0.0, 0.0});
    }
    else if constexpr (std::is_same_v<Payload, AcDbSolid>)
    {
        auto corner = [&](const char *key, AcGePoint3d &target) {
            if (const Value *v = data.find(key))
                target = jsonToPoint3(*v);
        };
        corner("firstCorner", payload.firstCorner);
        corner("secondCorner", payload.secondCorner);
        corner("thirdCorner", payload.thirdCorner);
        corner("fourthCorner", payload.fourthCorner);
    }
    else if constexpr (std::is_same_v<Payload, AcDbHatch>)
    {
        if (const Value *v = data.find("outerLoop"))
            payload.outerLoop = jsonToPoints3(*v);
        if (const Value *v = data.find("innerLoops"))
            for (const Value &loop : v->asArray())
                payload.innerLoops.push_back(jsonToPoints3(loop));
        if (const Value *v = data.find("solidFill"))
            payload.solidFill = v->asBool(true);
        if (const Value *v = data.find("patternName"))
            payload.patternName = v->asString();
        if (const Value *v = data.find("patternScale"))
            payload.patternScale = v->asNumber(1.0);
        if (const Value *v = data.find("patternAngle"))
            payload.patternAngle = v->asNumber();
    }
    else if constexpr (std::is_same_v<Payload, AcDb3dPolyline>)
    {
        if (const Value *v = data.find("vertices"))
            payload.vertices = jsonToPoints3(*v);
        if (const Value *v = data.find("bulges"))
            payload.bulges = jsonToDoubles(*v);
        if (const Value *v = data.find("closed"))
            payload.closed = v->asBool();
        if (const Value *v = data.find("normal"))
            payload.normal = jsonToVec3(*v);
        if (const Value *v = data.find("thickness"))
            payload.thickness = v->asNumber();
    }
    else if constexpr (std::is_same_v<Payload, AcDbPolyline>)
    {
        if (const Value *v = data.find("vertices"))
            for (const Value &item : v->asArray())
            {
                const json::Array &a = item.asArray();
                if (a.size() >= 2)
                    payload.vertices.push_back(
                        AcGePoint2d(a[0].asNumber(), a[1].asNumber()));
            }
        if (const Value *v = data.find("bulges"))
            payload.bulges = jsonToDoubles(*v);
        if (const Value *v = data.find("elevation"))
            payload.elevation = v->asNumber();
        if (const Value *v = data.find("thickness"))
            payload.thickness = v->asNumber();
        if (const Value *v = data.find("closed"))
            payload.closed = v->asBool();
        if (const Value *v = data.find("normal"))
            payload.normal = jsonToVec3(*v);
    }
    else if constexpr (std::is_same_v<Payload, AcDbSpline>)
    {
        if (const Value *v = data.find("degree"))
            payload.degree = int(v->asNumber(3.0));
        if (const Value *v = data.find("closed"))
            payload.closed = v->asBool();
        if (const Value *v = data.find("controlPoints"))
            payload.controlPoints = jsonToPoints3(*v);
        if (const Value *v = data.find("fitPoints"))
            payload.fitPoints = jsonToPoints3(*v);
        if (const Value *v = data.find("knots"))
            payload.knots = jsonToDoubles(*v);
    }
    else if constexpr (std::is_same_v<Payload, AcDbText>)
    {
        payload.insertion = jsonToPoint3(data.find("insertion") ? *data.find("insertion") : Value());
        if (const Value *v = data.find("height"))
            payload.height = v->asNumber(1.0);
        if (const Value *v = data.find("rotation"))
            payload.rotation = v->asNumber();
        if (const Value *v = data.find("text"))
            payload.text = v->asString();
        if (const Value *v = data.find("styleName"))
            payload.styleName = v->asString();
    }
    else if constexpr (std::is_same_v<Payload, AcDbMText>)
    {
        payload.insertion = jsonToPoint3(data.find("insertion") ? *data.find("insertion") : Value());
        if (const Value *v = data.find("direction"))
            payload.direction = jsonToVec3(*v, {1.0, 0.0, 0.0});
        if (const Value *v = data.find("width"))
            payload.width = v->asNumber();
        if (const Value *v = data.find("height"))
            payload.height = v->asNumber(1.0);
        if (const Value *v = data.find("text"))
            payload.text = v->asString();
    }
    else if constexpr (std::is_same_v<Payload, AcDbMline>)
    {
        if (const Value *v = data.find("vertices"))
            payload.vertices = jsonToPoints3(*v);
        if (const Value *v = data.find("scale"))
            payload.scale = jsonToVec3(*v, {1.0, 1.0, 1.0});
        if (const Value *v = data.find("closed"))
            payload.closed = v->asBool();
    }
    else if constexpr (std::is_same_v<Payload, AcDbPolyFaceMesh>)
    {
        if (const Value *v = data.find("positions"))
            for (const Value &item : v->asArray())
            {
                const json::Array &a = item.asArray();
                if (a.size() >= 3)
                    payload.geometry.positions.push_back(
                        AcGePoint3d(a[0].asNumber(),
                                    a[1].asNumber(),
                                    a[2].asNumber()));
            }
        if (const Value *v = data.find("normals"))
            for (const Value &item : v->asArray())
            {
                const json::Array &a = item.asArray();
                if (a.size() >= 3)
                    payload.geometry.normals.push_back(
                        glm::vec3(float(a[0].asNumber()),
                                  float(a[1].asNumber()),
                                  float(a[2].asNumber())));
            }
        if (const Value *v = data.find("uvs"))
            for (const Value &item : v->asArray())
            {
                const json::Array &a = item.asArray();
                if (a.size() >= 2)
                    payload.geometry.uvs.push_back(
                        glm::vec2(float(a[0].asNumber()),
                                  float(a[1].asNumber())));
            }
        if (const Value *v = data.find("indices"))
            for (const double index : jsonToDoubles(*v))
                payload.geometry.indices.push_back(
                    std::uint32_t(index));
        if (const Value *v = data.find("style"))
            payload.style = MeshStyle(int(v->asNumber()));
        if (const Value *v = data.find("material"))
            payload.material.baseColorFactor = jsonToColor(*v);
    }
    else if constexpr (std::is_same_v<Payload, AcDb3dSolid>)
    {
        if (const Value *v = data.find("vertices"))
            payload.vertices = jsonToPoints3(*v);
        if (const Value *v = data.find("indices"))
            for (const double index : jsonToDoubles(*v))
                payload.indices.push_back(std::uint32_t(index));
        if (const Value *v = data.find("renderClass"))
            payload.renderClass = RenderClass(int(v->asNumber()));
    }
    else if constexpr (std::is_same_v<Payload, AcDbLight>)
    {
        if (const Value *v = data.find("type"))
            payload.type = LightType(int(v->asNumber()));
        if (const Value *v = data.find("position"))
            payload.position = jsonToPoint3(*v);
        if (const Value *v = data.find("target"))
            payload.target = jsonToVec3(*v, {0.0, 0.0, -1.0});
        if (const Value *v = data.find("color"))
        {
            const json::Array &a = v->asArray();
            if (a.size() >= 3)
                payload.color = glm::vec3(float(a[0].asNumber(1.0)),
                                          float(a[1].asNumber(1.0)),
                                          float(a[2].asNumber(1.0)));
        }
        if (const Value *v = data.find("intensity"))
            payload.intensity = float(v->asNumber(1.0));
        if (const Value *v = data.find("range"))
            payload.range = float(v->asNumber());
        if (const Value *v = data.find("innerConeAngle"))
            payload.innerConeAngle = float(v->asNumber());
        if (const Value *v = data.find("outerConeAngle"))
            payload.outerConeAngle = float(v->asNumber());
    }
    else if constexpr (std::is_same_v<Payload, AcDbBlockReference>)
    {
        if (const Value *v = data.find("blockTableRecordName"))
            payload.blockTableRecordName = v->asString();
        if (const Value *v = data.find("position"))
        {
            const json::Array &a = v->asArray();
            if (a.size() >= 3)
                payload.position = AcGePoint3d(
                    a[0].asNumber(), a[1].asNumber(),
                    a[2].asNumber());
        }
        if (const Value *v = data.find("rotation"))
            payload.rotation = v->asNumber();
        if (const Value *v = data.find("scale"))
        {
            const json::Array &a = v->asArray();
            if (a.size() >= 3)
                payload.scale = AcGeVector3d(
                    a[0].asNumber(1.0),
                    a[1].asNumber(1.0),
                    a[2].asNumber(1.0));
        }
        if (const Value *v = data.find("normal"))
        {
            const json::Array &a = v->asArray();
            if (a.size() >= 3)
                payload.normal = AcGeVector3d(
                    a[0].asNumber(), a[1].asNumber(),
                    a[2].asNumber());
        }
    }
    else
    {
        return false;
    }
    out = std::move(payload);
    return true;
}

bool decodeEntity(const std::string &className, const Value &data,
                  AcDbEntityVariant &out)
{
    const Value *payloadData = data.find("payload");
    const Value *commonData = data.find("common");
    static const Value kEmptyObject = Value(json::Object{});
    if (payloadData == nullptr || commonData == nullptr)
        return false;
    if (className == "AcDbLine")
        return decodeInto<AcDbLine>(*commonData, *payloadData, out);
    if (className == "AcDbArc")
        return decodeInto<AcDbArc>(*commonData, *payloadData, out);
    if (className == "AcDbCircle")
        return decodeInto<AcDbCircle>(*commonData, *payloadData, out);
    if (className == "AcDbEllipse")
        return decodeInto<AcDbEllipse>(*commonData, *payloadData, out);
    if (className == "AcDbPoint")
        return decodeInto<AcDbPoint>(*commonData, *payloadData, out);
    if (className == "AcDbRay")
        return decodeInto<AcDbRay>(*commonData, *payloadData, out);
    if (className == "AcDbXline")
        return decodeInto<AcDbXline>(*commonData, *payloadData, out);
    if (className == "AcDbSolid")
        return decodeInto<AcDbSolid>(*commonData, *payloadData, out);
    if (className == "AcDbHatch")
        return decodeInto<AcDbHatch>(*commonData, *payloadData, out);
    if (className == "AcDb3dPolyline")
        return decodeInto<AcDb3dPolyline>(*commonData, *payloadData, out);
    if (className == "AcDbPolyline")
        return decodeInto<AcDbPolyline>(*commonData, *payloadData, out);
    if (className == "AcDbSpline")
        return decodeInto<AcDbSpline>(*commonData, *payloadData, out);
    if (className == "AcDbText")
        return decodeInto<AcDbText>(*commonData, *payloadData, out);
    if (className == "AcDbMText")
        return decodeInto<AcDbMText>(*commonData, *payloadData, out);
    if (className == "AcDbMline")
        return decodeInto<AcDbMline>(*commonData, *payloadData, out);
    if (className == "AcDbPolyFaceMesh")
        return decodeInto<AcDbPolyFaceMesh>(*commonData, *payloadData, out);
    if (className == "AcDb3dSolid")
        return decodeInto<AcDb3dSolid>(*commonData, *payloadData, out);
    if (className == "AcDbLight")
        return decodeInto<AcDbLight>(*commonData, *payloadData, out);
    if (className == "AcDbBlockReference")
        return decodeInto<AcDbBlockReference>(*commonData, *payloadData, out);
    return false;
}

void executeSchema(SQLite::Database &db)
{
    db.exec("CREATE TABLE IF NOT EXISTS meta("
            "key TEXT PRIMARY KEY, value TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS layers("
            "name TEXT PRIMARY KEY, handle INTEGER, color TEXT,"
            "line_type TEXT, line_weight REAL, is_off INTEGER,"
            "is_frozen INTEGER, is_plottable INTEGER)");
    db.exec("CREATE TABLE IF NOT EXISTS linetypes("
            "name TEXT PRIMARY KEY, handle INTEGER, description TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS text_styles("
            "name TEXT PRIMARY KEY, handle INTEGER, file_name TEXT,"
            "big_font TEXT, text_size REAL, oblique REAL)");
    db.exec("CREATE TABLE IF NOT EXISTS blocks("
            "name TEXT PRIMARY KEY, handle INTEGER, base_point TEXT,"
            "members TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS entities("
            "handle INTEGER PRIMARY KEY, owner INTEGER, class TEXT,"
            "erased INTEGER, data TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS ops("
            "seq INTEGER PRIMARY KEY AUTOINCREMENT, kind TEXT,"
            "payload TEXT, created TEXT DEFAULT (datetime('now')))");
}

} // namespace

StoreResult saveDatabase(const AcDbDatabase &database, const char *path)
{
    StoreResult result;
    if (path == nullptr || path[0] == '\0')
    {
        result.error = "empty path";
        return result;
    }
    try
    {
        SQLite::Database db(path, SQLite::OPEN_READWRITE |
                                      SQLite::OPEN_CREATE);
        db.exec("PRAGMA journal_mode=WAL");
        executeSchema(db);
        SQLite::Transaction transaction(db);
        db.exec("DELETE FROM meta; DELETE FROM layers; "
                "DELETE FROM linetypes; DELETE FROM text_styles; "
                "DELETE FROM blocks; DELETE FROM entities;");

        // ---- meta ----
        SQLite::Statement meta(db,
            "INSERT INTO meta(key, value) VALUES (?, ?)");
        auto putMeta = [&](const char *key, const std::string &value) {
            meta.reset();
            meta.bind(1, key);
            meta.bind(2, value);
            meta.exec();
        };
        putMeta("format_version", "1");
        putMeta("next_handle",
                std::to_string(database.nextHandleValue()));
        putMeta("active_layer", database.activeLayerName());
        putMeta("active_color",
                colorToJson(database.activeColor()).dump());
        putMeta("active_linetype", database.activeLineType());
        putMeta("active_ltscale",
                std::to_string(database.activeLineTypeScale()));
        putMeta("active_lineweight",
                std::to_string(database.activeLineWeight()));

        // ---- symbol tables ----
        database.layerTable().forEach([&](const std::string &name,
                                          const AcDbLayerTableRecord &r) {
            SQLite::Statement insert(db,
                "INSERT INTO layers VALUES (?,?,?,?,?,?,?,?)");
            insert.bind(1, name);
            insert.bind(2, std::int64_t(r.handle.value));
            insert.bind(3, colorToJson(r.color).dump());
            insert.bind(4, r.lineType);
            insert.bind(5, r.lineWeight);
            insert.bind(6, r.isOff ? 1 : 0);
            insert.bind(7, r.isFrozen ? 1 : 0);
            insert.bind(8, r.isPlottable ? 1 : 0);
            insert.exec();
        });
        database.linetypeTable().forEach([&](const std::string &name,
                                             const AcDbLinetypeTableRecord &r) {
            SQLite::Statement insert(db,
                "INSERT INTO linetypes VALUES (?,?,?)");
            insert.bind(1, name);
            insert.bind(2, std::int64_t(r.handle.value));
            insert.bind(3, r.description);
            insert.exec();
        });
        database.textStyleTable().forEach([&](const std::string &name,
                                              const AcDbTextStyleTableRecord &r) {
            SQLite::Statement insert(db,
                "INSERT INTO text_styles VALUES (?,?,?,?,?,?)");
            insert.bind(1, name);
            insert.bind(2, std::int64_t(r.handle.value));
            insert.bind(3, r.fileName);
            insert.bind(4, r.bigFontFileName);
            insert.bind(5, r.textSize);
            insert.bind(6, r.obliqueAngle);
            insert.exec();
        });
        database.blockTable().forEach([&](const std::string &name,
                                          const AcDbBlockTableRecord &r) {
            Value members(json::Array{});
            for (const AcDbHandle handle : r.entityHandles())
                members.push(Value(double(handle.value)));
            SQLite::Statement insert(db,
                "INSERT INTO blocks VALUES (?,?,?,?)");
            insert.bind(1, name);
            insert.bind(2, std::int64_t(r.handle.value));
            insert.bind(3, point3ToJson(AcGePoint3d(
                                r.basePoint().x, r.basePoint().y,
                                r.basePoint().z))
                                .dump());
            insert.bind(4, members.dump());
            insert.exec();
        });

        // ---- entities ----
        database.forEachEntity([&](AcDbHandle handle,
                                   const AcDbEntityVariant &payload) {
            Value data(json::Object{});
            data.set("common", commonToJson(common(payload)));
            data.set("payload", std::visit(PayloadEncoder{}, payload));
            SQLite::Statement insert(db,
                "INSERT INTO entities VALUES (?,?,?,?,?)");
            insert.bind(1, std::int64_t(handle.value));
            insert.bind(2,
                        std::int64_t(common(payload).ownerHandle.value));
            insert.bind(3, variantClassName(payload));
            insert.bind(4, database.isErased(handle) ? 1 : 0);
            insert.bind(5, data.dump());
            insert.exec();
            ++result.entities;
        });

        // ---- ops journal (append-only) ----
        {
            SQLite::Statement op(db,
                "INSERT INTO ops(kind, payload) VALUES ('save', ?)");
            op.bind(1, "{\"entities\":" + std::to_string(result.entities) +
                           "}");
            op.exec();
        }

        transaction.commit();
        result.ok = true;
    }
    catch (const std::exception &error)
    {
        result.error = error.what();
    }
    return result;
}

StoreResult loadDatabase(const char *path, AcDbDatabase &database)
{
    StoreResult result;
    if (path == nullptr || path[0] == '\0')
    {
        result.error = "empty path";
        return result;
    }
    try
    {
        SQLite::Database db(path, SQLite::OPEN_READONLY);
        executeSchema(db);

        // The document is value-semantic: rebuild into a fresh store
        // and move it over the caller's on success.
        AcDbDatabase loaded;

        SQLite::Statement meta(db, "SELECT key, value FROM meta");
        std::uint64_t nextHandle = 1;
        std::string activeLayer, activeColor, activeLineType;
        double activeLineTypeScale = 1.0, activeLineWeight = 0.0;
        while (meta.executeStep())
        {
            const std::string key = meta.getColumn(0).getString();
            const std::string value = meta.getColumn(1).getString();
            if (key == "next_handle")
                nextHandle = std::stoull(value);
            else if (key == "active_layer")
                activeLayer = value;
            else if (key == "active_color")
                activeColor = value;
            else if (key == "active_linetype")
                activeLineType = value;
            else if (key == "active_ltscale")
                activeLineTypeScale = std::stod(value);
            else if (key == "active_lineweight")
                activeLineWeight = std::stod(value);
        }

        // ---- entities first (blocks reference handles, not payloads) ----
        {
            SQLite::Statement rows(db,
                "SELECT handle, owner, class, erased, data "
                "FROM entities");
            std::uint64_t maxHandle = 1;
            while (rows.executeStep())
            {
                AcDbHandle handle{std::uint64_t(
                    rows.getColumn(0).getInt64())};
                const std::string className =
                    rows.getColumn(2).getString();
                const bool erased = rows.getColumn(3).getInt() != 0;
                const auto parsed =
                    json::parse(rows.getColumn(4).getString());
                if (!parsed)
                {
                    result.error = "bad entity JSON at handle " +
                                   std::to_string(handle.value);
                    return result;
                }
                AcDbEntityVariant payload;
                if (!decodeEntity(className, *parsed, payload))
                {
                    result.error =
                        "unknown entity class: " + className;
                    return result;
                }
                common(payload).handle = handle;
                common(payload).ownerHandle =
                    AcDbHandle{std::uint64_t(rows.getColumn(1).getInt64())};
                if (!loaded.insertLoadedEntity(std::move(payload)))
                {
                    result.error = "entity handle collision: " +
                                   std::to_string(handle.value);
                    return result;
                }
                if (erased)
                    loaded.eraseEntity(handle);
                maxHandle = std::max(maxHandle, handle.value + 1);
                ++result.entities;
            }
            nextHandle = std::max(nextHandle, maxHandle);
        }

        // ---- symbol tables ----
        {
            SQLite::Statement rows(db, "SELECT * FROM layers");
            while (rows.executeStep())
            {
                const std::string name = rows.getColumn(0).getString();
                AcDbLayerTableRecord &record =
                    loaded.layerTable().contains(name)
                        ? *loaded.layerTable().getMutable(name)
                        : loaded.layerTable().add(name, AcDbHandle{});
                record.handle = AcDbHandle{std::uint64_t(
                    rows.getColumn(1).getInt64())};
                if (const auto parsed = json::parse(
                        rows.getColumn(2).getString()))
                    record.color = jsonToColor(*parsed);
                record.lineType = rows.getColumn(3).getString();
                record.lineWeight = rows.getColumn(4).getDouble();
                record.isOff = rows.getColumn(5).getInt() != 0;
                record.isFrozen = rows.getColumn(6).getInt() != 0;
                record.isPlottable = rows.getColumn(7).getInt() != 0;
            }
        }
        {
            SQLite::Statement rows(db,
                "SELECT name, handle, description FROM linetypes");
            while (rows.executeStep())
            {
                const std::string name = rows.getColumn(0).getString();
                const AcDbHandle handle{std::uint64_t(
                    rows.getColumn(1).getInt64())};
                if (!loaded.linetypeTable().contains(name))
                    loaded.linetypeTable().add(name, handle);
                else
                    loaded.linetypeTable()
                        .getMutable(name)
                        ->handle = handle;
            }
        }
        {
            SQLite::Statement rows(db, "SELECT * FROM text_styles");
            while (rows.executeStep())
            {
                const std::string name = rows.getColumn(0).getString();
                AcDbTextStyleTableRecord &record =
                    loaded.textStyleTable().contains(name)
                        ? *loaded.textStyleTable().getMutable(name)
                        : loaded.textStyleTable().add(name,
                                                      AcDbHandle{});
                record.handle = AcDbHandle{std::uint64_t(
                    rows.getColumn(1).getInt64())};
                record.fileName = rows.getColumn(2).getString();
                record.bigFontFileName = rows.getColumn(3).getString();
                record.textSize = rows.getColumn(4).getDouble();
                record.obliqueAngle = rows.getColumn(5).getDouble();
            }
        }
        {
            SQLite::Statement rows(db, "SELECT * FROM blocks");
            while (rows.executeStep())
            {
                const std::string name = rows.getColumn(0).getString();
                const AcDbHandle handle{std::uint64_t(
                    rows.getColumn(1).getInt64())};
                AcDbBlockTableRecord &record =
                    loaded.blockTable().contains(name)
                        ? *loaded.blockTable().getMutable(name)
                        : loaded.blockTable().add(name, AcDbHandle{});
                record.handle = handle;
                if (const auto base =
                        json::parse(rows.getColumn(2).getString()))
                {
                    const AcGePoint3d basePoint =
                        jsonToPoint3(*base);
                    record.setBasePoint(glm::dvec3(basePoint.x,
                                                   basePoint.y,
                                                   basePoint.z));
                }
                record.entityHandles().clear();
                if (const auto members =
                        json::parse(rows.getColumn(3).getString()))
                    for (const Value &member : members->asArray())
                        record.appendEntityHandle(AcDbHandle{
                            std::uint64_t(member.asNumber())});
            }
        }

        // Active settings reference symbol tables, so they are
        // applied only after every table row is in.
        if (!activeLayer.empty())
            loaded.setActiveLayerName(activeLayer);
        if (!activeColor.empty())
        {
            const auto parsed = json::parse(activeColor);
            if (parsed)
                loaded.setActiveColor(jsonToColor(*parsed));
        }
        if (!activeLineType.empty())
            loaded.setActiveLineType(activeLineType);
        loaded.setActiveLineTypeScale(activeLineTypeScale);
        loaded.setActiveLineWeight(activeLineWeight);
        loaded.restoreNextHandle(nextHandle);
        database = std::move(loaded);
        result.ok = true;
    }
    catch (const std::exception &error)
    {
        result.error = error.what();
    }
    return result;
}

} // namespace acdb

