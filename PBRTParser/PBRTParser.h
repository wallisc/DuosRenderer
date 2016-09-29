#pragma once
#include "SceneParser.h"
#include <iostream>
#include <fstream>
#include <sstream>


#define PBRTPARSER_STRINGBUFFERSIZE 200

namespace PBRTParser
{
// Keep this inside our namespace because glm doesn't protect
// against double inclusion
#include "glm/vec4.hpp"
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
        void ParseTexture(std::ifstream &fileStream, SceneParser::Scene &outputScene);

        void ParseTransform();

        void ParseShape(std::ifstream &fileStream, SceneParser::Scene &outputScene, SceneParser::Mesh &mesh);

        void ParseBracketedVector3(std::istream, float &x, float &y, float &z);

        void InitializeDefaults(SceneParser::Scene &outputScene);
        void InitializeCameraDefaults(SceneParser::Camera &camera);

        static std::string CorrectNameString(const char *pString);
        static std::string CorrectNameString(const std::string &str);

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

            m_fileStream.getline(pTempBuffer, bufferSize);

            lastParsedWord = "";
            return pTempBuffer;
        }

        std::stringstream GetLineStream()
        {
            char *pTempBuffer;
            size_t bufferSize;
            GetTempCharBuffer(&pTempBuffer, bufferSize);

            m_fileStream.getline(pTempBuffer, bufferSize);

            return std::stringstream(std::string(pTempBuffer));
        }


        static SceneParser::Vector3 ConvertToVector3(const glm::vec4 &vec)
        {
            return SceneParser::Vector3(vec.x, vec.y, vec.z);
        }

        std::ifstream m_fileStream;

        glm::mat4 m_currentTransform;
        glm::vec4 m_lookAt;
        glm::vec4 m_camPos;
        glm::vec4 m_camUp;

        // Shouldn't be accessed directly outside of GetTempCharBuffer
        char _m_buffer[500];
        std::string lastParsedWord;
    };
}
