#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <atlconv.h>
#include <atlbase.h>
#include "resource.h"
#include "Renderer.h"
#include "D3D11Renderer.h"
#include "RTRenderer.h"
#include "RendererException.h"
#include "D3D11Canvas.h"
#include "DXUT/Core/DXUT.h"
#include "DXUT/Optional/DXUTgui.h"
#include "DXUT/Optional/SDKMisc.h"

#include "dxtk/inc/WICTextureLoader.h"

#include "assimp/Importer.hpp"      // C++ importer interface
#include "assimp/scene.h"      
#include "assimp/postprocess.h"     // Post processing flags

#include <list>
#include "Strsafe.h"
#include "PBRTParser.h"

using namespace DirectX;
using namespace Assimp;

UINT g_Width = 800;
UINT g_Height = 600;

enum
{
    CMD_CHANGE_RENDERER = 0,
    CMD_CAMERA_MODE,
    CMD_SHOW_GOLDEN_IMAGE,
    ROUGHNESS_TEXT,
    ROUGHNESS_SLIDER,
    REFLECTIVITY_TEXT,
    REFLECTIVITY_SLIDER,
    GAMMA_CORRECTION_CHECK_BOX,
    NUM_GUI_ITEMS
} GUI_ID;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;

enum RENDERER_TYPE
{
    D3D11,
    RAYTRACER,
    NUM_RENDERER_TYPES
};

const float CAMERA_ROTATION_SPEED = 3.14 / (800);
const float CAMERA_SIDE_SCREEN_ROTATION_SPEED = 3.14 / 4.0;

Renderer *g_pRenderer[NUM_RENDERER_TYPES];
Scene *g_pScene[NUM_RENDERER_TYPES];
Camera *g_pCamera[NUM_RENDERER_TYPES];
//std::vector<Material *> g_MaterialList[NUM_RENDERER_TYPES];
std::unordered_map<std::string, Material *> g_MaterialList[NUM_RENDERER_TYPES];

EnvironmentMap *g_pEnvironmentMap[NUM_RENDERER_TYPES];
RenderSettings g_RenderSettings = DefaultRenderSettings;
RENDERER_TYPE g_ActiveRenderer = D3D11;
bool g_ShowGoldenImage = false;

UINT g_NumRenderers = NUM_RENDERER_TYPES;
D3D11Canvas *g_pCanvas;
Assimp::Importer g_importer;

SceneParser::Scene g_outputScene;
std::string g_referenceImageFilePath;

bool g_MouseInitialized = false;
int g_MouseX;
int g_MouseY;

const unsigned int NO_MATERIAL_SELECTED = (unsigned int)-1;
std::string g_SelectedMaterialName = "";

bool g_CameraModeEnabled = false;

CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CDXUTDialog  g_GUI;
CDXUTTextHelper* g_pTextWriter = nullptr;

const INT MAX_SLIDER_VALUE = 100;

ID3D11Device *g_pDevice;
ID3D11DeviceContext *g_pImmediateContext;

IDXGISwapChain1* g_pSwapChain1;
IDXGISwapChain* g_pSwapChain;

ID3D11Texture2D* g_pGoldenImage = nullptr;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
//void InitSceneAndCamera(_In_ Renderer *, _In_ const aiScene &assimpScene, _In_ EnvironmentMap *pEnvMap, _Out_ std::vector<Material *> &MaterialList, _Out_ Scene **, _Out_ Camera **);
void InitSceneAndCamera(_In_ Renderer *, _In_ EnvironmentMap *pEnvMap, _In_ const SceneParser::Scene &fileScene, std::unordered_map<std::string, Material *> &materialList, _Out_ Scene **, _Out_ Camera **);
void InitEnvironmentMap(_In_ Renderer *pRenderer, char *CubeMapName, char *irradMapName, _Out_ EnvironmentMap **ppEnviromentMap);
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void UpdateCamera();
void Render();

