#include "pch.h"
#include "PBRTParser.h"

using namespace SceneParser;
using namespace std;

namespace PBRTParser
{
    void ThrowIfTrue(bool expression, std::string errorMessage)
    {
        if (expression)
        {
            throw new BadFormatException(errorMessage.c_str());
        }
    }

    void PBRTParser::Parse(std::string filename, Scene &outputScene)
    {
        ifstream fileStream(filename);

        if (!fileStream.good())
        {
            assert(false); // file not found
        }

        InitializeDefaults(outputScene);

        while (fileStream.good())
        {
            std::string firstWord;
            fileStream >> firstWord;
            if (firstWord.size() == 0)
            {
                continue;
            }
            else if (!firstWord.compare("Film"))
            {
                ParseFilm(fileStream, outputScene);
            }
            else if (!firstWord.compare("Camera"))
            {
                ParseCamera(fileStream, outputScene);
            }
            else if (!firstWord.compare("WorldBegin"))
            {
                ParseWorld(fileStream, outputScene);
            }
        }
    }

    void PBRTParser::ParseWorld(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        while (fileStream.good())
        {
            std::string firstWord;
            fileStream >> firstWord;
            if (firstWord.size() == 0)
            {
                continue;
            }
            else if (!firstWord.compare("MakeNamedMaterial"))
            {
                ParseMaterial(fileStream, outputScene);
            }
            else if (!firstWord.compare("NamedMaterial"))
            {
                ParseMesh(fileStream, outputScene);
            }
            else if (!firstWord.compare("WorldEnd"))
            {
                break;
            }
        }
    }

    void PBRTParser::ParseCamera(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        char *pTempBuffer;
        size_t bufferSize;
        GetTempCharBuffer(&pTempBuffer, bufferSize);

        fileStream.getline(pTempBuffer, bufferSize);

        UINT argCount = sscanf_s(pTempBuffer, " \"perspective\" \"float fov\" \[ %f \]",
            &outputScene.m_Camera.m_FieldOfView);

        ThrowIfTrue(argCount != 1, "Camera arguments not formatted correctly");
    }

    void PBRTParser::ParseFilm(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        char *pTempBuffer;
        size_t bufferSize;
        GetTempCharBuffer(&pTempBuffer, bufferSize);

        char fileName[PBRTPARSER_STRINGBUFFERSIZE];
        fileStream.getline(pTempBuffer, bufferSize);

        UINT argCount = sscanf_s(pTempBuffer, " \"image\" \"integer xresolution\" \[ %u \] \"integer yresolution\" \[ %u \] \"string filename\" \[ \"%s\" \]", 
            &outputScene.m_Film.m_ResolutionX,
            &outputScene.m_Film.m_ResolutionY,
            fileName, ARRAYSIZE(fileName));

        ThrowIfTrue(argCount != 3, "Film arguments not formatted correctly");
        
        // Sometimes scanf pulls more than it needs to, make sure to clean up 
        // any extra characters on the file name
        string correctedFileName(fileName);
        if (correctedFileName[correctedFileName.size() - 1] == '"')
        {
            correctedFileName = correctedFileName.substr(0, correctedFileName.size() - 1);
        }

        outputScene.m_Film.m_Filename = correctedFileName;
    }

    void PBRTParser::ParseMaterial(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        char *pTempBuffer;
        size_t bufferSize;
        GetTempCharBuffer(&pTempBuffer, bufferSize);

        fileStream.getline(pTempBuffer, bufferSize);
        Material material;
        char materialName[PBRTPARSER_STRINGBUFFERSIZE];
        char materialType[PBRTPARSER_STRINGBUFFERSIZE];
        UINT argCount = sscanf_s(pTempBuffer, " \"%s \"string type\" \[ \"%s \] \"rgb Kd\" \[ %f %f %f \]",
            materialName,
            ARRAYSIZE(materialName),
            materialType,
            ARRAYSIZE(materialType),
            &material.m_Diffuse.r,
            &material.m_Diffuse.g,
            &material.m_Diffuse.b);
        material.m_MaterialName = CorrectNameString(materialName);

        ThrowIfTrue(argCount != 5, "Material arguments not formatted correctly");

        outputScene.m_Materials[material.m_MaterialName] = material;
    }

    void PBRTParser::ParseMesh(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        char *pTempBuffer;
        size_t bufferSize;
        GetTempCharBuffer(&pTempBuffer, bufferSize);

        fileStream.getline(pTempBuffer, bufferSize);
        char materialName[PBRTPARSER_STRINGBUFFERSIZE];
        UINT argCount = sscanf_s(pTempBuffer, " \"%s \"",
            materialName,
            ARRAYSIZE(materialName));

        ThrowIfTrue(argCount != 1, "MakeNamedMaterial arguments not formatted correctly");

        outputScene.m_Meshes.push_back(Mesh());
        Mesh &mesh = outputScene.m_Meshes[outputScene.m_Meshes.size() - 1];
        string correctedMaterialName = CorrectNameString(materialName);
        mesh.m_pMaterial = &outputScene.m_Materials[correctedMaterialName];
        ThrowIfTrue(mesh.m_pMaterial == nullptr, "Material name not found");

        ParseShape(fileStream, outputScene, mesh);
    }

