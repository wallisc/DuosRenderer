#include "pch.h"
#include "PBRTParser.h"

using namespace SceneParser;
using namespace std;

namespace PBRTParser
{
    void ThrowIfNotTrue(bool expression, std::string errorMessage)
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

        ThrowIfNotTrue(argCount != 1, "Camera arguments not formatted correctly");
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

        ThrowIfNotTrue(argCount != 3, "Film arguments not formatted correctly");
        
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
            &material.m_DiffuseRed,
            &material.m_DiffuseGreen,
            &material.m_DiffuseBlue);
        material.m_MaterialName = materialName;

        ThrowIfNotTrue(argCount != 5, "Material arguments not formatted correctly");

        outputScene.m_Materials[material.m_MaterialName] = material;
    }
}

