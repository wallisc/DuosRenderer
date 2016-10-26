#include "pch.h"

using namespace SceneParser;
using namespace std;

namespace PlyParser
{
void ThrowIfTrue(bool expression, std::string errorMessage)
{
    if (expression)
    {
        throw new BadFormatException(errorMessage.c_str());
    }
}

void PlyParser::ParseHeader()
{
    m_fileStream >> lastParsedWord;
    ThrowIfTrue(lastParsedWord.compare("ply"), "First word in ply file expect to be \'Ply\'");
    
    while (m_fileStream.good())
    {
        if (!lastParsedWord.compare("end_header"))
        {
            break;
        }
        else if (!lastParsedWord.compare("property"))
        {
            m_fileStream >> lastParsedWord;
            if (!lastParsedWord.compare("float"))
            {
                m_fileStream >> lastParsedWord;
                if (!lastParsedWord.compare("x"))
                {
                    m_elementLayout.push_back(Element(POSITION, X));
                }
                else if (!lastParsedWord.compare("y"))
                {
                    m_elementLayout.push_back(Element(POSITION, Y));
                }
                else if (!lastParsedWord.compare("z"))
                {
                    m_elementLayout.push_back(Element(POSITION, Z));
                }
                else if (!lastParsedWord.compare("nx"))
                {
                    m_elementLayout.push_back(Element(NORMAL, X));
                }
                else if (!lastParsedWord.compare("ny"))
                {
                    m_elementLayout.push_back(Element(NORMAL, Y));
                }
                else if (!lastParsedWord.compare("nz"))
                {
                    m_elementLayout.push_back(Element(NORMAL, Z));
                }
                else if (!lastParsedWord.compare("u"))
                {
                    m_elementLayout.push_back(Element(TEXTURE, U));
                }
                else if (!lastParsedWord.compare("v"))
                {
                    m_elementLayout.push_back(Element(TEXTURE, V));
                }
            }
        }
        else if (!lastParsedWord.compare("element"))
        {
            m_fileStream >> lastParsedWord;
            if(!lastParsedWord.compare("face"))
            {
                m_fileStream >> m_numFaces;
            }
            else if (!lastParsedWord.compare("vertex"))
            {
                m_fileStream >> m_numVertices;
            }
        }
        else
        {
            m_fileStream >> lastParsedWord;
        }
    }
}

void PlyParser::ParseBody(SceneParser::Mesh &mesh)
{
    for (UINT vertex = 0; vertex < m_numVertices; vertex++)
    {

    }

    for (UINT face = 0; face < m_numFaces; face++)
}

void PlyParser::Parse(const std::string &filename, SceneParser::Mesh &mesh)
{
    m_fileStream = ifstream(filename);
    ThrowIfTrue(!m_fileStream.good(), "Failure opening file");

    ParseHeader();
    ParseBody();
}

}