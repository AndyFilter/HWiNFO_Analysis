#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

namespace GUI {
    HWND Setup(int (*OnGuiFunc)());
    void Destroy();
    int DrawFrame();
    inline void (*OnFileDrop)(WPARAM);
}