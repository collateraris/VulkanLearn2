#pragma once

#include <vk_types.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>

#include <vk_engine.h>
#include <vk_light_manager.h>
#include <graphic_pipeline/vk_gi_raytrace_graphics_pipeline.h>
#include <graphic_pipeline/vk_simple_accumulation_graphics_pipeline.h>
#include <vk_camera.h>

//-----------------------------------------------------------------------------
// [SECTION] Example App: Debug Log / ShowExampleAppLog()
//-----------------------------------------------------------------------------

// Usage:
//  static ExampleAppLog my_log;
//  my_log.AddLog("Hello %d world\n", 123);
//  my_log.Draw("title");
struct ImguiAppLog
{
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    bool                AutoScroll;  // Keep scrolling if already at the bottom.

    ImguiAppLog()
    {
        AutoScroll = true;
        Clear();
    }

    void    Clear()
    {
        Buf.clear();
        LineOffsets.clear();
        LineOffsets.push_back(0);
    }

    void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        int old_size = Buf.size();
        va_list args;
        va_start(args, fmt);
        Buf.appendfv(fmt, args);
        va_end(args);
        for (int new_size = Buf.size(); old_size < new_size; old_size++)
            if (Buf[old_size] == '\n')
                LineOffsets.push_back(old_size + 1);
    }

    void    Draw(const char* title, bool* p_open = NULL)
    {
        if (!ImGui::Begin(title, p_open))
        {
            ImGui::End();
            return;
        }

        // Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &AutoScroll);
            ImGui::EndPopup();
        }

        // Main window
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        bool clear = ImGui::Button("Clear");
        ImGui::SameLine();
        bool copy = ImGui::Button("Copy");
        ImGui::SameLine();
        Filter.Draw("Filter", -100.0f);

        ImGui::Separator();

        if (ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (clear)
                Clear();
            if (copy)
                ImGui::LogToClipboard();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            const char* buf = Buf.begin();
            const char* buf_end = Buf.end();
            if (Filter.IsActive())
            {
                // In this example we don't use the clipper when Filter is enabled.
                // This is because we don't have random access to the result of our filter.
                // A real application processing logs with ten of thousands of entries may want to store the result of
                // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
                for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
                {
                    const char* line_start = buf + LineOffsets[line_no];
                    const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    if (Filter.PassFilter(line_start, line_end))
                        ImGui::TextUnformatted(line_start, line_end);
                }
            }
            else
            {
                // The simplest and easy way to display the entire buffer:
                //   ImGui::TextUnformatted(buf_begin, buf_end);
                // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
                // to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
                // within the visible area.
                // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
                // on your side is recommended. Using ImGuiListClipper requires
                // - A) random access into your data
                // - B) items all being the  same height,
                // both of which we can handle since we have an array pointing to the beginning of each line of text.
                // When using the filter (in the block of code above) we don't have random access into the data to display
                // anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
                // it possible (and would be recommended if you want to search through tens of thousands of entries).
                ImGuiListClipper clipper;
                clipper.Begin(LineOffsets.Size);
                while (clipper.Step())
                {
                    for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                    {
                        const char* line_start = buf + LineOffsets[line_no];
                        const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                        ImGui::TextUnformatted(line_start, line_end);
                    }
                }
                clipper.End();
            }
            ImGui::PopStyleVar();

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
            // Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();
    }

// Demonstrate creating a simple log window with basic filtering.
static void ShowFPSLog(Stats stats)
{
    static ImguiAppLog log;
    static bool p_open = true;
    static int counter = 0;

    counter++;

    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Example: Log", &p_open);
    if (counter > 60)
    {   
        counter = 0;
        log.Clear();
        log.AddLog("FPS %d  ; frameCpuAvg %.2f ms ; frameGpuAvg %.2f ms\n triangle count %.1fM trianglesPerSec %.1fB tri/sec",
            int(1000.0 / stats.frameCpuAvg), stats.frameCpuAvg, stats.frameGpuAvg, double(stats.triangleCount) * 1e-6, stats.trianglesPerSec * 1e-9);
    }
    ImGui::End();

    // Actually call in the regular Log helper (which will Begin() into the same window as we just did)
    log.Draw("Example: Log", &p_open);
}

static void EditSun(VulkanLightManager& lightManager, int current_frame_index)
{
    static bool p_open = true;
    static VulkanLightManager::Light* pSun = nullptr;
    static bool bChangedValue = true;

    if (!pSun)
    {
        pSun = lightManager.get_sun_light();
    }

    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("EditSun", &p_open);
    static float direction[4] = { -1.f, -1.f, 0.f, 1.f };
    bChangedValue |= ImGui::InputFloat3("direction", direction);
    static float position[4] = { 2.10f, 0.20f, 0.30f, 0.44f };
    bChangedValue |= ImGui::InputFloat3("position", position);
    static float col1[3] = { 1.0f, 1.0f, 1.0f };
    bChangedValue |= ImGui::ColorEdit3("color", col1);
    if (bChangedValue)
    {
        bChangedValue = false;
        pSun->direction = glm::vec4(direction[0], direction[1], direction[2], 1.);
        pSun->position = glm::vec4(position[0], position[1], position[2], 1.);
        pSun->color = glm::vec4(col1[0], col1[1], col1[2], 1.);
        lightManager.update_light_buffer(current_frame_index);
    }
    ImGui::End();
}


#if GI_RAYTRACER_ON && GBUFFER_ON
static void EditGI(PlayerCamera& camera, VulkanGIShadowsRaytracingGraphicsPipeline& giGP, VulkanSimpleAccumulationGraphicsPipeline& saGP, int current_frame_index, int frameNumber)
{
    static bool p_open = true;
    static bool bChangedValue = true;
    static VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams giParams = {.aoRadius = 0.5, .shadowMult = 0.05, .numRays = 1};
    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Edit GI", &p_open);
    //bChangedValue |= ImGui::InputFloat("AO Radius", &giParams.aoRadius);
    static int numRays = 0;
    bChangedValue |= ImGui::InputInt("Indirect numRays", &numRays);
    bChangedValue |= ImGui::InputFloat("shadow Mult", &giParams.shadowMult);
    giParams.numRays = std::max(0, numRays);
    giParams.frameCount = frameNumber;
    giParams.camPos = glm::vec4(camera.position, 1.f);
    giParams.viewInverse = glm::inverse(camera.get_view_matrix());
    giParams.projInverse = glm::inverse(camera.get_projection_matrix());

    if (bChangedValue)
    {
        bChangedValue = false;
        saGP.reset_accumulation();
    }
  
    giGP.copy_global_uniform_data(giParams, current_frame_index);
    ImGui::End();
}
#endif

static void ShowVkMenu(VulkanEngine& engine)
{
    ImguiAppLog::ShowFPSLog(engine._stats);

    ImguiAppLog::EditSun(engine._lightManager, engine.get_current_frame_index());
#if GI_RAYTRACER_ON && GBUFFER_ON
    ImguiAppLog::EditGI(engine._camera, engine._giRtGraphicsPipeline, engine._simpleAccumGraphicsPipeline, engine.get_current_frame_index(), engine._frameNumber);
#endif
}

};