    void PBRTParser::ParseShape(std::ifstream &fileStream, SceneParser::Scene &outputScene, SceneParser::Mesh &mesh)
    {
        string parsedWord;
        fileStream >> parsedWord;
        ThrowIfTrue(parsedWord.compare("Shape"), "Geometry expected to be prepended with \"Shape\"");

        fileStream >> parsedWord;
        ThrowIfTrue(parsedWord.compare("\"trianglemesh\""), "Only TriangleMesh supported topology at the moment");

        fileStream >> parsedWord;
        if (!parsedWord.compare("\"integer"))
        {
            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("indices\""), "\"integer\" expected to be followed up with \"integer\"");

            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("["), "\"indices\" expected to be followed up with \"[\"");

            while (fileStream.good())
            {
                int index;
                fileStream >> index;
                if (fileStream.good())
                {
                    mesh.m_IndexBuffer.push_back(index);
                }
                else
                {
                    fileStream.clear(std::ios::goodbit);
                    fileStream >> parsedWord;
                    ThrowIfTrue(parsedWord.compare("]"), "Expected closing ']' after indices");
                    break;
                }
            }

            fileStream >> parsedWord;
        }

        if (!parsedWord.compare("\"point"))
        {
            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("P\""), "Expecting \"point\" syntax to be followed by \"P\"");

            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("["), "'P' expected to be followed up with \"[\"");

            while (fileStream.good())
            {
                Vertex vertex;
                fileStream >> vertex.Position.x;
                fileStream >> vertex.Position.y;
                fileStream >> vertex.Position.z;

                if (fileStream.good())
                {
                    mesh.m_VertexBuffer.push_back(vertex);
                }
                else
                {
                    fileStream.clear(std::ios::goodbit);
                    fileStream >> parsedWord;
                    ThrowIfTrue(parsedWord.compare("]"), "Expected closing ']' after positions");
                    break;
                }
            }

            fileStream >> parsedWord;
        }

        if (!parsedWord.compare("\"normal"))
        {
            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("N\""), "Expecting \"normal\" syntax to be followed by an \"N\"");

            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("["), "'N' expected to be followed up with \"[\"");

            UINT vertexIndex = 0;
            while (fileStream.good())
            {
                float x, y, z;
                fileStream >> x;
                fileStream >> y;
                fileStream >> z;
                if (fileStream.good())
                {
                    ThrowIfTrue(vertexIndex >= mesh.m_VertexBuffer.size(), "More position values specified than normals");
                    Vertex &vertex = mesh.m_VertexBuffer[vertexIndex];
                    vertexIndex++;

                    vertex.Normal.x = x;
                    vertex.Normal.y = y;
                    vertex.Normal.z = z;
                }
                else
                {
                    fileStream.clear(std::ios::goodbit);
                    fileStream >> parsedWord;
                    ThrowIfTrue(parsedWord.compare("]"), "Expected closing ']' after positions");
                    break;
                }
            }

            fileStream >> parsedWord;
        }

        if (!parsedWord.compare("\"float"))
        {
            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("uv\""), "Expecting \"float\" syntax to be followed by \"uv\"");

            fileStream >> parsedWord;
            ThrowIfTrue(parsedWord.compare("["), "'UV' expected to be followed up with \"[\"");

            UINT vertexIndex = 0;
            while (fileStream.good())
            {
                float u, v;
                fileStream >> u;
                fileStream >> v;
                if (fileStream.good())
                {
                    ThrowIfTrue(vertexIndex >= mesh.m_VertexBuffer.size(), "More UV values specified than normals");
                    Vertex &vertex = mesh.m_VertexBuffer[vertexIndex];
                    vertexIndex++;

                    vertex.UV.u = u;
                    vertex.UV.v = v;
                }
                else
                {
                    fileStream.clear(std::ios::goodbit);
                    fileStream >> parsedWord;
                    ThrowIfTrue(parsedWord.compare("]"), "Expected closing ']' after positions");
                    break;
                }
            }

            fileStream >> parsedWord;
        }
    }

    string PBRTParser::CorrectNameString(char *pString)
    {
        string correctedString(pString);
        UINT startIndex = 0;
        UINT endIndex = correctedString.size();
        if (correctedString.size())
        {
            // sscanf often pulls extra quotations, cut these out
            if (correctedString[0] == '"')
            {
                startIndex = 1;
            }
            
            if (correctedString[correctedString.size() - 1] == '"')
            {
                endIndex = endIndex - 1;
            }
        }
        return correctedString.substr(startIndex, endIndex);
    }

    void PBRTParser::InitializeDefaults(Scene &outputScene)
    {
        InitializeCameraDefaults(outputScene.m_Camera);
    }

    void PBRTParser::InitializeCameraDefaults(Camera &camera)
    {
        camera.m_FieldOfView = 90;
        camera.m_LookAt = Vector3( 0.0f, 0.0f, 0.0f );
        camera.m_Position = Vector3(0.0f, 0.0f, -1.0f);
    }
}

