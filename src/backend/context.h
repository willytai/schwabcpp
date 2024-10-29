#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <memory>

namespace l2viz {

    class Window;

    namespace Context {

        void init(std::shared_ptr<Window> window);
        void destroy();

        void newFrame();
        void endFrame();

        float* getClearColorPtr();
    }

}

#endif
