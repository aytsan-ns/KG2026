#include "DxApp.h"

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
    DxApp app;
    if (!app.Init(hInstance))
        return -1;

    return app.Run();
}
