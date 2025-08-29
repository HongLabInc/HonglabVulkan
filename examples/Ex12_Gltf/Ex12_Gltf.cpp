#include "engine/Application.h"

using namespace hlab;

int main()
{
    // Option 1: Use default configuration (same as before)
    // auto app = std::make_unique<Application>();

    // Option 2: GLTF showcase configuration
    // auto app = std::make_unique<Application>(ApplicationConfig::createGltfShowcase());

    // Option 3: Animation demo configuration
    // auto app = std::make_unique<Application>(ApplicationConfig::createAnimationDemo());

    // Option 4: Custom configuration
    ApplicationConfig config;

    // Add multiple models
    // config.models.push_back(
    //    ModelConfig("models/DamagedHelmet.glb", "Helmet")
    //        .setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f))));

    config.models.push_back(
        ModelConfig("characters/Leonard/Bboy Hip Hop Move.fbx", "Dancer")
            .setTransform(glm::rotate(
                glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-6.719f, 0.21f, -1.860f)),
                           glm::vec3(0.012f)),
                glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)))
            .setAnimation(true, 0, 1.0f, true));

    config.models.push_back(
        ModelConfig("models/AmazonLumberyardBistroMorganMcGuire/exterior.obj", "Bistro")
            .setBistroModel(true)
            .setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.01f))));

    // Custom camera position
    // config.camera = CameraConfig(glm::vec3(0.0f, 2.0f, -8.0f), // position
    //                             glm::vec3(0.0f, 0.0f, 0.0f),  // rotation
    //                             glm::vec3(0.0f, 2.0f, 8.0f)   // viewPos
    //);
    config.camera = CameraConfig::forBistro();

    auto app = std::make_unique<Application>(config);

    app->run();
    return 0;
}