HRESULT CALLBACK OnDeviceCreated(_In_ ID3D11Device* pd3dDevice, _In_ const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, _In_opt_ void* pUserContext);
void    CALLBACK OnFrameRender(_In_ ID3D11Device* pd3dDevice, _In_ ID3D11DeviceContext* pd3dImmediateContext, _In_ double fTime, _In_ float fElapsedTime, _In_opt_ void* pUserContext);
HRESULT CALLBACK OnResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext);
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);
void CALLBACK OnD3D11DestroyDevice(void* pUserContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);
void CALLBACK OnKeyPress(_In_ UINT nChar, _In_ bool bKeyDown, _In_ bool bAltDown, _In_opt_ void* pUserContext);
void CALLBACK OnMouseMove(_In_ bool bLeftButtonDown, _In_ bool bRightButtonDown, _In_ bool bMiddleButtonDown,
    _In_ bool bSideButton1Down, _In_ bool bSideButton2Down, _In_ int nMouseWheelDelta,
    _In_ int xPos, _In_ int yPos, _In_opt_ void* pUserContext);


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    auto commandLineToStringVector = [](LPWSTR commandLine) -> std::vector<std::string>{
        std::vector<std::string> parsedArgs;
        std::string word;
        for (UINT charIndex = 0;; charIndex++)
        {
            auto currentChar = commandLine[charIndex];
            if (isspace(currentChar))
            {
                if (word.size() > 0)
                {
                    parsedArgs.push_back(word);
                    word.clear();
                }
            }
            else if (currentChar == 0)
            {
                if (word.size() > 0) parsedArgs.push_back(word);
                break;
            }
            else 
            {
                word.push_back(currentChar);
            }
        }
        return parsedArgs;
    };

    auto parsedArgs = commandLineToStringVector(lpCmdLine);
    std::string sceneFilePath;
    for (UINT argIndex = 0; argIndex < parsedArgs.size(); argIndex++)
    {
        auto &arg = parsedArgs[argIndex];
        if (arg.compare("-i") == 0 && argIndex < parsedArgs.size() - 1)
        {
            g_referenceImageFilePath = parsedArgs[++argIndex];
        }

        if (arg.compare("-s") == 0 && argIndex < parsedArgs.size() - 1)
        {
            sceneFilePath = parsedArgs[++argIndex];
        }
    }

    if(sceneFilePath.substr(sceneFilePath.size() - 4, 4).compare("pbrt") == 0)
    {
        PBRTParser::PBRTParser().Parse(sceneFilePath, g_outputScene);
    }
    else
    {
        // Unknown scene file format
        return 0;
    }
    
    if (g_outputScene.m_Film.m_ResolutionX > 0 && g_outputScene.m_Film.m_ResolutionY > 0)
    {
        g_Width = g_outputScene.m_Film.m_ResolutionX;
        g_Height = g_outputScene.m_Film.m_ResolutionY;
    }

    // DXUT will create and use the best device
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks

    DXUTSetCallbackMsgProc(MsgProc);
    //UTSetCallbackDeviceChanging(ModifyDeviceSettings);
    //UTSetCallbackDeviceRemoved(OnDeviceRemoved);

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11

    DXUTSetCallbackD3D11DeviceCreated(OnDeviceCreated);
    DXUTSetCallbackD3D11SwapChainResized(OnResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnFrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
    DXUTSetCallbackKeyboard(OnKeyPress);
    DXUTSetCallbackMouse(OnMouseMove, true, nullptr);

    g_GUI.Init(&g_DialogResourceManager);

    DXUTInit(true, true, nullptr); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen

    DXUTCreateWindow(L"Duos Renderer");

    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_1, true, 1024, 1024);
    DXUTMainLoop();

    return DXUTGetExitCode();
}

const aiScene* InitAssimpScene(_In_ char *pSceneFile);

