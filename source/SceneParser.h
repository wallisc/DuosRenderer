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

    struct Vector2
    {
        union {
            struct {
                float x, y;
            };
            struct {
                float u, v;
            };
        };
    };

    struct Vector3
    {
        union {
            struct {
                float x, y, z;
            };
            struct {
                float r, g, b;
            };
        };
    };

    struct Vertex
    {
        Vector3 Normal;
        Vector3 Position;
        Vector2 UV;
    };

    struct Mesh
    {
        Material *m_pMaterial;
        std::vector<int> m_IndexBuffer;
        std::vector<Vertex> m_VertexBuffer;
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
