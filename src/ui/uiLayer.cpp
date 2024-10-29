#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "uiLayer.h"
#include "imgui.h"
#include "../backend/context.h"

namespace l2viz {

static ImGuiWindowFlags  s_windowFlags;
static ImGuiDockNodeFlags s_dockspaceFlags = ImGuiDockNodeFlags_None;
static bool s_dockspaceOpen = false;

UILayer::UILayer()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enabling this causes the ImGui::RenderPlatformWindowsDefault() call the crash

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    // Setup style
    // ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    initCustomStyle();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can
    // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
    // them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
    // need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please
    // handle those errors in your application (e.g. use an assertion, or display
    // an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored
    // into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which
    // ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype
    // for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string
    // literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font =
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
    // nullptr, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != nullptr);

    s_dockspaceOpen = true;

    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    s_windowFlags = ImGuiWindowFlags_NoDocking  |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize   |
                    ImGuiWindowFlags_NoMove     |
                    ImGuiWindowFlags_NoNavFocus |
                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (s_dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode) {
        s_windowFlags |= ImGuiWindowFlags_NoBackground;
    }
}

UILayer::~UILayer()
{

}

void UILayer::onUpdate()
{
    beginDockspace();
    showExample();
    endDockspace();
}

void UILayer::beginDockspace()
{
    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    // Info: set to zero padding and window style
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Window", &s_dockspaceOpen, s_windowFlags);
    ImGui::PopStyleVar(3);

    // DockSpace
    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGuiStyle& style = ImGui::GetStyle();
    float temp = style.WindowMinSize.x;
    style.WindowMinSize.x = 360.0f;
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), s_dockspaceFlags);
    style.WindowMinSize.x = temp;
}

void UILayer::endDockspace()
{
    ImGui::End();
}

void UILayer::showExample()
{
    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about
    // Dear ImGui!).
    static bool show_demo_window = true;
    static bool show_another_window = false;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);
    
    // 2. Show a simple window that we create ourselves. We use a Begin/End
    // pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;
    
        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                       // and append into it.
    
        ImGui::Text("This is some useful text."); // Display some text (you can
                                                  // use a format strings too)
        ImGui::Checkbox("Demo Window",
                        &show_demo_window); // Edit bools storing our window
                                            // open/close state
        ImGui::Checkbox("Another Window", &show_another_window);
    
        ImGui::SliderFloat(
            "float", &f, 0.0f,
            1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3(
            "clear color",
            Context::getClearColorPtr()); // Edit 3 floats representing a color
    
        if (ImGui::Button( "Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
    
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }
    
    // 3. Show another simple window.
    if (show_another_window) {
        ImGui::Begin(
            "Another Window",
            &show_another_window); // Pass a pointer to our bool variable (the
                                   // window will have a closing button that
                                   // will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }

}

void UILayer::initCustomStyle()
{
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = colors[ImGuiCol_WindowBg] + ImVec4(0.05f, 0.05f, 0.05f, 0.00f);
    colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // does't really need to be changed
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // titile bar
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg] + ImVec4(0.05f, 0.05f, 0.05f, 0.0f);
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_TitleBg];
    colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg] * ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
    
    // tab
    colors[ImGuiCol_Tab] = colors[ImGuiCol_TitleBg] + ImVec4(0.05f, 0.05f, 0.05f, 0.0f);
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_Tab] + ImVec4(0.05f, 0.05f, 0.05f, 0.0f);
    colors[ImGuiCol_TabActive] = colors[ImGuiCol_Tab] + ImVec4(0.15f, 0.15f, 0.15f, 0.0f);
    colors[ImGuiCol_TabUnfocused] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabActive] - ImVec4(0.02f, 0.02f, 0.02f, 0.0f);
    
    // header
    colors[ImGuiCol_Header] = colors[ImGuiCol_PopupBg] + ImVec4(0.1f, 0.1f, 0.1f, 0.0f);
    colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_Header] + ImVec4(0.1f, 0.1f, 0.1f, 0.0f);
    colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_Header] + ImVec4(0.05f, 0.05f, 0.05f, 0.0f);
    
    // frame
    colors[ImGuiCol_FrameBg] = colors[ImGuiCol_Header];
    colors[ImGuiCol_FrameBgHovered] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_FrameBgActive] = colors[ImGuiCol_HeaderActive];
    
    // button
    colors[ImGuiCol_Button] = colors[ImGuiCol_FrameBg];
    colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgHovered];
    colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_FrameBgActive];
    
    // seperator
    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_SeparatorHovered] + ImVec4(0.2f, 0.2f, 0.2f, 0.0f);
    
    // slider
    colors[ImGuiCol_SliderGrab] = ImVec4(0.60f, 0.70f, 0.878f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.75f, 0.80f, 0.88f, 1.00f);
    
    colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_HeaderActive] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

std::unique_ptr<UILayer>
UILayer::create()
{
    return std::make_unique<UILayer>();
}

}
