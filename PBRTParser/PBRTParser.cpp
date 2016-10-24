﻿#include "pch.h"
#include "PBRTParser.h"

using namespace SceneParser;
using namespace std;

namespace PBRTParser
{
    void ThrowIfTrue(bool expression, std::string errorMessage = "")
    {
        if (expression)
        {
            throw new BadFormatException(errorMessage.c_str());
        }
    }

    PBRTParser::PBRTParser()
    {
        m_AttributeStack.push(Attributes());
    }

    PBRTParser::~PBRTParser()
    {
        m_AttributeStack.pop();
        assert(m_AttributeStack.size() == 0);
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
                fileStream >> m_CurrentMaterial;
                fileStream >> lastParsedWord;
            }
            else if(!lastParsedWord.compare("Shape"))
            {
                ParseMesh(fileStream, outputScene);
            }
            else if (!lastParsedWord.compare("Texture"))
            {
                ParseTexture(fileStream, outputScene);
            }
            else if (!lastParsedWord.compare("AreaLightSource"))
            {
                ParseAreaLightSource(fileStream, outputScene);
            }
            else if (!lastParsedWord.compare("AttributeBegin"))
            {
                m_AttributeStack.push(Attributes());
                fileStream >> lastParsedWord;
            }
            else if (!lastParsedWord.compare("AttributeEnd"))
            {
                m_AttributeStack.pop();
                fileStream >> lastParsedWord;
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

    void PBRTParser::ParseBracketedVector3(std::istream is, float &x, float &y, float &z)
    {
        is >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("["), "Expect '[' at beginning of vector");

        is >> x;
        is >> y;
        is >> z;

        ThrowIfTrue(!is.good());

        is >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("]"), "Expect '[' at beginning of vector");
    }


    void PBRTParser::ParseMaterial(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        Material material;
        char materialName[PBRTPARSER_STRINGBUFFERSIZE];
        std::string materialType;

        auto lineStream = GetLineStream();
        
        lineStream >> lastParsedWord;
        material.m_MaterialName = CorrectNameString(lastParsedWord);

        while (lineStream.good())
        {
            if (!lastParsedWord.compare("\"string"))
            {
                lineStream >> lastParsedWord;
                if (!lastParsedWord.compare("type\""))
                {
                    lineStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("["));

                    lineStream >> materialType;
                    materialType = CorrectNameString(materialType);

                    lineStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("]"));
                }
                else
                {
                    ThrowIfTrue(false, "string not followed up with recognized token");
                }
            }
            else if (!lastParsedWord.compare("\"rgb"))
            {
                lineStream >> lastParsedWord;
                if (!lastParsedWord.compare("Kd\""))
                {
                    lineStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("["));

                    lineStream >> material.m_Diffuse.r;
                    if (lineStream.good())
                    {
                        lineStream >> material.m_Diffuse.g;
                        lineStream >> material.m_Diffuse.b;
                    }
                    else
                    {
                        lineStream.clear();
                        lineStream >> material.m_DiffuseTextureFilename;
                    }

                    lineStream >> lastParsedWord;
                    ThrowIfTrue(lastParsedWord.compare("]"));
                }
            }
            else
            {
                lineStream >> lastParsedWord;
            }
        }
        outputScene.m_Materials[material.m_MaterialName] = material;
    }

    void PBRTParser::ParseAreaLightSource(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        auto &lineStream = GetLineStream();

        lineStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("\"diffuse\""));

        lineStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("\"rgb"));

        lineStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("L\""));

        lineStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("["));

        AreaLightAttribute attribute;
        char materialName[PBRTPARSER_STRINGBUFFERSIZE];
        lineStream >> attribute.m_lightColor.r;
        lineStream >> attribute.m_lightColor.g;
        lineStream >> attribute.m_lightColor.b;

        lineStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("]"));

        SetCurrentAttributes(Attributes(attribute));
    }
    
    void PBRTParser::ParseExpectedWords(std::istream &inStream, _In_reads_(numWords) std::string *pWords, UINT numWords)
    {
        for (UINT i = 0; i < numWords; i++)
        {
            ParseExpectedWord(inStream, pWords[i]);
        }
    }

    void PBRTParser::ParseExpectedWord(std::istream &inStream, const std::string &word)
    {
        inStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare(word));
    }

    void PBRTParser::ParseTexture(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        // "float uscale"[20.000000] "float vscale"[20.000000] "rgb tex1"[0.325000 0.310000 0.250000] "rgb tex2"[0.725000 0.710000 0.680000]
        auto &lineStream = GetLineStream();

        std::string textureName;
        lineStream >> textureName;

        lineStream >> lastParsedWord;
        ThrowIfTrue(lastParsedWord.compare("\"spectrum\""));

        lineStream >> lastParsedWord;
        if (!lastParsedWord.compare("\"checkerboard\""))
        {
            float uscale;
            {
                std::string expectedWords[] = { "\"float", "uscale\"", "[" };
                ParseExpectedWords(lineStream, expectedWords, ARRAYSIZE(expectedWords));
                lineStream >> uscale;
                ParseExpectedWord(lineStream, "]");
            }

            float vscale;
            {
                std::string expectedWords[] = { "\"float", "vscale\"", "[" };
                ParseExpectedWords(lineStream, expectedWords, ARRAYSIZE(expectedWords));
                lineStream >> vscale;
                ParseExpectedWord(lineStream, "]");
            }

            Vector3 col1;
            {
                std::string expectedWords[] = { "\"rgb", "tex1\"", "[" };
                ParseExpectedWords(lineStream, expectedWords, ARRAYSIZE(expectedWords));
                lineStream >> col1.r;
                lineStream >> col1.g;
                lineStream >> col1.b;
                ParseExpectedWord(lineStream, "]");
            }

            Vector3 col2;
            {
                std::string expectedWords[] = { "\"rgb", "tex2\"", "[" };
                ParseExpectedWords(lineStream, expectedWords, ARRAYSIZE(expectedWords));
                lineStream >> col2.r;
                lineStream >> col2.g;
                lineStream >> col2.b;
                ParseExpectedWord(lineStream, "]");
            }
        }
        else
        {
            ThrowIfTrue(true);
        }
    }

    void PBRTParser::ParseMesh(std::ifstream &fileStream, SceneParser::Scene &outputScene)
    {
        Mesh *pMesh;
        if (GetCurrentAttributes().GetType() == Attributes::AreaLight)
        {
            outputScene.m_AreaLights.push_back(
                AreaLight(GetCurrentAttributes().GetAreaLightAttribute().m_lightColor));
            pMesh = &outputScene.m_AreaLights.back().m_Mesh;
        }
        else
        {
            outputScene.m_Meshes.push_back(Mesh());
            pMesh = &outputScene.m_Meshes[outputScene.m_Meshes.size() - 1];
        }
        string correctedMaterialName = CorrectNameString(m_CurrentMaterial);
        pMesh->m_pMaterial = &outputScene.m_Materials[correctedMaterialName];
        ThrowIfTrue(pMesh->m_pMaterial == nullptr, "Material name not found");

        ParseShape(fileStream, outputScene, *pMesh);
    }

    void PBRTParser::ParseShape(std::ifstream &fileStream, SceneParser::Scene &outputScene, SceneParser::Mesh &mesh)
    {
        fileStream >> lastParsedWord;
        
        if (!lastParsedWord.compare("PlyMesh"))
        {
            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("\"string"), "PlyMesh expected to be prepended with \"string\"");
            
            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("filename\""), "string expected to be prepended with filename\"");
            
            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("["), "Expected \'[\'");

            fileStream >> lastParsedWord;
            std::string correctedFileName = CorrectNameString(lastParsedWord.c_str());

            PlyParser::PlyParser().Parse(correctedFileName, mesh);

            fileStream >> lastParsedWord;
            ThrowIfTrue(lastParsedWord.compare("]"), "Expected \']\'");
        }
        else if (!lastParsedWord.compare("\"trianglemesh\""))
        {
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
    }

    string PBRTParser::CorrectNameString(const string &str)
    {
        return CorrectNameString(str.c_str());
    }

    string PBRTParser::CorrectNameString(const char *pString)
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
        return correctedString.substr(startIndex, endIndex - startIndex);
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

