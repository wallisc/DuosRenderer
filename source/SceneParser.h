#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>

namespace SceneParser
{
    struct Film
    {
        UINT m_ResolutionX;
        UINT m_ResolutionY;
        std::string m_Filename;
    };

    struct Camera
    {
        float m_FieldOfView;
    };

    struct Material
    {
        std::string m_MaterialName;
        float m_DiffuseRed;
        float m_DiffuseGreen;
        float m_DiffuseBlue;
    };

    struct Scene
    {
        Camera m_Camera;
        Film m_Film;
        std::unordered_map<std::string, Material> m_Materials;
    };

    class BadFormatException : public std::exception
    {
    public:
        BadFormatException(char const* const errorMessage) : std::exception(errorMessage) {}
    };

    class SceneParserClass
    {
        virtual void Parse(std::string filename, Scene &outputScene) = 0;
    };
};