HRESULT CALLBACK OnDeviceCreated(_In_ ID3D11Device* pd3dDevice, _In_ const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, _In_opt_ void* pUserContext)
{
    g_pDevice = pd3dDevice;
    g_pImmediateContext = DXUTGetD3D11DeviceContext();

    if (g_referenceImageFilePath.size())
    {
        CComPtr<ID3D11Resource> pGoldenResource;
        CA2WEX<MAX_ALLOWED_STR_LENGTH> WideTextureName(g_referenceImageFilePath.c_str());
        HRESULT result = CreateWICTextureFromFileEx(
            g_pDevice,
            WideTextureName,
            20 * 1024 * 1024,
            D3D11_USAGE_DEFAULT,
            0,
            0,
            0,
            false,
            &pGoldenResource,
            nullptr);
        if (FAILED(result))
        {
            return result;
        }

        pGoldenResource->QueryInterface(&g_pGoldenImage);
        g_NumRenderers++;

        D3D11_TEXTURE2D_DESC texDesc;
        g_pGoldenImage->GetDesc(&texDesc);

        g_Width = texDesc.Width;
        g_Height = texDesc.Height;

    }

    g_pCanvas = new D3D11Canvas(g_pDevice, g_pImmediateContext, g_Width, g_Height);
    g_pSwapChain = DXUTGetDXGISwapChain();

    HRESULT hr = g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, g_pImmediateContext);
    g_pTextWriter = new CDXUTTextHelper(pd3dDevice, g_pImmediateContext, &g_DialogResourceManager, 15);

    g_GUI.SetCallback(OnGUIEvent); int iY = 10;
    const INT BUTTON_HEIGHT = 26;
    hr = g_GUI.AddButton(CMD_CHANGE_RENDERER, L"Change renderer (space)", 0, iY, 170, BUTTON_HEIGHT, VK_SPACE);
    assert(SUCCEEDED(hr));

    iY += BUTTON_HEIGHT;
    hr = g_GUI.AddButton(CMD_SHOW_GOLDEN_IMAGE, L"Toggle Golden Image (i)", 0, iY, 170, BUTTON_HEIGHT, 'I');
    assert(SUCCEEDED(hr));

    iY += BUTTON_HEIGHT;
    hr = g_GUI.AddButton(CMD_CAMERA_MODE, L"Toggle camera moves (T)", 0, iY, 170, BUTTON_HEIGHT, 'T');
    assert(SUCCEEDED(hr));

    iY += BUTTON_HEIGHT;
    g_GUI.AddStatic(ROUGHNESS_TEXT, L"Roughness", 0, iY, 170, BUTTON_HEIGHT);

    iY += BUTTON_HEIGHT;
    hr = g_GUI.AddSlider(ROUGHNESS_SLIDER, 50, iY, 100, BUTTON_HEIGHT, 0, MAX_SLIDER_VALUE, 0);
    assert(SUCCEEDED(hr));

    iY += BUTTON_HEIGHT;
    g_GUI.AddStatic(REFLECTIVITY_TEXT, L"Reflectivity", 0, iY, 170, BUTTON_HEIGHT);

    iY += BUTTON_HEIGHT;
    hr = g_GUI.AddSlider(REFLECTIVITY_SLIDER, 50, iY, 100, BUTTON_HEIGHT, 0, MAX_SLIDER_VALUE, 0);
    assert(SUCCEEDED(hr));

    iY += BUTTON_HEIGHT;
    hr = g_GUI.AddCheckBox(GAMMA_CORRECTION_CHECK_BOX, L"Gamma Correct", 0, iY, 100, BUTTON_HEIGHT, g_RenderSettings.m_GammaCorrection);
    assert(SUCCEEDED(hr));

    FAIL_CHK(FAILED(hr), "Failed to create DXUT dialog manager");

    const aiScene* pAssimpScene = InitAssimpScene("Assets/sampleScene/sampleScene.dae");

    FAIL_CHK(!pAssimpScene, "Failed to open scene file");

    for (UINT i = 0; i < NUM_RENDERER_TYPES; i++)
    {
        switch (i)
        {
        case RENDERER_TYPE::D3D11:
            g_pRenderer[i] = new D3D11Renderer(g_hWnd, g_Width, g_Height);
            break;
        case RENDERER_TYPE::RAYTRACER:
            g_pRenderer[i] = new RTRenderer(g_Width, g_Height);
            break;
        default:
            break;
        }

        g_pRenderer[i]->SetCanvas(g_pCanvas);
        InitEnvironmentMap(g_pRenderer[i], "Assets\\EnvironmentMap\\Uffizi\\Uffizi", "Assets\\EnvironmentMap\\Uffizi\\irrad", &g_pEnvironmentMap[i]);
        
        InitSceneAndCamera(g_pRenderer[i], g_pEnvironmentMap[i], g_outputScene, g_MaterialList[i], &g_pScene[i], &g_pCamera[i]);
        //InitSceneAndCamera(g_pRenderer[i], *pAssimpScene, g_pEnvironmentMap[i], g_MaterialList[i], &g_pScene[i], &g_pCamera[i]);
    }

    return	S_OK;
}

const aiScene* InitAssimpScene(_In_ char *pSceneFile)
{
    // Create an instance of the Importer class
    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll 
    // propably to request more postprocessing than we do in this example.
    const aiScene* AssimpScene = g_importer.ReadFile(pSceneFile,
        aiProcess_Triangulate |
        aiProcess_MakeLeftHanded |
        aiProcess_FlipUVs |
        aiProcess_FlipWindingOrder |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_PreTransformVertices);

    return AssimpScene;
}

Vec3 ConvertVec3(const SceneParser::Vector3 &vec)
{
    return Vec3(vec.x, vec.y, vec.z);
}

Vec2 ConvertVec2(const SceneParser::Vector2 &vec)
{
    return Vec2(vec.x, vec.y);
}


Vec3 ConvertVec3(const aiVector3D &vec)
{
    return Vec3(vec.x, vec.y, vec.z);
}

Vec2 ConvertVec2(const aiVector3D &vec)
{
    return Vec2(vec.x, vec.y);
}

