#include "engine/Application.h"

using namespace hlab;

int main()
{
    ApplicationConfig config;

    config.models.push_back(ModelConfig("models/DamagedHelmet.glb", "Helmet")
                                .setTransform(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                                          glm::vec3(1.0f, 0.0f, 0.0f))));

    config.camera = config.camera.forHelmet();

    auto app = std::make_unique<Application>(config);

    app->run();
    return 0;
}
