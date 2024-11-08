#include "app.h"
#include "backend/context.h"
#include "client/client.h"
#include "event/windowCloseEvent.h"
#include "ui/window.h"
#include "ui/uiLayer.h"

namespace l2viz {

Level2Visualizer::Level2Visualizer()
    : m_shouldRun(false)
{
    initWindow();
    initContext();
    initUILayer();
}

Level2Visualizer::~Level2Visualizer()
{
    // release ui layer
    m_UILayer.reset();

    // destroy the context
    Context::destroy();

    // release the window
    m_window.reset();

    // wait for client to exit
    if (m_clientThread.joinable()) {
        m_clientThread.join();
    }
}

void Level2Visualizer::initWindow()
{
    m_window = Window::create(1280, 720, std::bind(&Level2Visualizer::onEvent, this, std::placeholders::_1));
}

void Level2Visualizer::initContext()
{
    Context::init(m_window);
}

void Level2Visualizer::initUILayer()
{
    m_UILayer = UILayer::create();
}

void Level2Visualizer::run()
{
    m_shouldRun = true;

    startClientThread();

    // ===============================================================================

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};

    // Main loop
    std::unique_lock lock(m_mtExit);
    while (m_shouldRun) {
        // done checking flag, unlock
        lock.unlock();

        @autoreleasepool {
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
            // tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
            // your main application, or clear/overwrite your copy of the mouse data.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
            // data to your main application, or clear/overwrite your copy of the
            // keyboard data. Generally you may always pass all inputs to dear imgui,
            // and hide them from your application based on those two flags.
            m_window->pollEvents();

            Context::newFrame();

            m_UILayer->onUpdate();

            Context::endFrame();
        }

        // lock again before checking
        lock.lock();
    }

    // event loop exited, signal client to exit
    m_shouldRun = false;
    lock.unlock();

    m_cvExit.notify_all();
}

void Level2Visualizer::onEvent(std::unique_ptr<Event> event)
{
    EventDispatcher dispatcher(event.get());
    dispatcher.dispatch<WindowCloseEvent>(std::bind(&Level2Visualizer::onWindowClose, this, std::placeholders::_1));
}

bool Level2Visualizer::onWindowClose(WindowCloseEvent* event)
{
    {
        std::lock_guard lock(m_mtExit);
        m_shouldRun = false;
    }
    return true;
}

void Level2Visualizer::startClientThread() {
    m_clientThread = std::thread(
        [this] {

            // start the client
            Client client;
            client.startStreamer();

            // TEST: testing pause resume
            {
                // std::this_thread::sleep_for(std::chrono::seconds(5));
                // client.pauseStreamer();
                // std::this_thread::sleep_for(std::chrono::seconds(5));
                // client.resumeStreamer();
            }

            // wait for the exit signal
            std::unique_lock<std::mutex> lock(m_mtExit);
            m_cvExit.wait(lock, [this] { return !m_shouldRun; });

            // stop the streamer
            client.stopStreamer();

            // client's destructor will get called when it goes out of scope
        }
    );
}

}
