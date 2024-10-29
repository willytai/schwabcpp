#include "context.h"
#include "../ui/window.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

namespace l2viz {

// hiding objc stuffs here
static struct ContextData {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    CAMetalLayer* layer = nil;
    MTLRenderPassDescriptor* renderPassDescriptor = nil;
    std::shared_ptr<Window> window = nullptr;
} s_contextData;

static struct FrameData {
    id<MTLCommandBuffer> commandBuffer = nil;
    id<MTLRenderCommandEncoder> commandEncoder = nil;
    id<CAMetalDrawable> drawable = nil;
    float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};
} s_frameData;

void Context::init(std::shared_ptr<Window> window)
{
    // -- context data --
    s_contextData.device = MTLCreateSystemDefaultDevice();
    s_contextData.commandQueue = [s_contextData.device newCommandQueue];
    s_contextData.layer = [CAMetalLayer layer];
    s_contextData.layer.device = s_contextData.device;
    s_contextData.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    s_contextData.renderPassDescriptor = [MTLRenderPassDescriptor new];
    s_contextData.window = window;


    // -- window setup --
    NSWindow* win = glfwGetCocoaWindow(window->getNativeWindow());
    win.contentView.layer = s_contextData.layer;
    win.contentView.wantsLayer = YES;


    // -- imgui --

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window->getNativeWindow(), true);
    ImGui_ImplMetal_Init(s_contextData.device);
}

void Context::newFrame()
{
    int width, height;
    glfwGetFramebufferSize(s_contextData.window->getNativeWindow(), &width, &height);
    s_contextData.layer.drawableSize = CGSizeMake(width, height);
    s_frameData.drawable = [s_contextData.layer nextDrawable];
    
    s_frameData.commandBuffer = [s_contextData.commandQueue commandBuffer];
    s_contextData.renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
        s_frameData.clear_color[0] * s_frameData.clear_color[3], s_frameData.clear_color[1] * s_frameData.clear_color[3],
        s_frameData.clear_color[2] * s_frameData.clear_color[3], s_frameData.clear_color[3]);
    s_contextData.renderPassDescriptor.colorAttachments[0].texture = s_frameData.drawable.texture;
    s_contextData.renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    s_contextData.renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    s_frameData.commandEncoder = [s_frameData.commandBuffer
        renderCommandEncoderWithDescriptor:s_contextData.renderPassDescriptor];
    [s_frameData.commandEncoder pushDebugGroup:@"ImGui demo"];
    
    // Start the Dear ImGui frame
    ImGui_ImplMetal_NewFrame(s_contextData.renderPassDescriptor);
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Context::endFrame()
{
    // ImGui Rendering
    ImGui::Render();
    ImGui_ImplMetal_RenderDrawData(
        ImGui::GetDrawData(),
        s_frameData.commandBuffer,
        s_frameData.commandEncoder
    );

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    [s_frameData.commandEncoder popDebugGroup];
    [s_frameData.commandEncoder endEncoding];
    [s_frameData.commandBuffer presentDrawable:s_frameData.drawable];
    [s_frameData.commandBuffer commit];
}

void Context::destroy()
{
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // TODO:
    // release s_contextData?
    s_contextData.window.reset();
}

float* Context::getClearColorPtr()
{
    return s_frameData.clear_color;
}

}
