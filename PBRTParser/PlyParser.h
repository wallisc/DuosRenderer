#pragma once
namespace PlyParser
{
    class PlyParser
    {
    public:
        void Parse(const std::string &filename, SceneParser::Mesh &mesh);
        void ParseHeader();
    private:
        std::ifstream m_fileStream;
        std::string lastParsedWord;

        enum ElementType
        {
            POSITION,
            NORMAL,
            TEXTURE
        };

        enum ElementIndex
        {
            U = 0,
            V = 1,
            X = 0,
            Y = 1,
            Z = 2,
        };

        typedef std::pair<ElementType, ElementIndex> Element;
        std::vector<Element> m_elementLayout;
        UINT m_numFaces;
        UINT m_numVertices;
    };
}