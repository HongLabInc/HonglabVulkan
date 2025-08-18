#include "engine/Context.h"

using namespace hlab;

int main()
{
    vector<const char*> requiredInstanceExtensions = {};
    bool useSwapchain = false;

    Context ctx(requiredInstanceExtensions, useSwapchain);

    return 0;
}
