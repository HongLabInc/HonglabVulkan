#include "engine/Application.h"

using namespace hlab;

int main()
{
    /* Currently, which models to load from beginning is hardcoded in Application class.
     * Is there a way to specify which model (or models) to read here?
     */

    // 안내: 힙 메모리 사용하기 위해 unique_ptr 사용
    auto app = std::make_unique<Application>();

    app->run();

    return 0;
}