void InitEnvironmentMap(_In_ Renderer *pRenderer, char *CubeMapName, char* irradMapName, _Out_ EnvironmentMap **ppEnviromentMap)
{
    CreateEnvironmentMapDescriptor EnvMapDescriptor;
    char textureCubFileNames[TEXTURES_PER_CUBE][MAX_ALLOWED_STR_LENGTH];
    char irradTextureCubFileNames[TEXTURES_PER_CUBE][MAX_ALLOWED_STR_LENGTH];
    EnvMapDescriptor.m_EnvironmentType = CreateEnvironmentMapDescriptor::EnvironmentType::TEXTURE_CUBE;
    CreateEnvironmentTextureCube &TexCubeDescriptor = EnvMapDescriptor.m_TextureCube;
    for (UINT i = 0; i < TEXTURES_PER_CUBE; i++)
    {
        sprintf_s(textureCubFileNames[i], "%s_c0%d.bmp", CubeMapName, i);
        TexCubeDescriptor.m_TextureNames[i] = textureCubFileNames[i];
    }

    *ppEnviromentMap = pRenderer->CreateEnvironmentMap(&EnvMapDescriptor);
}

void InitSceneAndCamera(_In_ Renderer *pRenderer, _In_ EnvironmentMap *pEnvMap, _In_ const SceneParser::Scene &fileScene, _Out_ std::unordered_map<std::string, Material*> &MaterialList, _Out_ Scene **ppScene, _Out_ Camera **ppCamera)
{
    *ppScene = pRenderer->CreateScene(pEnvMap);
    Scene *pScene = *ppScene;

    const UINT materialCount = g_outputScene.m_Materials.size();
    MaterialList.reserve(materialCount);
    for (auto &materialKeyValuePair : g_outputScene.m_Materials)
    {
        SceneParser::Material &material = materialKeyValuePair.second;

        CreateMaterialDescriptor CreateMaterialDescriptor = {};
        CreateMaterialDescriptor.m_TextureName = material.m_DiffuseTextureFilename.c_str();

        CreateMaterialDescriptor.m_DiffuseColor.x = material.m_Diffuse.r;
        CreateMaterialDescriptor.m_DiffuseColor.y = material.m_Diffuse.g;
        CreateMaterialDescriptor.m_DiffuseColor.z = material.m_Diffuse.b;

        // TODO: Need to add these
        const float shininess = 0.5f;
        CreateMaterialDescriptor.m_Reflectivity = 0.5f;
        CreateMaterialDescriptor.m_Roughness = sqrt(2.0 / (shininess + 2.0f));

        MaterialList[materialKeyValuePair.first] = pRenderer->CreateMaterial(&CreateMaterialDescriptor);
    }

    {
        std::vector<Vertex> vertexList;
        std::vector<unsigned int> indexList;
        for (const SceneParser::Mesh &mesh : fileScene.m_Meshes)
        {
            UINT numVerts = mesh.m_VertexBuffer.size();
            UINT numIndices = mesh.m_IndexBuffer.size();

            vertexList.resize(numVerts);
            indexList.resize(numIndices);
            for (UINT vertIdx = 0; vertIdx < numVerts; vertIdx++)
            {
                const SceneParser::Vertex &inputVertex = mesh.m_VertexBuffer[vertIdx];
                Vertex &vertex = vertexList[vertIdx];
                vertex.m_Position = ConvertVec3(inputVertex.Position);
                vertex.m_Normal = ConvertVec3(inputVertex.Normal);
                vertex.m_Tex = ConvertVec2(inputVertex.UV);
                vertex.m_Tangent = ConvertVec3(inputVertex.Tangents);
            }
            
            for (UINT ibIdx = 0; ibIdx < mesh.m_IndexBuffer.size(); ibIdx++)
            {
                assert(mesh.m_IndexBuffer[ibIdx] >= 0);
                indexList[ibIdx] = (UINT)mesh.m_IndexBuffer[ibIdx];
            }

            CreateGeometryDescriptor geometryDescriptor;
            geometryDescriptor.m_pVertices = vertexList.data();
            geometryDescriptor.m_NumVertices = numVerts;
            geometryDescriptor.m_pIndices = indexList.data();
            geometryDescriptor.m_NumIndices = numIndices;
            geometryDescriptor.m_pMaterial = MaterialList[mesh.m_pMaterial->m_MaterialName];
            Geometry *pGeometry = pRenderer->CreateGeometry(&geometryDescriptor);
            pScene->AddGeometry(pGeometry);
        }
    }

    {
        CreateLightDescriptor CreateLight;
        CreateDirectionalLight CreateDirectional;
        CreateDirectional.m_EmissionDirection = Vec3(1.0f, -1.0, -1.0);
        CreateLight.m_Color = Vec3(1.0f, 1.0f, 1.0f);
        CreateLight.m_LightType = CreateLightDescriptor::DIRECTIONAL_LIGHT;
        CreateLight.m_pCreateDirectionalLight = &CreateDirectional;

        Light *pLight = pRenderer->CreateLight(&CreateLight);
        pScene->AddLight(pLight);
    }

    const float LensHeight = 2.0f;
    const float AspectRatio = (float)fileScene.m_Film.m_ResolutionX / fileScene.m_Film.m_ResolutionY;
    const float LensWidth = LensHeight * AspectRatio;
    const float FocalLength = LensWidth / (2.0f* tan(fileScene.m_Camera.m_FieldOfView / 2.0f));
    float VerticalFov = 2 * atan(LensHeight / (2.0f * FocalLength));

    CreateCameraDescriptor CameraDescriptor = {};
    CameraDescriptor.m_Height = fileScene.m_Film.m_ResolutionX;
    CameraDescriptor.m_Width = fileScene.m_Film.m_ResolutionY;
    CameraDescriptor.m_FocalPoint = ConvertVec3(fileScene.m_Camera.m_Position);
    CameraDescriptor.m_LookAt = ConvertVec3(fileScene.m_Camera.m_LookAt);
    CameraDescriptor.m_Up = ConvertVec3(fileScene.m_Camera.m_Up);
    CameraDescriptor.m_NearClip = fileScene.m_Camera.m_NearPlane;
    CameraDescriptor.m_FarClip = fileScene.m_Camera.m_FarPlane;
    CameraDescriptor.m_VerticalFieldOfView = VerticalFov;

    *ppCamera = pRenderer->CreateCamera(&CameraDescriptor);
}

