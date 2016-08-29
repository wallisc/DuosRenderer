#pragma once
#include "SceneParser.h"
#include <iostream>
#include <fstream>


#define PBRTPARSER_STRINGBUFFERSIZE 200

namespace PBRTParser
{
// Keep this inside our namespace because glm doesn't protect
// against double inclusion
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/glm.hpp"

    class PBRTParser : public SceneParser::SceneParserClass
    {
    public:
        virtual void Parse(std::string filename, SceneParser::Scene &outputScene);

    private:
        void ParseFilm(std::ifstream &fileStream, SceneParser::Scene &outputScene);
        void ParseCamera(std::ifstream &fileStream, SceneParser::Scene &outputScene);
        void ParseWorld(std::ifstream &fileStream, SceneParser::Scene &outputScene);
        void ParseMaterial(std::ifstream &fileStream, SceneParser::Scene &outputScene);
        void ParseMesh(std::ifstream &fileStream, SceneParser::Scene &outputScene);
        
        void ParseTransform();

        void ParseShape(std::ifstream &fileStream, SceneParser::Scene &outputScene, SceneParser::Mesh &mesh);

        void InitializeDefaults(SceneParser::Scene &outputScene);
        void InitializeCameraDefaults(SceneParser::Camera &camera);

        static std::string CorrectNameString(char *pString);

        void GetTempCharBuffer(char **ppBuffer, size_t &charBufferSize)
        {
            *ppBuffer = _m_buffer;
            charBufferSize = ARRAYSIZE(_m_buffer);
        };

        char *GetLine()
        {
            char *pTempBuffer;
            size_t bufferSize;
            GetTempCharBuffer(&pTempBuffer, bufferSize);

            char fileName[PBRTPARSER_STRINGBUFFERSIZE];
            m_fileStream.getline(pTempBuffer, bufferSize);

            lastParsedWord = "";
            return pTempBuffer;
        }

        std::ifstream m_fileStream;

        glm::mat4 m_currentTransform;

        // Shouldn't be accessed directly outside of GetTempCharBuffer
        char _m_buffer[500];
        std::string lastParsedWord;
    };
}
