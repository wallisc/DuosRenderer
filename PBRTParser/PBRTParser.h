#pragma once
#include "SceneParser.h"
#include <iostream>
#include <fstream>

#define PBRTPARSER_STRINGBUFFERSIZE 200

namespace PBRTParser
{
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

        void ParseShape(std::ifstream &fileStream, SceneParser::Scene &outputScene, SceneParser::Mesh &mesh);

        void InitializeDefaults(SceneParser::Scene &outputScene);
        void InitializeCameraDefaults(SceneParser::Camera &camera);

        static std::string CorrectNameString(char *pString);

        void GetTempCharBuffer(char **ppBuffer, size_t &charBufferSize)
        {
            *ppBuffer = _m_buffer;
            charBufferSize = ARRAYSIZE(_m_buffer);
        };

        // Shouldn't be accessed directly outside of GetTempCharBuffer
        char _m_buffer[500]; 
    };
}