#if 0
void InitSceneAndCamera(_In_ Renderer *pRenderer, _In_ const aiScene &assimpScene, _In_ EnvironmentMap *pEnvMap, _Out_ std::vector<Material *> &MaterialList, _Out_ Scene **ppScene, _Out_ Camera **ppCamera)
{
    *ppScene = pRenderer->CreateScene(pEnvMap);
    Scene *pScene = *ppScene;

    MaterialList = std::vector<Material *>(assimpScene.mNumMaterials);
    for (UINT i = 0; i < assimpScene.mNumMaterials; i++)
    {
        aiMaterial *pMat = assimpScene.mMaterials[i];
        assert(pMat);

        aiString path;
        float reflectivity, shininess;
        pMat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
        pMat->Get(AI_MATKEY_REFLECTIVITY, reflectivity);
        pMat->Get(AI_MATKEY_SHININESS, shininess);

        aiColor3D diffuse;
        pMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);

        CreateMaterialDescriptor CreateMaterialDescriptor = {};
        CreateMaterialDescriptor.m_TextureName = path.C_Str();

        // TODO: Collada doesn't support bump maps so I'm using this hack to test bump mapping on the sample scene
        if (strcmp(CreateMaterialDescriptor.m_TextureName, "Assets/sampleScene/crate.png") == 0)
        {
            CreateMaterialDescriptor.m_NormalMapName = "Assets/sampleScene/crateBump.jpg";
        }

        CreateMaterialDescriptor.m_DiffuseColor.x = diffuse.r;
        CreateMaterialDescriptor.m_DiffuseColor.y = diffuse.g;
        CreateMaterialDescriptor.m_DiffuseColor.z = diffuse.b;

        CreateMaterialDescriptor.m_Reflectivity = reflectivity;
        CreateMaterialDescriptor.m_Roughness = sqrt(2.0 / (shininess + 2.0f));

        MaterialList[i] = pRenderer->CreateMaterial(&CreateMaterialDescriptor);
    }

    {
        std::vector<Vertex> vertexList;
        std::vector<unsigned int> indexList;
        for (UINT i = 0; i < assimpScene.mNumMeshes; i++)
        {
            const aiMesh *pMesh = assimpScene.mMeshes[i];
            assert(*pMesh->mNumUVComponents == 2 || *pMesh->mNumUVComponents == 0);
            assert(pMesh->HasTangentsAndBitangents());

            UINT numVerts = pMesh->mNumVertices;
            UINT numFaces = pMesh->mNumFaces;
            UINT numIndices = numFaces * 3;

            vertexList.resize(numVerts);
            indexList.resize(numIndices);
            for (UINT vertIdx = 0; vertIdx < numVerts; vertIdx++)
            {
                Vertex &vertex = vertexList[vertIdx];
                vertex.m_Position = ConvertVec3(pMesh->mVertices[vertIdx]);
                vertex.m_Normal = ConvertVec3(pMesh->mNormals[vertIdx]);
                vertex.m_Tex = ConvertVec2(pMesh->mTextureCoords[0][vertIdx]);
                vertex.m_Tangent = ConvertVec3(pMesh->mTangents[vertIdx]);
            }

            for (UINT i = 0; i < numFaces; i++)
            {
                auto pFace = &pMesh->mFaces[i];
                assert(pFace->mNumIndices == 3);
                indexList[i * 3] = pFace->mIndices[0];
                indexList[i * 3 + 1] = pFace->mIndices[1];
                indexList[i * 3 + 2] = pFace->mIndices[2];
            }

            CreateGeometryDescriptor geometryDescriptor;
            geometryDescriptor.m_pVertices = &vertexList[0];
            geometryDescriptor.m_NumVertices = numVerts;
            geometryDescriptor.m_pIndices = &indexList[0];
            geometryDescriptor.m_NumIndices = numIndices;
            geometryDescriptor.m_pMaterial = MaterialList[pMesh->mMaterialIndex];
            Geometry *pGeometry = pRenderer->CreateGeometry(&geometryDescriptor);
            pScene->AddGeometry(pGeometry);
        }
    }
    
    {
        CreateLightDescriptor CreateLight;
        CreateDirectionalLight CreateDirectional;
        CreateDirectional.m_EmissionDirection = Vec3(1.0f, -1.0, -1.0);
        CreateLight.m_Color = Vec3(1.0f, 1.0f, 1.0f);
        CreateLight.m_LightType = CreateLightDescriptor::DIRECTIONAL_LIGHT;
        CreateLight.m_pCreateDirectionalLight = &CreateDirectional;

        Light *pLight = pRenderer->CreateLight(&CreateLight);
        pScene->AddLight(pLight);
    }

    assert(assimpScene.HasCameras() && assimpScene.mNumCameras == 1);
    auto pCam = assimpScene.mCameras[0];

    aiVector3D lookAt;
        
    aiNode* rootNode = assimpScene.mRootNode;
    aiNode* camNode = assimpScene.mRootNode->FindNode(pCam->mName);
        
    aiMatrix4x4 camMat;
    pCam->GetCameraMatrix(camMat);

    XMVECTOR position = XMVectorSet(pCam->mPosition.x, pCam->mPosition.y, pCam->mPosition.z, 1.0);
    XMMATRIX camMatrix = XMMatrixSet(
        camMat.a1, camMat.a2, camMat.a3, camMat.a4,
        camMat.b1, camMat.b2, camMat.b3, camMat.b4,
        camMat.c1, camMat.c2, camMat.c3, camMat.c4,
        camMat.d1, camMat.d2, camMat.d3, camMat.d4);
    position = XMVector4Transform(position, camMatrix);

    const float LensHeight = 2.0f;
    const float AspectRatio = (float)g_Width / (float)g_Height;
    const float LensWidth = LensHeight * AspectRatio;
    const float FocalLength = LensWidth / (2.0f* tan(pCam->mHorizontalFOV / 2.0f));
    float VerticalFov = 2 * atan(LensHeight / (2.0f * FocalLength));

    CreateCameraDescriptor CameraDescriptor = {};
    CameraDescriptor.m_Height = g_Height;
    CameraDescriptor.m_Width = g_Width;
    CameraDescriptor.m_FocalPoint = ConvertVec3(pCam->mPosition);
    CameraDescriptor.m_LookAt = ConvertVec3(pCam->mLookAt);
    CameraDescriptor.m_Up = ConvertVec3(pCam->mUp);
    CameraDescriptor.m_NearClip = pCam->mClipPlaneNear;
    CameraDescriptor.m_FarClip = pCam->mClipPlaneFar;
    CameraDescriptor.m_VerticalFieldOfView = VerticalFov;

    *ppCamera = pRenderer->CreateCamera(&CameraDescriptor);
}
#endif
void RenderText(float fps)
{
    g_pTextWriter->Begin();
    g_pTextWriter->SetInsertionPos(5, 5);
    g_pTextWriter->SetForegroundColor(Colors::Yellow);
    if (g_ShowGoldenImage)
    {
        g_pTextWriter->DrawTextLine(L"Golden Image");
    }
    else
    {
        g_pTextWriter->DrawTextLine(g_ActiveRenderer == D3D11 ? L"Rasterizer" : L"Raytracer");
    }
    g_pTextWriter->DrawFormattedTextLine(L"%f", fps);
    g_pTextWriter->DrawFormattedTextLine(L"Mouse coord: %d, %d", g_MouseX, g_MouseY);
    g_pTextWriter->End();
}

