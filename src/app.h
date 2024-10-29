#ifndef __APP_H__
#define __APP_H__

#include <thread>
#include <memory>

namespace l2viz {

class Client;
class Event;
class Window;
class WindowCloseEvent;
class UILayer;

class Level2Visualizer {
public:
                                Level2Visualizer();
                                ~Level2Visualizer();

    void                        run();

private:
    void                        initWindow();
    void                        initContext();
    void                        initUILayer();

    void                        startClientThread();

private:
    void                        onEvent(std::unique_ptr<Event> event);
    bool                        onWindowClose(WindowCloseEvent* event);

private:
    bool                        m_shouldRun;
    std::mutex                  m_mtExit;
    std::condition_variable     m_cvExit;
    std::thread                 m_clientThread;

    std::shared_ptr<Window>     m_window;

    std::unique_ptr<UILayer>    m_UILayer;
};

}

#endif
