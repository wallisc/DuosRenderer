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
        m_fileStream = ifstream(filename);
        m_currentTransform = glm::mat4();

        if (!m_fileStream.good())
        {
            assert(false); // file not found
        }

        InitializeDefaults(outputScene);

        while (m_fileStream.good())
        {
            if (!lastParsedWord.compare("Film"))
            {
                ParseFilm(m_fileStream, outputScene);
            }
            else if (!lastParsedWord.compare("Camera"))
            {
                ParseCamera(m_fileStream, outputScene);
            }
            else if (!lastParsedWord.compare("Transform"))
            {
                ParseTransform();
            }
            else if (!lastParsedWord.compare("WorldBegin"))
            {
                m_currentTransform = glm::mat4();
                ParseWorld(m_fileStream, outputScene);
            }
            else
            {
                m_fileStream >> lastParsedWord;
            }
        }
    }

    void PBRTParser::ParseWorld(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        while (fileStream.good())
        {
            if (!lastParsedWord.compare("MakeNamedMaterial"))
            {
                ParseMaterial(fileStream, outputScene);
            }
            else if (!lastParsedWord.compare("NamedMaterial"))
            {
                ParseMesh(fileStream, outputScene);
            }
            else if (!lastParsedWord.compare("WorldEnd"))
            {
                break;
            }
            else
            {
                fileStream >> lastParsedWord;
            }
        }
    }

    void PBRTParser::ParseCamera(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        char *pTempBuffer = GetLine();

        UINT argCount = sscanf_s(pTempBuffer, " \"perspective\" \"float fov\" \[ %f \]",
            &outputScene.m_Camera.m_FieldOfView);

        ThrowIfTrue(argCount != 1, "Camera arguments not formatted correctly");
    
        outputScene.m_Camera.m_LookAt = ConvertToVector3(m_currentTransform * m_lookAt);
        outputScene.m_Camera.m_Position = ConvertToVector3(m_currentTransform * m_camPos);
        outputScene.m_Camera.m_Up = ConvertToVector3(m_currentTransform * m_camUp);
    }

    void PBRTParser::ParseFilm(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        char *pTempBuffer = GetLine();

        char fileName[PBRTPARSER_STRINGBUFFERSIZE];
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
        char *pTempBuffer = GetLine();

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
        char *pTempBuffer = GetLine();

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
        fileStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("Shape"), "Geometry expected to be prepended with \"Shape\"");

        fileStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("\"trianglemesh\""), "Only TriangleMesh supported topology at the moment");

        fileStream >> lastParsedWord;
        if (!lastParsedWord.compare("\"integer"))
        {
            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("indices\""), "\"integer\" expected to be followed up with \"integer\"");

            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("["), "\"indices\" expected to be followed up with \"[\"");

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
                    fileStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("]"), "Expected closing ']' after indices");
                    break;
                }
            }

            fileStream >> lastParsedWord;
        }

        if (!lastParsedWord.compare("\"point"))
        {
            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("P\""), "Expecting \"point\" syntax to be followed by \"P\"");

            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("["), "'P' expected to be followed up with \"[\"");

            while (fileStream.good())
            {
                Vertex vertex = {};
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
                    fileStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("]"), "Expected closing ']' after positions");
                    break;
                }
            }

            fileStream >> lastParsedWord;
        }

        if (!lastParsedWord.compare("\"normal"))
        {
            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("N\""), "Expecting \"normal\" syntax to be followed by an \"N\"");

            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("["), "'N' expected to be followed up with \"[\"");

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
                    fileStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("]"), "Expected closing ']' after positions");
                    break;
                }
            }

            fileStream >> lastParsedWord;
        }

        if (!lastParsedWord.compare("\"float"))
        {
            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("uv\""), "Expecting \"float\" syntax to be followed by \"uv\"");

            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("["), "'UV' expected to be followed up with \"[\"");

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
                    fileStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("]"), "Expected closing ']' after positions");
                    break;
                }
            }

            fileStream >> lastParsedWord;
        }

        mesh.m_AreTangentsValid = false;
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
        m_lookAt = glm::vec4(0.0f, 2.0f, 1.0f, 1.0f);
        m_camPos = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f);
        m_camUp = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

        camera.m_FieldOfView = 90;
        camera.m_LookAt = ConvertToVector3(m_lookAt);
        camera.m_Position = ConvertToVector3(m_camPos);
        camera.m_Up = Vector3(0.0f, 1.0f, 0.0f);
        camera.m_NearPlane = 0.001f;
        camera.m_FarPlane = 999999.0f;
    }

    void PBRTParser::ParseTransform()
    {
        char *pTempBuffer = GetLine();
        float mat[4][4];

        UINT argCount = sscanf_s(pTempBuffer, " \[ %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f \] ",
            &mat[0][0],
            &mat[0][1],
            &mat[0][2],
            &mat[0][3],

            &mat[1][0],
            &mat[1][1],
            &mat[1][2],
            &mat[1][3],

            &mat[2][0],
            &mat[2][1],
            &mat[2][2],
            &mat[2][3],

            &mat[3][0],
            &mat[3][1],
            &mat[3][2],
            &mat[3][3]);

        ThrowIfTrue(argCount != 16, "Transform arguments not formatted correctly");

        m_currentTransform = glm::mat4(
            mat[0][0],
            mat[0][1],
            mat[0][2],
            mat[0][3],

            mat[1][0],
            mat[1][1],
            mat[1][2],
            mat[1][3],

            mat[2][0],
            mat[2][1],
            mat[2][2],
            mat[2][3],

            mat[3][0],
            mat[3][1],
            mat[3][2],
            mat[3][3]);
    }
}

