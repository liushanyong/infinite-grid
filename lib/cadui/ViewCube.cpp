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
        constexpr float kCardinalR = 1.57f;

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

            const std::array<std::array<glm::vec3, 3>, 8> corners{{
                {{ { kF,  kF,  kE}, { kF,  kE,  kF}, { kE,  kF,  kF} }},
                {{ {-kF,  kF,  kE}, {-kF,  kE,  kF}, {-kE,  kF,  kF} }},
                {{ { kF,  kF, -kE}, { kF,  kE, -kF}, { kE,  kF, -kF} }},
                {{ {-kF,  kF, -kE}, {-kF,  kE, -kF}, {-kE,  kF, -kF} }},
                {{ { kF, -kF,  kE}, { kF, -kE,  kF}, { kE, -kF,  kF} }},
                {{ {-kF, -kF,  kE}, {-kF, -kE,  kF}, {-kE, -kF,  kF} }},
                {{ { kF, -kF, -kE}, { kF, -kE, -kF}, { kE, -kF, -kF} }},
                {{ {-kF, -kF, -kE}, {-kF, -kE, -kF}, {-kE, -kF, -kF} }}
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
            int bestId = -1;
            float bestDepth = std::numeric_limits<float>::lowest();
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

        void drawPolygons(ImDrawList* drawList,
                          std::vector<PolygonData>& polygons,
                          int hoveredId,
                          bool fill)
        {
            std::sort(polygons.begin(), polygons.end(), [](const PolygonData& lhs, const PolygonData& rhs)
            {
                return lhs.depth > rhs.depth;
            });

            if (fill)
            {
                for (const PolygonData& polygon : polygons)
                {
                    const ImU32 color = polygon.id == hoveredId ? kHover : kSurface;
                    if (polygon.count == 4)
                    {
                        drawList->AddConvexPolyFilled(polygon.points.data(), 4, color);
                    }
                    else
                    {
                        drawList->AddTriangleFilled(polygon.points[0], polygon.points[1], polygon.points[2], color);
                    }
                }
            }

            for (const PolygonData& polygon : polygons)
            {
                if (polygon.depth >= 0.0f)
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
            if (std::abs(size - ImGui::GetFontSize()) > 0.01f)
            {
                drawList->AddText(nullptr, size, position, color, text);
                return;
            }
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            drawList->AddText(ImVec2(position.x - textSize.x * 0.5f,
                                     position.y - textSize.y * 0.5f), color, text);
        }

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
            for (unsigned i = 0; i < 4; ++i)
            {
                const glm::vec3 eye = rotation * directions[i];
                drawTextCentered(drawList, { center.x + eye.x * radius, center.y - eye.y * radius },
                                 names[i], kRingLabel, 11.0f);
            }
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

        void drawHomeIcon(ImDrawList* drawList, const ImVec2& center, bool hovered)
        {
            const float scale = 13.0f / 24.0f;
            std::array<ImVec2, 12> polygon{{
                iconPoint(center, scale, -9.5f, -8.5f),
                iconPoint(center, scale, 0.0f, -8.5f),
                iconPoint(center, scale, 9.5f, 0.0f),
                iconPoint(center, scale, 6.5f, 0.0f),
                iconPoint(center, scale, 6.5f, 8.5f),
                iconPoint(center, scale, 2.0f, 8.5f),
                iconPoint(center, scale, 2.0f, 2.5f),
                iconPoint(center, scale, -2.0f, 2.5f),
                iconPoint(center, scale, -2.0f, 8.5f),
                iconPoint(center, scale, -6.5f, 8.5f),
                iconPoint(center, scale, -6.5f, 0.0f),
                iconPoint(center, scale, -9.5f, 0.0f)
            }};
            drawList->AddConvexPolyFilled(polygon.data(), static_cast<int>(polygon.size()), kIcon);
            if (hovered)
            {
                drawList->AddPolyline(polygon.data(), static_cast<int>(polygon.size()), IM_COL32_WHITE, ImDrawFlags_Closed, 1.0f);
            }
        }

        void drawRollIcon(ImDrawList* drawList, const ImVec2& center, bool redo, bool hovered)
        {
            const float scale = 12.0f / 24.0f;
            const ImU32 color = hovered ? IM_COL32_WHITE : kIcon;
            drawList->PathClear();
            if (!redo)
            {
                drawList->PathLineTo(iconPoint(center, scale, 8.0f, -4.0f));
                drawList->PathLineTo(iconPoint(center, scale, -2.0f, 1.0f));
                drawList->PathLineTo(iconPoint(center, scale, 8.0f, 6.0f));
                drawList->PathStroke(color, false, 2.2f);
                drawList->PathClear();
                drawList->PathLineTo(iconPoint(center, scale, -2.0f, 1.0f));
                drawList->PathLineTo(iconPoint(center, scale, 8.0f, 1.0f));
                drawList->PathArcTo(iconPoint(center, scale, 8.0f, 7.0f), 6.0f * scale, -1.5707963f, 1.5707963f, 12);
                drawList->PathLineTo(iconPoint(center, scale, 7.0f, 13.0f));
                drawList->PathStroke(color, false, 2.2f);
            }
            else
            {
                drawList->PathLineTo(iconPoint(center, scale, -8.0f, -4.0f));
                drawList->PathLineTo(iconPoint(center, scale, 2.0f, 1.0f));
                drawList->PathLineTo(iconPoint(center, scale, -8.0f, 6.0f));
                drawList->PathStroke(color, false, 2.2f);
                drawList->PathClear();
                drawList->PathLineTo(iconPoint(center, scale, 2.0f, 1.0f));
                drawList->PathLineTo(iconPoint(center, scale, -8.0f, 1.0f));
                drawList->PathArcTo(iconPoint(center, scale, -8.0f, 7.0f), 6.0f * scale, -1.5707963f, 1.5707963f, 12);
                drawList->PathLineTo(iconPoint(center, scale, -7.0f, 13.0f));
                drawList->PathStroke(color, false, 2.2f);
            }
        }

        void drawNudgeIcon(ImDrawList* drawList, const ImVec2& center, int direction, bool hovered)
        {
            const float half = 4.0f;
            std::array<ImVec2, 3> points{};
            if (direction == 0)
            {
                points[0] = { center.x, center.y + half };
                points[1] = { center.x - half, center.y - half };
                points[2] = { center.x + half, center.y - half };
            }
            else if (direction == 1)
            {
                points[0] = { center.x, center.y - half };
                points[1] = { center.x - half, center.y + half };
                points[2] = { center.x + half, center.y + half };
            }
            else if (direction == 2)
            {
                points[0] = { center.x + half, center.y };
                points[1] = { center.x - half, center.y - half };
                points[2] = { center.x - half, center.y + half };
            }
            else
            {
                points[0] = { center.x - half, center.y };
                points[1] = { center.x + half, center.y - half };
                points[2] = { center.x + half, center.y + half };
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
        if (widgetHovered && !pickerHovered)
        {
            if (options.showControls)
            {
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

                const std::array<const ImRect*, 7> rects{{
                    &homeRect, &undoRect, &redoRect, &nudgeUp, &nudgeDown, &nudgeLeft, &nudgeRight
                }};
                int controlIndex = -1;
                for (unsigned i = 0; i < rects.size(); ++i)
                {
                    if (mouseInRect(mouse, *rects[i]))
                    {
                        controlIndex = static_cast<int>(i);
                        break;
                    }
                }

                if (controlIndex >= 0)
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

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
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
            }

            if (!controlHover)
            {
                hoveredId = hitTestPolygons(mouse, polygons);
                if (hoveredId < 0)
                {
                    const glm::vec2 delta{ mouse.x - navRect.GetCenter().x, mouse.y - navRect.GetCenter().y };
                    if (glm::length(delta) <= cubeHalf * 0.34f)
                    {
                        float best = std::numeric_limits<float>::max();
                        int cardinal = 0;
                        for (unsigned i = 0; i < 4; ++i)
                        {
                            const glm::vec3 direction = ViewCubeWidget::cardinalDirection(static_cast<int>(i));
                            const glm::vec3 eye = viewRotation * glm::vec3{ direction.x * kCardinalR, direction.y * kCardinalR, kRingZ };
                            const float distance = glm::length(glm::vec2{ mouse.x - (navRect.GetCenter().x + eye.x * radius),
                                                                          mouse.y - (navRect.GetCenter().y - eye.y * radius) });
                            if (distance < best)
                            {
                                best = distance;
                                cardinal = static_cast<int>(i);
                            }
                        }
                        if (clicked)
                        {
                            result.action.kind = ViewCubeActionKind::Cardinal;
                            result.action.region = cardinalRegion(cardinal);
                        }
                        result.hasHover = true;
                        result.hoveredRegion = cardinalRegion(cardinal);
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

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(rectMin, rectMax, true);
        if (m_renderer && m_renderer->isValid())
        {
            m_renderer->render(navRect.GetSize(), cubeRotation, viewRotation, hoveredId);
            drawList->AddImage(m_renderer->textureId(), navRect.Min, navRect.Max);
            drawPolygons(drawList, polygons, hoveredId, false);
        }
        else
        {
            drawRingFallback(drawList, viewRotation, navRect.GetCenter(), radius);
            drawPolygons(drawList, polygons, hoveredId, true);
        }

        drawRingLabels(drawList, viewRotation, navRect.GetCenter(), radius);

        for (unsigned face = 0; face < 6; ++face)
        {
            if (faceDepths[face] >= -0.03f)
            {
                continue;
            }
            drawTextCentered(drawList, faceCenters[face],
                             options.faceLabels[face],
                             static_cast<int>(face) == hoveredId ? IM_COL32(255, 255, 255, 240) : kEdge,
                             11.0f);
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
