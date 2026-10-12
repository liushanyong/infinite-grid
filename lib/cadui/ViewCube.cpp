#include "ViewCube.hpp"
#include "ViewCubeBgfx.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace cadui
{
    namespace
    {
        constexpr float kNavInset = 2.0f;
        constexpr float kViewCubePx = 84.0f;
        constexpr float kViewCubeScale = 0.36f;
        constexpr float kF = 0.80f;
        constexpr float kE = 1.00f;
        constexpr float kM = (kF + kE) * 0.5f;
        constexpr float kRingZ = -1.0f;
        constexpr float kRingR0 = 1.40f;
        constexpr float kRingR1 = 1.74f;
        // The band midline is (1.40 + 1.74) / 2 == 1.57, which is where
        // OpenCADStudio anchors R_CARD.  Measuring the rendered output shows the
        // rasterised ink of W/E/S landing ~0.10..0.15 local units (~3..5 px) on
        // the *inner* side of that midline, so the letters visibly float in the
        // gap between the cube and the band.  The glyph is ~0.26 units wide
        // radially, so 1.66 keeps the whole ink box inside 1.40..1.74 instead of
        // straddling the inner edge.
        constexpr float kCardinalR = 1.66f;
        constexpr float kLabelHeight = 0.46f;
        constexpr float kProjectionHalfExtent = 2000.0f;

        constexpr ImU32 kSurface = IM_COL32(158, 194, 214, 255);
        constexpr ImU32 kHover = IM_COL32(51, 184, 168, 255);
        constexpr ImU32 kEdge = IM_COL32(13, 20, 33, 255);
        constexpr ImU32 kIcon = IM_COL32(180, 182, 185, 255);
        constexpr ImU32 kRingLabel = IM_COL32(232, 238, 242, 210);

        struct PolygonData
        {
            std::array<ImVec2, 4> points{};
            int count{ 4 };
            int id{ 0 };
            float depth{ 0.0f };
        };

        const char* regionLabelById(int id)
        {
            static constexpr const char* labels[26] = {
                "TOP", "BOTTOM", "FRONT", "BACK", "RIGHT", "LEFT",
                "Top Front", "Top Back", "Top Right", "Top Left",
                "Bot Front", "Bot Back", "Bot Right", "Bot Left",
                "Front Right", "Front Left", "Back Right", "Back Left",
                "Top Front Right", "Top Front Left", "Top Back Right", "Top Back Left",
                "Bot Front Right", "Bot Front Left", "Bot Back Right", "Bot Back Left"
            };
            return id >= 0 && id < 26 ? labels[id] : "";
        }

        ViewCubeRegion makeRegion(int id, const ViewCubeOptions& options)
        {
            ViewCubeRegion region = ViewCubeWidget::regionById(id);
            if (id >= 0 && id < 6)
            {
                region.label = options.faceLabels[static_cast<size_t>(id)];
            }
            return region;
        }

        // OpenCADStudio's hit test projects the raw region centroid, not the
        // normalized snap direction.  Keeping this distinction is important:
        // edges sit at |centroid| = 0.90 and corners sit at |centroid| = 1.56,
        // which gives the corners their larger, reference-accurate hit areas.
        glm::vec3 regionCentroid(int id)
        {
            static constexpr float centroids[26][3] = {
                {0.0f, 0.0f,  kE}, {0.0f, 0.0f, -kE}, {0.0f, -kE, 0.0f},
                {0.0f,  kE, 0.0f}, { kE, 0.0f, 0.0f}, {-kE, 0.0f, 0.0f},
                {0.0f, -kM,  kM}, {0.0f,  kM,  kM}, { kM, 0.0f,  kM}, {-kM, 0.0f,  kM},
                {0.0f, -kM, -kM}, {0.0f,  kM, -kM}, { kM, 0.0f, -kM}, {-kM, 0.0f, -kM},
                { kM, -kM, 0.0f}, {-kM, -kM, 0.0f}, { kM,  kM, 0.0f}, {-kM,  kM, 0.0f},
                { kM, -kM,  kM}, {-kM, -kM,  kM}, { kM,  kM,  kM}, {-kM,  kM,  kM},
                { kM, -kM, -kM}, {-kM, -kM, -kM}, { kM,  kM, -kM}, {-kM,  kM, -kM}
            };
            id = std::clamp(id, 0, 25);
            return { centroids[id][0], centroids[id][1], centroids[id][2] };
        }

        void appendQuad(std::vector<PolygonData>& polygons,
                        const std::array<glm::vec3, 4>& local,
                        int id,
                        const glm::mat3& rotation,
                        const ImVec2& center,
                        float radius)
        {
            PolygonData polygon;
            polygon.count = 4;
            polygon.id = id;
            glm::vec3 sum{ 0.0f };
            for (unsigned i = 0; i < 4; ++i)
            {
                const glm::vec3 eye = rotation * local[i];
                sum += eye;
                polygon.points[i] = { center.x + eye.x * radius, center.y - eye.y * radius };
            }
            polygon.depth = (sum * 0.25f).z;
            polygons.push_back(polygon);
        }

        void appendTriangle(std::vector<PolygonData>& polygons,
                            const std::array<glm::vec3, 3>& local,
                            int id,
                            const glm::mat3& rotation,
                            const ImVec2& center,
                            float radius)
        {
            PolygonData polygon;
            polygon.count = 3;
            polygon.id = id;
            glm::vec3 sum{ 0.0f };
            for (unsigned i = 0; i < 3; ++i)
            {
                const glm::vec3 eye = rotation * local[i];
                sum += eye;
                polygon.points[i] = { center.x + eye.x * radius, center.y - eye.y * radius };
            }
            polygon.depth = (sum / 3.0f).z;
            polygons.push_back(polygon);
        }
    } // namespace

    namespace
    {
        std::vector<PolygonData> buildPolygons(const glm::mat3& rotation,
                                               const ImVec2& center,
                                               float radius)
        {
            std::vector<PolygonData> polygons;
            polygons.reserve(26);

            appendQuad(polygons, {{ {-kF, -kF,  kE}, { kF, -kF,  kE}, { kF,  kF,  kE}, {-kF,  kF,  kE} }}, 0, rotation, center, radius);
            appendQuad(polygons, {{ {-kF,  kF, -kE}, { kF,  kF, -kE}, { kF, -kF, -kE}, {-kF, -kF, -kE} }}, 1, rotation, center, radius);
            appendQuad(polygons, {{ { kF, -kE, -kF}, {-kF, -kE, -kF}, {-kF, -kE,  kF}, { kF, -kE,  kF} }}, 2, rotation, center, radius);
            appendQuad(polygons, {{ {-kF,  kE, -kF}, { kF,  kE, -kF}, { kF,  kE,  kF}, {-kF,  kE,  kF} }}, 3, rotation, center, radius);
            appendQuad(polygons, {{ { kE,  kF, -kF}, { kE, -kF, -kF}, { kE, -kF,  kF}, { kE,  kF,  kF} }}, 4, rotation, center, radius);
            appendQuad(polygons, {{ {-kE, -kF, -kF}, {-kE,  kF, -kF}, {-kE,  kF,  kF}, {-kE, -kF,  kF} }}, 5, rotation, center, radius);

            appendQuad(polygons, {{ { kF, -kF,  kE}, {-kF, -kF,  kE}, {-kF, -kE,  kF}, { kF, -kE,  kF} }}, 6, rotation, center, radius);
            appendQuad(polygons, {{ {-kF,  kF,  kE}, { kF,  kF,  kE}, { kF,  kE,  kF}, {-kF,  kE,  kF} }}, 7, rotation, center, radius);
            appendQuad(polygons, {{ { kF,  kF,  kE}, { kF, -kF,  kE}, { kE, -kF,  kF}, { kE,  kF,  kF} }}, 8, rotation, center, radius);
            appendQuad(polygons, {{ {-kF, -kF,  kE}, {-kF,  kF,  kE}, {-kE,  kF,  kF}, {-kE, -kF,  kF} }}, 9, rotation, center, radius);
            appendQuad(polygons, {{ { kF, -kF, -kE}, {-kF, -kF, -kE}, {-kF, -kE, -kF}, { kF, -kE, -kF} }}, 10, rotation, center, radius);
            appendQuad(polygons, {{ {-kF,  kF, -kE}, { kF,  kF, -kE}, { kF,  kE, -kF}, {-kF,  kE, -kF} }}, 11, rotation, center, radius);
            appendQuad(polygons, {{ { kF,  kF, -kE}, { kF, -kF, -kE}, { kE, -kF, -kF}, { kE,  kF, -kF} }}, 12, rotation, center, radius);
            appendQuad(polygons, {{ {-kF, -kF, -kE}, {-kF,  kF, -kE}, {-kE,  kF, -kF}, {-kE, -kF, -kF} }}, 13, rotation, center, radius);
            appendQuad(polygons, {{ { kF, -kE, -kF}, { kF, -kE,  kF}, { kE, -kF,  kF}, { kE, -kF, -kF} }}, 14, rotation, center, radius);
            appendQuad(polygons, {{ {-kF, -kE,  kF}, {-kF, -kE, -kF}, {-kE, -kF, -kF}, {-kE, -kF,  kF} }}, 15, rotation, center, radius);
            appendQuad(polygons, {{ { kF,  kE,  kF}, { kF,  kE, -kF}, { kE,  kF, -kF}, { kE,  kF,  kF} }}, 16, rotation, center, radius);
            appendQuad(polygons, {{ {-kF,  kE,  kF}, {-kF,  kE, -kF}, {-kE,  kF, -kF}, {-kE,  kF,  kF} }}, 17, rotation, center, radius);

            // Corner order must match regionLabelById(), snapDirection() and the
            // OpenCADStudio region_centroids() order:
            // TFR, TFL, TBR, TBL, BFR, BFL, BBR, BBL.
            const std::array<std::array<glm::vec3, 3>, 8> corners{{
                {{ { kF, -kF,  kE}, { kF, -kE,  kF}, { kE, -kF,  kF} }},
                {{ {-kF, -kF,  kE}, {-kF, -kE,  kF}, {-kE, -kF,  kF} }},
                {{ { kF,  kF,  kE}, { kF,  kE,  kF}, { kE,  kF,  kF} }},
                {{ {-kF,  kF,  kE}, {-kF,  kE,  kF}, {-kE,  kF,  kF} }},
                {{ { kF, -kF, -kE}, { kF, -kE, -kF}, { kE, -kF, -kF} }},
                {{ {-kF, -kF, -kE}, {-kF, -kE, -kF}, {-kE, -kF, -kF} }},
                {{ { kF,  kF, -kE}, { kF,  kE, -kF}, { kE,  kF, -kF} }},
                {{ {-kF,  kF, -kE}, {-kF,  kE, -kF}, {-kE,  kF, -kF} }}
            }};
            for (unsigned i = 0; i < 8; ++i)
            {
                appendTriangle(polygons, corners[i], 18 + static_cast<int>(i), rotation, center, radius);
            }
            return polygons;
        }

        bool pointInPolygon(const ImVec2& point, const PolygonData& polygon)
        {
            bool inside = false;
            for (int i = 0, j = polygon.count - 1; i < polygon.count; j = i++)
            {
                const ImVec2& a = polygon.points[static_cast<size_t>(j)];
                const ImVec2& b = polygon.points[static_cast<size_t>(i)];
                if ((a.y > point.y) != (b.y > point.y) &&
                    point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
                {
                    inside = !inside;
                }
            }
            return inside;
        }

        int hitTestPolygons(const ImVec2& mouse, const std::vector<PolygonData>& polygons)
        {
            // glm::lookAt maps a normal pointing toward the camera to positive
            // eye Z.  A convex cube can project several regions onto one screen
            // point, so keep the region closest to the camera (largest Z).
            int bestId = -1;
            float bestDepth = -std::numeric_limits<float>::max();
            for (const PolygonData& polygon : polygons)
            {
                if (pointInPolygon(mouse, polygon) && polygon.depth > bestDepth)
                {
                    bestDepth = polygon.depth;
                    bestId = polygon.id;
                }
            }
            return bestId;
        }

        int hitTestRegion(const ImVec2& mouse,
                          const glm::mat3& rotation,
                          const ImVec2& center,
                          float radius)
        {
            // OpenCADStudio rejects clicks outside the cube's ±half box
            // before the region loop, so the generous face threshold cannot
            // steal clicks meant for the compass ring.
            if (std::abs(mouse.x - center.x) > radius ||
                std::abs(mouse.y - center.y) > radius)
            {
                return -1;
            }
            int bestId = -1;
            float bestDistanceSq = std::numeric_limits<float>::max();
            for (int id = 0; id < 26; ++id)
            {
                const glm::vec3 centroid = regionCentroid(id);
                const glm::vec3 eye = rotation * centroid;
                // OpenCADStudio normalizes the rotated centroid before the
                // 0.05 grazing cutoff; edge/corner centroids are longer than
                // 1, so comparing the raw eye.z lets near-grazing regions
                // through that the reference culls.
                if (glm::normalize(eye).z < 0.05f)
                {
                    continue;
                }
                const float threshold = radius * (id < 6 ? 0.92f : id < 18 ? 0.38f : 0.28f);
                const float dx = mouse.x - (center.x + eye.x * radius);
                const float dy = mouse.y - (center.y - eye.y * radius);
                const float distanceSq = dx * dx + dy * dy;
                if (distanceSq <= threshold * threshold && distanceSq < bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    bestId = id;
                }
            }
            return bestId;
        }

        void drawPolygons(ImDrawList* drawList,
                          std::vector<PolygonData>& polygons,
                          int hoveredId,
                          bool fill)
        {
            std::sort(polygons.begin(), polygons.end(), [](const PolygonData& lhs, const PolygonData& rhs)
            {
                // Far faces first (negative eye Z), near faces last (positive eye Z).
                return lhs.depth < rhs.depth;
            });

            for (const PolygonData& polygon : polygons)
            {
                const bool hovered = polygon.id == hoveredId;
                if (!fill && !hovered)
                {
                    continue;
                }
                // BGFX already highlights the hovered surface in its shader.
                // Do not draw an ImGui polygon overlay on top of the depth-
                // rendered cube, or hover can appear to select a back face.
                if (!fill)
                {
                    continue;
                }
                const ImU32 color = hovered ? kHover : kSurface;
                if (polygon.count == 4)
                {
                    drawList->AddConvexPolyFilled(polygon.points.data(), 4, color);
                }
                else
                {
                    drawList->AddTriangleFilled(polygon.points[0], polygon.points[1], polygon.points[2], color);
                }
            }

            for (const PolygonData& polygon : polygons)
            {
                if (polygon.depth < 0.0f)
                {
                    continue;
                }
                drawList->AddPolyline(polygon.points.data(), polygon.count, kEdge, ImDrawFlags_Closed, 1.25f);
            }
        }
    } // namespace

    namespace
    {
        void drawRingFallback(ImDrawList* drawList,
                              const glm::mat3& rotation,
                              const ImVec2& center,
                              float radius)
        {
            constexpr unsigned segments = 64;
            for (unsigned i = 0; i < segments; ++i)
            {
                const float a0 = static_cast<float>(i) * 6.28318530718f / static_cast<float>(segments);
                const float a1 = static_cast<float>(i + 1) * 6.28318530718f / static_cast<float>(segments);
                const glm::vec3 p0{ std::cos(a0) * kRingR0, std::sin(a0) * kRingR0, kRingZ };
                const glm::vec3 p1{ std::cos(a1) * kRingR0, std::sin(a1) * kRingR0, kRingZ };
                const glm::vec3 p2{ std::cos(a1) * kRingR1, std::sin(a1) * kRingR1, kRingZ };
                const glm::vec3 p3{ std::cos(a0) * kRingR1, std::sin(a0) * kRingR1, kRingZ };
                const ImVec2 s0{ center.x + (rotation * p0).x * radius, center.y - (rotation * p0).y * radius };
                const ImVec2 s1{ center.x + (rotation * p1).x * radius, center.y - (rotation * p1).y * radius };
                const ImVec2 s2{ center.x + (rotation * p2).x * radius, center.y - (rotation * p2).y * radius };
                const ImVec2 s3{ center.x + (rotation * p3).x * radius, center.y - (rotation * p3).y * radius };
                drawList->AddTriangleFilled(s0, s1, s2, IM_COL32(158, 194, 214, 235));
                drawList->AddTriangleFilled(s0, s2, s3, IM_COL32(158, 194, 214, 235));
            }
        }

        void drawTextCentered(ImDrawList* drawList, const ImVec2& position,
                              const char* text, ImU32 color, float size)
        {
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            const ImVec2 centered{ position.x - textSize.x * 0.5f,
                                   position.y - textSize.y * 0.5f };
            if (std::abs(size - ImGui::GetFontSize()) > 0.01f)
            {
                drawList->AddText(nullptr, size, centered, color, text);
                return;
            }
            drawList->AddText(centered, color, text);
        }

        // ImGui lays a text run out on its *em* box: CalcTextSize reports a line
        // height equal to the requested font size and the glyph X0/Y0 offsets
        // are measured from that box's top-left.  OpenCADStudio instead
        // rasterises every label into its ink box (viewcube_text_atlas.rs keeps
        // the ink extents plus 3 px padding) and centres *that* box on the
        // anchor.  Both renderers below route through measureInkBox() so a
        // label sits on the ring midline exactly like the reference instead of
        // riding about half a cap-height high, and so the ImGui fallback agrees
        // with the depth-tested bgfx path.
        struct InkBox
        {
            ImVec2 center{ 0.0f, 0.0f };
            bool valid{ false };
        };

        InkBox measureInkBox(const char* text, float size)
        {
            InkBox box;
            ImFont* font = ImGui::GetFont();
            if (!font || !text || !*text)
            {
                return box;
            }
            const float scale = size / font->FontSize;
            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float maxY = std::numeric_limits<float>::lowest();
            float pen = 0.0f;   // running pen position: glyph X0/X1 are per-glyph
            const char* cursor = text;
            const char* end = text + std::string::traits_type::length(text);
            while (cursor < end)
            {
                unsigned int codepoint = 0;
                const int bytes = ImTextCharFromUtf8(&codepoint, cursor, end);
                if (bytes <= 0 || codepoint == 0)
                {
                    break;
                }
                cursor += bytes;
                const ImFontGlyph* glyph = font->FindGlyph(static_cast<ImWchar>(codepoint));
                if (!glyph)
                {
                    glyph = font->FindGlyph(static_cast<ImWchar>('?'));
                }
                if (!glyph || !glyph->Visible)
                {
                    pen += glyph ? glyph->AdvanceX : font->FallbackAdvanceX;
                    continue;
                }
                minX = std::min(minX, pen + glyph->X0);
                minY = std::min(minY, glyph->Y0);
                maxX = std::max(maxX, pen + glyph->X1);
                maxY = std::max(maxY, glyph->Y1);
                pen += glyph->AdvanceX;
            }
            if (maxX < minX || maxY < minY)
            {
                return box;
            }
            box.center = ImVec2{ (minX + maxX) * 0.5f * scale, (minY + maxY) * 0.5f * scale };
            box.valid = true;
            return box;
        }

        void drawTextWithHalo(ImDrawList* drawList, const ImVec2& position,
                              const char* text, ImU32 color, float size)
        {
            // CalcTextSize() always measures with the *current* font size; the
            // label runs pass an explicit size, so ask the font directly.
            ImFont* font = ImGui::GetFont();
            const ImVec2 textSize = font && font->FontSize > 0.0f
                                        ? font->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.0f, text)
                                        : ImVec2(size, size);
            const ImVec2 centered{ position.x - textSize.x * 0.5f,
                                   position.y - textSize.y * 0.5f };
            constexpr ImU32 halo = IM_COL32(255, 255, 255, 175);
            constexpr float offsets[4][2] = {
                { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f }
            };
            for (const auto& offset : offsets)
            {
                drawList->AddText(nullptr, size,
                                  ImVec2(centered.x + offset[0], centered.y + offset[1]),
                                  halo, text);
            }
            drawList->AddText(nullptr, size, centered, color, text);
        }

        // The compass ring is a real 3D ring, not a screen-space circle: ViewCubeBgfx's
        // buildRing() emits a quad at z = RING_Z whose *positions* go through
        // uView = compassRotation, while frag.sc carves the 1.40..1.74 annulus
        // from the un-rotated local XY carried in a_normal.  The band therefore
        // foreshortens with the camera exactly like OpenCADStudio's triangle
        // ring (build_ring), and the letters must be projected the same way —
        // pin them to a screen-space circle and they slide off the band.
        void drawRingLabels(ImDrawList* drawList,
                            const glm::mat3& rotation,
                            const ImVec2& center,
                            float radius)
        {
            const std::array<const char*, 4> names{ "N", "E", "S", "W" };
            const std::array<glm::vec3, 4> directions{{
                { 0.0f,  kCardinalR, kRingZ },
                { kCardinalR, 0.0f,  kRingZ },
                { 0.0f, -kCardinalR, kRingZ },
                {-kCardinalR, 0.0f,  kRingZ }
            }};
            // Same metric as the depth-tested path so the fallback cannot drift
            // away from the bgfx renderer when the GPU path is unavailable.
            const float size = kLabelHeight * radius;
            for (unsigned i = 0; i < 4; ++i)
            {
                const glm::vec3 eye = rotation * directions[i];
                ImVec2 anchor{ center.x + eye.x * radius, center.y - eye.y * radius };
                const InkBox ink = measureInkBox(names[i], size);
                if (ink.valid)
                {
                    anchor.x -= ink.center.x;
                    anchor.y -= ink.center.y;
                }
                drawTextWithHalo(drawList, anchor, names[i], IM_COL32(13, 20, 33, 255), size);
            }
        }

        void appendTextVertices(std::vector<ViewCubeLabelVertex>& output,
                                const ImVec2& origin,
                                ImVec2 position,
                                const char* text,
                                ImU32 color,
                                float size,
                                float eyeZ)
        {
            ImFont* font = ImGui::GetFont();
            if (!font || !text)
            {
                return;
            }

            const float scale = size / font->FontSize;
            const ImVec2 textSize = font->CalcTextSizeA(
                size, std::numeric_limits<float>::max(), 0.0f, text, nullptr, nullptr);
            position.x -= textSize.x * 0.5f;
            position.y -= textSize.y * 0.5f;

            const char* cursor = text;
            const char* end = text + std::string::traits_type::length(text);
            float x = position.x;
            const float y = position.y;
            while (cursor < end)
            {
                unsigned int codepoint = 0;
                const int bytes = ImTextCharFromUtf8(&codepoint, cursor, end);
                if (bytes <= 0 || codepoint == 0)
                {
                    break;
                }
                cursor += bytes;
                const ImFontGlyph* glyph = font->FindGlyph(static_cast<ImWchar>(codepoint));
                if (!glyph)
                {
                    glyph = font->FindGlyph(static_cast<ImWchar>('?'));
                }
                if (glyph && glyph->Visible)
                {
                    // label positions are widget-screen coordinates; convert them
                    // into the private ViewCube framebuffer's local coordinates.
                    const float x0 = x + glyph->X0 * scale - origin.x;
                    const float y0 = y + glyph->Y0 * scale - origin.y;
                    const float x1 = x + glyph->X1 * scale - origin.x;
                    const float y1 = y + glyph->Y1 * scale - origin.y;
                    const ImVec2 uv0{ glyph->U0, glyph->V0 };
                    const ImVec2 uv1{ glyph->U1, glyph->V1 };
                    ViewCubeLabelVertex corners[4] = {
                        { ImVec2{ x0, y0 }, ImVec2{ uv0.x, uv0.y }, color, eyeZ },
                        { ImVec2{ x1, y0 }, ImVec2{ uv1.x, uv0.y }, color, eyeZ },
                        { ImVec2{ x1, y1 }, ImVec2{ uv1.x, uv1.y }, color, eyeZ },
                        { ImVec2{ x0, y1 }, ImVec2{ uv0.x, uv1.y }, color, eyeZ }
                    };
                    constexpr unsigned int order[6] = { 0, 1, 2, 0, 2, 3 };
                    for (unsigned int index : order)
                    {
                        output.push_back(corners[index]);
                    }
                }
                x += glyph ? glyph->AdvanceX * scale : font->FallbackAdvanceX * scale;
            }
        }

        void appendOrientedTextVertices(std::vector<ViewCubeLabelVertex>& output,
                                        const ImVec2& origin,
                                        const ImVec2& cubeCenter,
                                        float radius,
                                        const glm::mat3& rotation,
                                        const glm::vec3& center,
                                        const glm::vec3& u,
                                        const glm::vec3& v,
                                        const char* text,
                                        ImU32 color,
                                        float size,
                                        bool inkCentered)
        {
            ImFont* font = ImGui::GetFont();
            if (!font || !text)
            {
                return;
            }

            const float scale = size / font->FontSize;
            const ImVec2 textSize = font->CalcTextSizeA(
                size, std::numeric_limits<float>::max(), 0.0f, text, nullptr, nullptr);
            // Face labels keep ImGui's em-box centring — that is what every
            // other ImGui label in the app does.  Only the compass letters ask
            // for ink-box centring, because they are anchored on the ring
            // midline and an em box would ride about half a cap-height high.
            const InkBox ink = inkCentered ? measureInkBox(text, size) : InkBox{};
            float x = ink.valid ? -ink.center.x : -textSize.x * 0.5f;
            const float y = ink.valid ? -ink.center.y : -textSize.y * 0.5f;

            const char* cursor = text;
            const char* end = text + std::string::traits_type::length(text);
            while (cursor < end)
            {
                unsigned int codepoint = 0;
                const int bytes = ImTextCharFromUtf8(&codepoint, cursor, end);
                if (bytes <= 0 || codepoint == 0)
                {
                    break;
                }
                cursor += bytes;

                const ImFontGlyph* glyph = font->FindGlyph(static_cast<ImWchar>(codepoint));
                if (!glyph)
                {
                    glyph = font->FindGlyph(static_cast<ImWchar>('?'));
                }
                if (glyph && glyph->Visible)
                {
                    // ImGui glyph coordinates are top-down.  The compass text
                    // plane uses +Y as screen-up, exactly like OpenCADStudio.
                    // The ring's local space is unit-sized; the glyph metrics
                    // are widget pixels.  Convert pixels into local units so a
                    // 12 px glyph stays 12 px after the renderer scales by radius.
                    const float lx0 = (x + glyph->X0 * scale) / radius;
                    const float ly0 = -(y + glyph->Y0 * scale) / radius;
                    const float lx1 = (x + glyph->X1 * scale) / radius;
                    const float ly1 = -(y + glyph->Y1 * scale) / radius;
                    const glm::vec3 local[4] = {
                        center + u * lx0 + v * ly0,
                        center + u * lx1 + v * ly0,
                        center + u * lx1 + v * ly1,
                        center + u * lx0 + v * ly1
                    };
                    const ImVec2 uv0{ glyph->U0, glyph->V0 };
                    const ImVec2 uv1{ glyph->U1, glyph->V1 };
                    // Corner UVs must mix U and V independently.  Assigning
                    // uv0/uv1 as pairs makes V follow the quad's X axis, so the
                    // glyph is sampled along its diagonal and renders as bars.
                    ViewCubeLabelVertex corners[4] = {
                        { ImVec2{}, ImVec2{ uv0.x, uv0.y }, color, 0.0f },
                        { ImVec2{}, ImVec2{ uv1.x, uv0.y }, color, 0.0f },
                        { ImVec2{}, ImVec2{ uv1.x, uv1.y }, color, 0.0f },
                        { ImVec2{}, ImVec2{ uv0.x, uv1.y }, color, 0.0f }
                    };
                    for (unsigned i = 0; i < 4; ++i)
                    {
                        const glm::vec3 eye = rotation * local[i];
                        corners[i].position = ImVec2{
                            cubeCenter.x + eye.x * radius - origin.x,
                            cubeCenter.y - eye.y * radius - origin.y
                        };
                        corners[i].depth = eye.z;
                    }
                    constexpr unsigned int order[6] = { 0, 1, 2, 0, 2, 3 };
                    for (unsigned int index : order)
                    {
                        output.push_back(corners[index]);
                    }
                }
                x += glyph ? glyph->AdvanceX * scale : font->FallbackAdvanceX * scale;
            }
        }

        void faceLabelAxes(int face, glm::vec3& u, glm::vec3& v)
        {
            switch (face)
            {
            case 0: u = {  1.0f, 0.0f, 0.0f }; v = { 0.0f,  1.0f, 0.0f }; break;
            case 1: u = {  1.0f, 0.0f, 0.0f }; v = { 0.0f, -1.0f, 0.0f }; break;
            case 2: u = {  1.0f, 0.0f, 0.0f }; v = { 0.0f,  0.0f, 1.0f }; break;
            case 3: u = { -1.0f, 0.0f, 0.0f }; v = { 0.0f,  0.0f, 1.0f }; break;
            case 4: u = {  0.0f, 1.0f, 0.0f }; v = { 0.0f,  0.0f, 1.0f }; break;
            case 5: u = {  0.0f,-1.0f, 0.0f }; v = { 0.0f,  0.0f, 1.0f }; break;
            default: u = { 1.0f, 0.0f, 0.0f }; v = { 0.0f,  1.0f, 0.0f }; break;
            }
        }

        void appendTextWithHaloVertices(std::vector<ViewCubeLabelVertex>& output,
                                        const ImVec2& origin,
                                        const ImVec2& position,
                                        const char* text,
                                        ImU32 color,
                                        float size,
                                        float eyeZ)
        {
            constexpr ImU32 halo = IM_COL32(255, 255, 255, 175);
            constexpr float offsets[4][2] = {
                { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f }
            };
            for (const auto& offset : offsets)
            {
                appendTextVertices(output, origin,
                                   ImVec2{ position.x + offset[0], position.y + offset[1] },
                                   text, halo, size, eyeZ + 0.02f);
            }
            appendTextVertices(output, origin, position, text, color, size, eyeZ);
        }

        ImVec2 iconPoint(const ImVec2& center, float scale, float x, float y)
        {
            return { center.x + x * scale, center.y + y * scale };
        }

        void drawIconButton(ImDrawList* drawList, const ImRect& rect, bool hovered)
        {
            if (hovered)
            {
                drawList->AddRectFilled(rect.Min, rect.Max, IM_COL32(85, 136, 170, 90), 3.0f);
            }
        }

        // OpenCADStudio assets/icons/ui/home.svg — a 24-unit viewBox outline
        //   M12 3 L21.5 11.5 H18.5 V20.5 H14 V14.5 H10 V20.5 H5.5 V11.5 H2.5 Z
        // rendered at 13 px (themed_home(13.0)) inside the 16 px button.  The
        // outline is concave (door notch) and the previous point list also
        // carried a stray (-9.5,-8.5) vertex that flattened the gable into a
        // half-roof.  AddConvexPolyFilled only fills the convex core of a path,
        // so the shape is emitted as three convex pieces (roof + two wall
        // slabs) instead of one 12-gon: the door notch then survives.
        void drawHomeIcon(ImDrawList* drawList, const ImVec2& center, bool hovered)
        {
            constexpr float kHomeIconPx = 13.0f;
            const float scale = kHomeIconPx / 24.0f;
            const ImU32 color = hovered ? IM_COL32_WHITE : kIcon;

            std::array<ImVec2, 3> roof{{
                iconPoint(center, scale,  0.0f, -9.0f),
                iconPoint(center, scale, -9.5f, -0.5f),
                iconPoint(center, scale,  9.5f, -0.5f)
            }};
            drawList->AddTriangleFilled(roof[0], roof[1], roof[2], color);

            // Walls run from the eave line to the base; the 4-unit gap between
            // x = -2 and x = 2 is the doorway.  Their top edge is pulled 0.2
            // units above the roof base so the antialiased fills overlap
            // instead of leaving a hairline seam.
            std::array<ImVec2, 4> leftWall{{
                iconPoint(center, scale, -6.5f, -0.7f),
                iconPoint(center, scale, -2.0f, -0.7f),
                iconPoint(center, scale, -2.0f,  8.5f),
                iconPoint(center, scale, -6.5f,  8.5f)
            }};
            drawList->AddConvexPolyFilled(leftWall.data(), 4, color);

            std::array<ImVec2, 4> rightWall{{
                iconPoint(center, scale, 2.0f, -0.7f),
                iconPoint(center, scale, 6.5f, -0.7f),
                iconPoint(center, scale, 6.5f,  8.5f),
                iconPoint(center, scale, 2.0f,  8.5f)
            }};
            drawList->AddConvexPolyFilled(rightWall.data(), 4, color);

            if (hovered)
            {
                std::array<ImVec2, 12> outline{{
                    iconPoint(center, scale,  0.0f, -9.0f),
                    iconPoint(center, scale,  9.5f, -0.5f),
                    iconPoint(center, scale,  6.5f, -0.5f),
                    iconPoint(center, scale,  6.5f,  8.5f),
                    iconPoint(center, scale,  2.0f,  8.5f),
                    iconPoint(center, scale,  2.0f,  2.5f),
                    iconPoint(center, scale, -2.0f,  2.5f),
                    iconPoint(center, scale, -2.0f,  8.5f),
                    iconPoint(center, scale, -6.5f,  8.5f),
                    iconPoint(center, scale, -6.5f, -0.5f),
                    iconPoint(center, scale, -9.5f, -0.5f),
                    iconPoint(center, scale,  0.0f, -9.0f)
                }};
                drawList->AddPolyline(outline.data(), static_cast<int>(outline.size()) - 1,
                                      IM_COL32_WHITE, ImDrawFlags_Closed, 1.0f);
            }
        }

        // OpenCADStudio assets/icons/ui/undo.svg / redo.svg — 24-unit stroke
        // icons (stroke-width 2.2, round caps): a chevron whose tip meets the
        // start of a horizontal line, which dives through a half-circle drum
        // into a short return stub.
        //   undo: M8 3 L3 8 L8 13   M3 8 H14 A6 6 0 0 1 14 20 H12
        //   redo: mirrored about x = 12.
        // Coordinates below are the SVG units re-centred on the icon origin
        // (x-12, y-12); kIcon happens to be the SVGs' own stroke colour
        // #B4B6B9.  The previous geometry put the chevron tip 5 units inboard
        // of the line start and the whole glyph ~6 units high, so the arrow
        // rode on top of the drum instead of feeding it.
        void drawRollIcon(ImDrawList* drawList, const ImVec2& center, bool redo, bool hovered)
        {
            constexpr float kRollIconPx = 13.0f;   // matches themed_home(13.0)
            const float scale = kRollIconPx / 24.0f;
            const ImU32 color = hovered ? IM_COL32_WHITE : kIcon;
            const float s = redo ? 1.0f : -1.0f;   // redo mirrors undo about x = 0

            drawList->PathClear();
            drawList->PathLineTo(iconPoint(center, scale, s * 4.0f, -9.0f));
            drawList->PathLineTo(iconPoint(center, scale, s * 9.0f, -4.0f));
            drawList->PathLineTo(iconPoint(center, scale, s * 4.0f, 1.0f));
            drawList->PathStroke(color, false, 2.2f * scale);

            // Half-circle drum: undo's bulges right (sweep -pi/2 -> +pi/2
            // through angle 0), redo's mirrors through angle -pi.  PathArcTo
            // interpolates the angle linearly, so the reversed sweep works.
            drawList->PathClear();
            drawList->PathLineTo(iconPoint(center, scale, s * 9.0f, -4.0f));
            drawList->PathLineTo(iconPoint(center, scale, s * -2.0f, -4.0f));
            if (!redo)
            {
                drawList->PathArcTo(iconPoint(center, scale, 2.0f, 2.0f), 6.0f * scale,
                                    -1.5707963f, 1.5707963f, 12);
            }
            else
            {
                drawList->PathArcTo(iconPoint(center, scale, -2.0f, 2.0f), 6.0f * scale,
                                    -1.5707963f, -4.7123890f, 12);
            }
            drawList->PathLineTo(iconPoint(center, scale, 0.0f, 8.0f));
            drawList->PathStroke(color, false, 2.2f * scale);
        }

        // OpenCADStudio assets/icons/ui/tri_*.svg draw a *flat* triangle inside a
        // 12-unit box — `M2 4.5 L10 4.5 L6 9 Z` — and the nav control renders
        // that box at themed_arrow_*(8.0) inside the 9 px nudge button.  The old
        // icon was a symmetric 8x8 px triangle that filled the whole button and
        // read as a blob, so use the reference proportions: 8 * 8/12 px wide by
        // 4.5 * 8/12 px tall.  Slots still point inward, exactly like the
        // reference (top slot uses arrow_down, left slot uses arrow_right).
        void drawNudgeIcon(ImDrawList* drawList, const ImVec2& center, int direction, bool hovered)
        {
            constexpr float kNudgeIconPx = 8.0f;
            const float halfWidth = 4.0f * kNudgeIconPx / 12.0f;
            const float halfHeight = 4.5f * kNudgeIconPx / 24.0f;
            std::array<ImVec2, 3> points{};
            if (direction == 0)
            {
                points[0] = { center.x, center.y + halfHeight };
                points[1] = { center.x - halfWidth, center.y - halfHeight };
                points[2] = { center.x + halfWidth, center.y - halfHeight };
            }
            else if (direction == 1)
            {
                points[0] = { center.x, center.y - halfHeight };
                points[1] = { center.x - halfWidth, center.y + halfHeight };
                points[2] = { center.x + halfWidth, center.y + halfHeight };
            }
            else if (direction == 2)
            {
                points[0] = { center.x + halfWidth, center.y };
                points[1] = { center.x - halfWidth, center.y - halfHeight };
                points[2] = { center.x - halfWidth, center.y + halfHeight };
            }
            else
            {
                points[0] = { center.x - halfWidth, center.y };
                points[1] = { center.x + halfWidth, center.y - halfHeight };
                points[2] = { center.x + halfWidth, center.y + halfHeight };
            }
            drawList->AddTriangleFilled(points[0], points[1], points[2], hovered ? IM_COL32_WHITE : kIcon);
        }

        bool mouseInRect(const ImVec2& mouse, const ImRect& rect)
        {
            return mouse.x >= rect.Min.x && mouse.x <= rect.Max.x &&
                   mouse.y >= rect.Min.y && mouse.y <= rect.Max.y;
        }
    } // namespace

    void ViewCubeWidget::setBgfxRenderer(ViewCubeBgfxRenderer* renderer)
    {
        m_renderer = renderer;
    }

    ViewCubeResult ViewCubeWidget::render(const char* strId,
                                          const ImVec2& size,
                                          const glm::mat3& viewRotation,
                                          const glm::mat3& ucsRotation,
                                          const ViewCubeOptions& options)
    {
        ViewCubeResult result;
        ImGui::InvisibleButton(strId, size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const ImVec2 rectMin = ImGui::GetItemRectMin();
        const ImVec2 rectMax = ImGui::GetItemRectMax();
        const ImVec2 center{ (rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f };
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool widgetHovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        const float pickerHeight = options.showUcsPicker ? ImGui::GetFrameHeight() : 0.0f;
        bool pickerHovered = false;
        const float navSize = kViewCubePx * kViewCubeScale * 2.0f * kNavInset; // 120.96 px
        (void)size;
        const float cubeHalf = navSize * 0.25f;
        const float radius = cubeHalf;
        const ImVec2 navCenter{ center.x, center.y - pickerHeight * 0.5f };
        const ImRect navRect{ ImVec2(navCenter.x - navSize * 0.5f, navCenter.y - navSize * 0.5f),
                              ImVec2(navCenter.x + navSize * 0.5f, navCenter.y + navSize * 0.5f) };

        if (options.showUcsPicker)
        {
            ImGui::SetCursorScreenPos(ImVec2(rectMin.x, navRect.Max.y + 2.0f));
            ImGui::PushItemWidth(size.x);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(30, 38, 48, 220));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 52, 64, 230));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(58, 76, 94, 230));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(76, 96, 116, 230));
            bool changed = false;
            const std::string preview = options.activeUcs ? options.activeUcs : "WCS";
            if (ImGui::BeginCombo("##CadUcsPicker", preview.c_str()))
            {
                if (options.ucsNames)
                {
                    for (unsigned i = 0; i < options.ucsNames->size(); ++i)
                    {
                        const std::string& name = (*options.ucsNames)[i];
                        const bool selected = name == preview;
                        if (ImGui::Selectable(name.c_str(), selected))
                        {
                            changed = true;
                            result.action.kind = ViewCubeActionKind::UcsChanged;
                            result.action.ucsIndex = static_cast<int>(i);
                            result.action.ucs = name;
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                ImGui::EndCombo();
            }
            pickerHovered = ImGui::IsItemHovered();
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();
            if (changed)
            {
                return result;
            }
        }

        const glm::mat3 cubeRotation = viewRotation * ucsRotation;
        std::vector<PolygonData> polygons = buildPolygons(cubeRotation, navRect.GetCenter(), radius);

        std::array<ImVec2, 6> faceCenters{};
        std::array<float, 6> faceDepths{};
        for (unsigned face = 0; face < 6; ++face)
        {
            const glm::vec3 eye = cubeRotation * ViewCubeWidget::snapDirection(regionById(static_cast<int>(face)));
            faceCenters[face] = { navRect.GetCenter().x + eye.x * radius,
                                  navRect.GetCenter().y - eye.y * radius };
            faceDepths[face] = eye.z;
        }

        int hoveredId = -1;
        bool controlHover = false;
        ViewCubeAction action;

        // Nav control rects.  Computed once: the hover gate below uses them
        // for hit-testing, and the always-on drawing pass later in this
        // function reuses them (OpenCADStudio's iced buttons never hide).
        const float buttonSize = 16.0f;
        const ImRect homeRect{ ImVec2(navRect.Min.x + 3.0f, navRect.Min.y + 3.0f),
                               ImVec2(navRect.Min.x + 3.0f + buttonSize, navRect.Min.y + 3.0f + buttonSize) };
        const float rax = navRect.Max.x - 2.0f * buttonSize - 4.0f;
        const float rbx = navRect.Max.x - buttonSize - 2.0f;
        const ImRect undoRect{ ImVec2(rax, navRect.Min.y + 2.0f), ImVec2(rax + buttonSize, navRect.Min.y + 2.0f + buttonSize) };
        const ImRect redoRect{ ImVec2(rbx, navRect.Min.y + 2.0f), ImVec2(rbx + buttonSize, navRect.Min.y + 2.0f + buttonSize) };

        const float nudgeSize = 9.0f;
        const float nr = cubeHalf + 6.0f;
        const ImVec2 nc = navRect.GetCenter();
        const auto nudgeRect = [&](float x, float y)
        {
            return ImRect{ ImVec2(x - nudgeSize * 0.5f, y - nudgeSize * 0.5f),
                           ImVec2(x + nudgeSize * 0.5f, y + nudgeSize * 0.5f) };
        };
        const ImRect nudgeUp = nudgeRect(nc.x, nc.y - nr);
        const ImRect nudgeDown = nudgeRect(nc.x, nc.y + nr);
        const ImRect nudgeLeft = nudgeRect(nc.x - nr, nc.y);
        const ImRect nudgeRight = nudgeRect(nc.x + nr, nc.y);

        const std::array<const ImRect*, 7> controlRects{{
            &homeRect, &undoRect, &redoRect, &nudgeUp, &nudgeDown, &nudgeLeft, &nudgeRight
        }};
        int controlIndex = -1;
        for (unsigned i = 0; i < controlRects.size(); ++i)
        {
            if (mouseInRect(mouse, *controlRects[i]))
            {
                controlIndex = static_cast<int>(i);
                break;
            }
        }

        if (widgetHovered && !pickerHovered)
        {
            if (options.showControls && controlIndex >= 0)
            {
                controlHover = true;
                if (clicked)
                {
                    switch (controlIndex)
                    {
                    case 0: action.kind = ViewCubeActionKind::Home; break;
                    case 1: action.kind = ViewCubeActionKind::RollLeft; break;
                    case 2: action.kind = ViewCubeActionKind::RollRight; break;
                    case 3: action.kind = ViewCubeActionKind::NudgeUp; break;
                    case 4: action.kind = ViewCubeActionKind::NudgeDown; break;
                    case 5: action.kind = ViewCubeActionKind::NudgeLeft; break;
                    case 6: action.kind = ViewCubeActionKind::NudgeRight; break;
                    default: break;
                    }
                    result.action = action;
                }
            }

            if (!controlHover)
            {
                // Use the same projected polygons used by ImGui fallback/highlight.
                hoveredId = hitTestRegion(mouse, cubeRotation, navRect.GetCenter(), radius);
                if (hoveredId < 0)
                {
                    // Compass letters sit on the ring at kCardinalR.  Like
                    // OpenCADStudio's hit_test_cardinal, accept the nearest
                    // projected letter anchor within 0.34 * cubeHalf after the
                    // cube test misses (bounded by the nav inset box).  The
                    // previous center-zone gate made the letters unclickable.
                    const ImVec2 navCenter = navRect.GetCenter();
                    const float inset = cubeHalf * kNavInset;
                    if (std::abs(mouse.x - navCenter.x) <= inset &&
                        std::abs(mouse.y - navCenter.y) <= inset)
                    {
                        float best = std::numeric_limits<float>::max();
                        int cardinal = -1;
                        for (unsigned i = 0; i < 4; ++i)
                        {
                            // Same 3D anchor the label is drawn with, so the hit
                            // area follows the letters onto the foreshortened band.
                            const glm::vec3 direction = ViewCubeWidget::cardinalDirection(static_cast<int>(i));
                            const glm::vec3 eye = viewRotation * glm::vec3{ direction.x * kCardinalR, direction.y * kCardinalR, kRingZ };
                            const float distance = glm::length(glm::vec2{ mouse.x - (navCenter.x + eye.x * radius),
                                                                          mouse.y - (navCenter.y - eye.y * radius) });
                            if (distance < cubeHalf * 0.34f && distance < best)
                            {
                                best = distance;
                                cardinal = static_cast<int>(i);
                            }
                        }
                        if (cardinal >= 0)
                        {
                            if (clicked)
                            {
                                result.action.kind = ViewCubeActionKind::Cardinal;
                                result.action.region = cardinalRegion(cardinal);
                            }
                            result.hasHover = true;
                            result.hoveredRegion = cardinalRegion(cardinal);
                        }
                    }
                }
                else
                {
                    result.hasHover = true;
                    result.hoveredRegion = makeRegion(hoveredId, options);
                    if (clicked)
                    {
                        result.clicked = true;
                        result.clickedRegion = result.hoveredRegion;
                        result.action.kind = ViewCubeActionKind::Region;
                        result.action.region = result.clickedRegion;
                    }
                }
            }
        }

        const bool depthTestedLabels = m_renderer && m_renderer->isValid() && m_renderer->canRenderText();
        std::vector<ViewCubeLabelVertex> labelVertices;
        if (depthTestedLabels)
        {
            const ImVec2 origin = navRect.Min;
            const ImVec2 cubeCenter = navRect.GetCenter();
            for (unsigned face = 0; face < 6; ++face)
            {
                if (faceDepths[face] < 0.12f)
                {
                    continue;
                }
                const glm::vec3 normal = ViewCubeWidget::snapDirection(
                    regionById(static_cast<int>(face)));
                glm::vec3 u;
                glm::vec3 v;
                faceLabelAxes(static_cast<int>(face), u, v);
                appendOrientedTextVertices(
                    labelVertices, origin, cubeCenter, radius, cubeRotation,
                    normal * 1.002f, u, v,
                    options.faceLabels[face],
                    IM_COL32(13, 20, 33, 255),
                    kLabelHeight * radius, false);
            }

            const std::array<const char*, 4> names{ "N", "E", "S", "W" };
            const std::array<glm::vec3, 4> directions{{
                { 0.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f },
                { 0.0f, -1.0f, 0.0f }, {-1.0f, 0.0f, 0.0f }
            }};
            for (unsigned index = 0; index < directions.size(); ++index)
            {
                // Project through the same view rotation the ring uses, so the
                // letters ride the foreshortened band instead of a screen-space
                // circle.  inkCentered: the reference centres the rasterised ink
                // box of each label, not ImGui's em box.
                const glm::vec3 center{
                    directions[index].x * kCardinalR,
                    directions[index].y * kCardinalR,
                    kRingZ + 0.004f
                };
                appendOrientedTextVertices(
                    labelVertices, origin, cubeCenter, radius + 1 , viewRotation,
                    center, glm::vec3{ 1.0f, 0.0f, 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f },
                    names[index], IM_COL32(13, 20, 33, 255),
                    kLabelHeight * radius, true);
            }
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(rectMin, rectMax, true);
        if (m_renderer && m_renderer->isValid())
        {
            m_renderer->render(navRect.GetSize(), cubeRotation, viewRotation,
                               hoveredId, labelVertices);
            drawList->AddImage(m_renderer->textureId(), navRect.Min, navRect.Max);
        }
        else
        {
            drawRingFallback(drawList, viewRotation, navRect.GetCenter(), radius);
            drawPolygons(drawList, polygons, hoveredId, true);
        }

        if (!depthTestedLabels)
        {
            drawRingLabels(drawList, viewRotation, navRect.GetCenter(), radius);
        }

        if (!depthTestedLabels)
        {
            for (unsigned face = 0; face < 6; ++face)
            {
                if (faceDepths[face] < 0.12f)
                {
                    continue;
                }
                drawTextWithHalo(drawList, faceCenters[face],
                                 options.faceLabels[face],
                                 static_cast<int>(face) == hoveredId ? IM_COL32(255, 255, 255, 255) : IM_COL32(13, 20, 33, 255),
                                 13.0f);
            }
        }

        // Home / roll / nudge controls are drawn every frame like
        // OpenCADStudio's iced buttons; hover only drives the highlight.
        if (options.showControls)
        {
            drawIconButton(drawList, homeRect, controlIndex == 0);
            drawHomeIcon(drawList, homeRect.GetCenter(), controlIndex == 0);
            drawIconButton(drawList, undoRect, controlIndex == 1);
            drawRollIcon(drawList, undoRect.GetCenter(), false, controlIndex == 1);
            drawIconButton(drawList, redoRect, controlIndex == 2);
            drawRollIcon(drawList, redoRect.GetCenter(), true, controlIndex == 2);
            drawIconButton(drawList, nudgeUp, controlIndex == 3);
            drawNudgeIcon(drawList, nudgeUp.GetCenter(), 0, controlIndex == 3);
            drawIconButton(drawList, nudgeDown, controlIndex == 4);
            drawNudgeIcon(drawList, nudgeDown.GetCenter(), 1, controlIndex == 4);
            drawIconButton(drawList, nudgeLeft, controlIndex == 5);
            drawNudgeIcon(drawList, nudgeLeft.GetCenter(), 2, controlIndex == 5);
            drawIconButton(drawList, nudgeRight, controlIndex == 6);
            drawNudgeIcon(drawList, nudgeRight.GetCenter(), 3, controlIndex == 6);
        }
        drawList->PopClipRect();


        if (result.hasHover && !controlHover)
        {
            ImGui::SetTooltip("%s", result.hoveredRegion.label);
        }
        return result;
    }

    ViewCubeRegion ViewCubeWidget::regionById(int id)
    {
        ViewCubeRegion region;
        if (id < 0 || id >= 26)
        {
            return region;
        }
        region.id = static_cast<unsigned char>(id);
        region.label = regionLabelById(id);
        if (id < 6)
        {
            region.kind = RegionKind::Face;
            region.index = static_cast<unsigned char>(id);
        }
        else if (id < 18)
        {
            region.kind = RegionKind::Edge;
            region.index = static_cast<unsigned char>(id - 6);
        }
        else
        {
            region.kind = RegionKind::Corner;
            region.index = static_cast<unsigned char>(id - 18);
        }
        return region;
    }

    glm::vec3 ViewCubeWidget::snapDirection(const ViewCubeRegion& region)
    {
        if (region.kind == RegionKind::Face)
        {
            switch (region.index)
            {
            case 0: return { 0.0f, 0.0f,  1.0f };
            case 1: return { 0.0f, 0.0f, -1.0f };
            case 2: return { 0.0f, -1.0f, 0.0f };
            case 3: return { 0.0f,  1.0f, 0.0f };
            case 4: return {  1.0f, 0.0f, 0.0f };
            case 5: return { -1.0f, 0.0f, 0.0f };
            default: break;
            }
        }

        static constexpr float coordinates[26][3] = {
            {0.0f, 0.0f,  kE}, {0.0f, 0.0f, -kE}, {0.0f, -kE, 0.0f},
            {0.0f,  kE, 0.0f}, { kE, 0.0f, 0.0f}, {-kE, 0.0f, 0.0f},
            {0.0f, -kM,  kM}, {0.0f,  kM,  kM}, { kM, 0.0f,  kM}, {-kM, 0.0f,  kM},
            {0.0f, -kM, -kM}, {0.0f,  kM, -kM}, { kM, 0.0f, -kM}, {-kM, 0.0f, -kM},
            { kM, -kM, 0.0f}, {-kM, -kM, 0.0f}, { kM,  kM, 0.0f}, {-kM,  kM, 0.0f},
            { kM, -kM,  kM}, {-kM, -kM,  kM}, { kM,  kM,  kM}, {-kM,  kM,  kM},
            { kM, -kM, -kM}, {-kM, -kM, -kM}, { kM,  kM, -kM}, {-kM,  kM, -kM}
        };
        const int id = std::clamp(static_cast<int>(region.id), 0, 25);
        const glm::vec3 centroid{ coordinates[id][0], coordinates[id][1], coordinates[id][2] };
        return glm::normalize(centroid);
    }

    ViewCubeRegion ViewCubeWidget::oppositeRegion(const ViewCubeRegion& region)
    {
        const glm::vec3 target = snapDirection(region);
        int bestId = region.id;
        float bestDistance = std::numeric_limits<float>::max();
        for (int id = 0; id < 26; ++id)
        {
            const glm::vec3 candidate = snapDirection(regionById(id));
            const float distance = glm::length(candidate + target);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestId = id;
            }
        }
        return regionById(bestId);
    }

    ViewCubeRegion ViewCubeWidget::cardinalRegion(int cardinalIndex)
    {
        const int index = std::clamp(cardinalIndex, 0, 3);
        ViewCubeRegion region;
        region.kind = RegionKind::Face;
        region.id = 0;
        region.index = static_cast<unsigned char>(index);
        static constexpr const char* labels[4] = { "N", "E", "S", "W" };
        region.label = labels[index];
        return region;
    }

    glm::vec3 ViewCubeWidget::cardinalDirection(int cardinalIndex)
    {
        switch (std::clamp(cardinalIndex, 0, 3))
        {
        case 0: return { 0.0f,  1.0f, 0.0f };
        case 1: return { 1.0f,  0.0f, 0.0f };
        case 2: return { 0.0f, -1.0f, 0.0f };
        case 3: return {-1.0f,  0.0f, 0.0f };
        default: break;
        }
        return { 0.0f, 0.0f, 1.0f };
    }
} // namespace cadui
