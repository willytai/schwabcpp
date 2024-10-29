#include "window.h"
#include "../utils/logger.h"
#include "../event/windowCloseEvent.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace l2viz {

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

Window::Window(uint32_t width, uint32_t height, EventCallbackFn callback)
    : m_data({width, height, callback})
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        LOG_FATAL("Failed to initialize glfw.");
    }

    m_window = glfwCreateWindow(
        m_data.width, m_data.height, "Level2Visualizer", nullptr, nullptr
    );

    if (!m_window) {
        LOG_FATAL("Failed to create glfw window.");
    }

    glfwSetInputMode(m_window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
    glfwSetWindowUserPointer(m_window, &m_data);

    // callbacks to pass the event to the app
    glfwSetWindowCloseCallback(m_window, [] (GLFWwindow* window) {
        Data* data = (Data*)glfwGetWindowUserPointer(window);
        data->callback(std::make_unique<WindowCloseEvent>());
    });
}

Window::~Window()
{
    glfwDestroyWindow(m_window);  // this marks the window to be close
    glfwWaitEvents();             // wait for the window to actually close
    glfwTerminate();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window);
}

void Window::onUpdate()
{
    glfwPollEvents();
}

void Window::pollEvents()
{
    glfwPollEvents();
}

std::shared_ptr<Window>
Window::create(uint32_t width, uint32_t height, EventCallbackFn callback)
{
    return std::make_shared<Window>(width, height, callback);
}

}
