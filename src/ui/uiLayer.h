#ifndef __UI_LAYER_H__
#define __UI_LAYER_H__

#include <memory>

namespace l2viz {

class UILayer {
public:
    UILayer();
    ~UILayer();

    void onUpdate();

    static std::unique_ptr<UILayer> create();

private:
    void beginDockspace();
    void endDockspace();

private:
    void initCustomStyle();
    void showExample();
};

}

#endif
