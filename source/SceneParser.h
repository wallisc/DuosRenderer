#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>

namespace SceneParser
{
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
        Vector3(float nX, float nY, float nZ) : x(nX), y(nY), z(nZ) {}
        Vector3() : Vector3(0, 0, 0) {}


        union {
            struct {
                float x, y, z;
            };
            struct {
                float r, g, b;
            };
        };
    };

    struct Film
    {
        UINT m_ResolutionX;
        UINT m_ResolutionY;
        std::string m_Filename;
    };

    struct Camera
    {
        // In Degrees. The is the narrower of the view frustrums width/height
        float m_FieldOfView;
        float m_NearPlane;
        float m_FarPlane;
        Vector3 m_Position;
        Vector3 m_LookAt;
        Vector3 m_Up;
    };

    struct Material
    {
        std::string m_MaterialName;
        Vector3 m_Diffuse;
        std::string m_DiffuseTextureFilename;
    };

    struct Vertex
    {
        Vector3 Normal;
        Vector3 Position;
        Vector3 Tangents;
        Vector2 UV;
    };

    struct Mesh
    {
        Material *m_pMaterial;
        std::vector<int> m_IndexBuffer;
        std::vector<Vertex> m_VertexBuffer;

        bool m_AreTangentsValid;
    };

    struct AreaLight
    {
        AreaLight(Vector3 LightColor) : m_LightColor(LightColor) {}

        Mesh m_Mesh;
        Vector3 m_LightColor;
    };

    struct Scene
    {
        Camera m_Camera;
        Film m_Film;
        std::unordered_map<std::string, Material> m_Materials;
        std::vector<AreaLight> m_AreaLights;
        std::vector<Mesh> m_Meshes;
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