void CALLBACK OnFrameRender(_In_ ID3D11Device* pd3dDevice, _In_ ID3D11DeviceContext* pd3dImmediateContext, _In_ double fTime, _In_ float fElapsedTime, _In_opt_ void* pUserContext)
{
    UpdateCamera();


    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    FAIL_CHK(FAILED(hr), "Failed to get the back buffer");

    if (g_ShowGoldenImage)
    {
        g_pImmediateContext->CopyResource(pBackBuffer, g_pGoldenImage);
    }
    else
    {
        g_pRenderer[g_ActiveRenderer]->DrawScene(g_pCamera[g_ActiveRenderer], g_pScene[g_ActiveRenderer], g_RenderSettings);
        g_pImmediateContext->CopyResource(pBackBuffer, g_pCanvas->GetCanvasResource());
    }

    g_GUI.OnRender(fElapsedTime);

    float fps = 1.0f / fElapsedTime;
    RenderText(fps);
}

HRESULT CALLBACK OnResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr = g_DialogResourceManager.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc);
    FAIL_CHK(FAILED(hr), "Failed Resize of dialog manager");

    g_GUI.SetLocation(pBackBufferSurfaceDesc->Width - 170, 0);
    g_GUI.SetSize(170, 170);

    return S_OK;
}

LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    bool* pbNoFurtherProcessing, void* pUserContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_GUI.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    return 0;
}

void SetMaterialProperty(UINT ControlID)
{
    if (g_SelectedMaterialName.size() > 0)
    {
        float Value = ((CDXUTSlider*)g_GUI.GetControl(ControlID))->GetValue() / (float)MAX_SLIDER_VALUE;

        for (UINT i = 0; i < NUM_RENDERER_TYPES; i++)
        {
            Material *pMaterial = g_MaterialList[i][g_SelectedMaterialName];
            switch (ControlID)
            {
            case ROUGHNESS_SLIDER:
                pMaterial->SetRoughness(Value);
                break;
            case REFLECTIVITY_SLIDER:
                pMaterial->SetReflectivity(Value);
                break;
            }
        }
    }
}

void SetReflectivityOnSelectedGeometry(float Reflectivity)
{
    if (g_SelectedMaterialName.size() > 0)
    {
        for (UINT i = 0; i < NUM_RENDERER_TYPES; i++)
        {
            g_MaterialList[i][g_SelectedMaterialName]->SetReflectivity(Reflectivity);
        }
    }
}

void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    switch (nControlID)
    {
    case CMD_CHANGE_RENDERER:
    {
        g_ActiveRenderer = (RENDERER_TYPE)((g_ActiveRenderer + 1) % NUM_RENDERER_TYPES);
        break;
    }
    case CMD_SHOW_GOLDEN_IMAGE:
        if(g_pGoldenImage) g_ShowGoldenImage = !g_ShowGoldenImage;
        break;
    case CMD_CAMERA_MODE:
        g_CameraModeEnabled = !g_CameraModeEnabled;
        break;
    case ROUGHNESS_SLIDER:
    case REFLECTIVITY_SLIDER:
        SetMaterialProperty(nControlID);
        break;
    case GAMMA_CORRECTION_CHECK_BOX:
        g_RenderSettings.m_GammaCorrection = !g_RenderSettings.m_GammaCorrection;
        break;
    }
}

void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

void CALLBACK OnKeyPress(_In_ UINT nChar, _In_ bool bKeyDown, _In_ bool bAltDown, _In_opt_ void* pUserContext)
{
    const float CAMERA_SPEED = 0.3f;
    for (UINT i = 0; i < RENDERER_TYPE::NUM_RENDERER_TYPES; i++)
    {
        switch (nChar)
        {
        case 'Q':
            g_pCamera[i]->Translate(Vec3(0.0f, 0.0f, CAMERA_SPEED));
            break;
        case 'E':
            g_pCamera[i]->Translate(Vec3(0.0f, 0.0f, -CAMERA_SPEED));
            break;
        case VK_UP:
        case 'W':
            g_pCamera[i]->Translate(Vec3(0.0f, CAMERA_SPEED, 0.0f));
            break;
        case VK_DOWN:
        case 'S':
            g_pCamera[i]->Translate(Vec3(0.0f, -CAMERA_SPEED, 0.0f));
            break;
        case VK_RIGHT:
        case 'D':
            g_pCamera[i]->Translate(Vec3(-CAMERA_SPEED, 0.0f, 0.0f));
            break;
        case VK_LEFT:
        case 'A':
            g_pCamera[i]->Translate(Vec3(CAMERA_SPEED, 0.0f, 0.0f));
            break;
        }
    }
}

