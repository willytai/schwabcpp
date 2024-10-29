#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <cstdint>
#include <functional>
#include <memory>

// forward declaration
struct GLFWwindow;

namespace l2viz {

// forward declaration
class Event;

class Window {
public:

    typedef std::function<void(std::unique_ptr<Event>)> EventCallbackFn;

    struct Data
    {
        uint32_t width;
        uint32_t height;
        EventCallbackFn callback;
    };

                                    Window(uint32_t width, uint32_t height, EventCallbackFn callback);
                                    ~Window();

    bool                            shouldClose() const;
    void                            onUpdate();
    void                            pollEvents();
    GLFWwindow*                     getNativeWindow() const { return m_window; }

    static std::shared_ptr<Window>  create(uint32_t width, uint32_t height, EventCallbackFn callback);

private:
    GLFWwindow*                     m_window;
    Data                            m_data;
};

}

#endif
