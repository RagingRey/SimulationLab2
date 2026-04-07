#pragma once

#include "SceneRuntime.h"
#include <string>
#include <vector>

namespace SimRuntime
{
    struct SceneLoadResult
    {
        bool success = false;
        std::string error;
        std::vector<std::string> warnings;
        SceneRuntime scene;
    };

    class SceneLoaderFlatBuffer
    {
    public:
        static SceneLoadResult LoadFromFile(const std::string& filePath);
    };
}