void UpdateCamera()
{
    if (g_CameraModeEnabled)
    {
        static bool firstTimeQuery = true;
        static LARGE_INTEGER lastTime;
        static LARGE_INTEGER performanceFrequency;
        if (firstTimeQuery)
        {
            firstTimeQuery = false;
            QueryPerformanceCounter(&lastTime);
            QueryPerformanceFrequency(&performanceFrequency);
        }
        else
        {
            LARGE_INTEGER newTime;
            QueryPerformanceCounter(&newTime);
            float deltaTime = (float)(newTime.QuadPart - lastTime.QuadPart) / (float)performanceFrequency.QuadPart;
            lastTime = newTime;

            float deltaX = 0, deltaY = 0;
            if (g_MouseX < 100)
            {
                deltaX += 1;
            }
            else if (g_MouseX > g_Width - 100)
            {
                deltaX -= 1;
            }

            if (g_MouseY < 100)
            {
                deltaY += 1;
            }
            else if (g_MouseY > g_Height - 100)
            {
                deltaY -= 1;
            }

            for (UINT i = 0; i < RENDERER_TYPE::NUM_RENDERER_TYPES; i++)
            {
                g_pCamera[i]->Rotate(0.0f, -CAMERA_SIDE_SCREEN_ROTATION_SPEED * deltaX * deltaTime, CAMERA_SIDE_SCREEN_ROTATION_SPEED * deltaY * deltaTime);
            }
        }
    }
}

bool IsOverGUI(UINT x, UINT y)
{
    // GetControlAtPoint is relative to g_GUI's coordinate space, so 
    // the mouse coordinates need to be transformed first
    POINT MouseCoord;
    g_GUI.GetLocation(MouseCoord);
    MouseCoord.x = x - MouseCoord.x;
    MouseCoord.y = y - MouseCoord.y;

    return g_GUI.GetControlAtPoint(MouseCoord) != nullptr;
}

void SetSliderValue(UINT ControlID, float value)
{
    ((CDXUTSlider*)g_GUI.GetControl(ControlID))->SetValue(value * MAX_SLIDER_VALUE);
}

void SetGUISliders(Material *pMaterial)
{
    const float roughness = pMaterial ? pMaterial->GetRoughness() : 0.0f;
    const float reflectivity = pMaterial ? pMaterial->GetReflectivity() : 0.0f;

    SetSliderValue(ROUGHNESS_SLIDER, roughness);
    SetSliderValue(REFLECTIVITY_SLIDER, reflectivity);
}

void CALLBACK OnMouseMove(_In_ bool bLeftButtonDown, _In_ bool bRightButtonDown, _In_ bool bMiddleButtonDown,
    _In_ bool bSideButton1Down, _In_ bool bSideButton2Down, _In_ int nMouseWheelDelta,
    _In_ int xPos, _In_ int yPos, _In_opt_ void* pUserContext)
{
    if (!g_MouseInitialized)
    {
        g_MouseX = xPos;
        g_MouseY = yPos;
        g_MouseInitialized = true;
    }
    else
    {
        if (g_CameraModeEnabled)
        {
            int deltaX = g_MouseX - xPos;
            int deltaY = g_MouseY - yPos;
            
            for (UINT i = 0; i < RENDERER_TYPE::NUM_RENDERER_TYPES; i++)
            {
                g_pCamera[i]->Rotate(0.0f, -CAMERA_ROTATION_SPEED * deltaX, CAMERA_ROTATION_SPEED * deltaY);
            }
        }
        g_MouseX = xPos;
        g_MouseY = yPos;

        if (bLeftButtonDown && !IsOverGUI(xPos, yPos))
        {
            const UINT RendererIndex = RAYTRACER; // Not implemented in D3D11 renderer
            Geometry *pGeometry = g_pRenderer[RendererIndex]->GetGeometryAtPixel(g_pCamera[RendererIndex], g_pScene[RendererIndex], Vec2(xPos, yPos));
            SetGUISliders(pGeometry ? pGeometry->GetMaterial() : nullptr);

            g_SelectedMaterialName = "";
            if (pGeometry)
            {
                for (auto &materialPair : g_MaterialList[RAYTRACER])
                {
                    if (pGeometry->GetMaterial() == materialPair.second)
                    {
                        g_SelectedMaterialName = materialPair.first;
                    }
                }
            }
        }
    }
}

void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(g_pTextWriter);
}