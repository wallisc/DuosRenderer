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

#include "assimp/inc/Importer.hpp"      // C++ importer interface
#include "assimp/inc/scene.h"      
#include "assimp/inc/postprocess.h"     // Post processing flags

#include <list>


using namespace DirectX;
using namespace Assimp;

#define WIDTH 800
#define HEIGHT 600

enum
{
	CMD_CHANGE_RENDERER
};

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

Renderer *g_pRenderer[NUM_RENDERER_TYPES];
Scene *g_pScene[NUM_RENDERER_TYPES];
Camera *g_pCamera[NUM_RENDERER_TYPES];
RENDERER_TYPE g_ActiveRenderer = D3D11;
D3D11Canvas *g_pCanvas;

bool g_MouseInitialized = false;
int g_MouseX;
int g_MouseY;

CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CDXUTDialog  g_GUI;
CDXUTTextHelper* g_pTextWriter = nullptr;


ID3D11Device *g_pDevice;
ID3D11DeviceContext *g_pImmediateContext;

IDXGISwapChain1* g_pSwapChain1;
IDXGISwapChain* g_pSwapChain;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
void InitSceneAndCamera(_In_ Renderer *, _In_ const aiScene &assimpScene, _Out_ Scene **, _Out_ Camera **);
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
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



	DXUTInit(true, true, nullptr); // Parse the command line, show msgboxes on error, no extra command line params
	DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen

	DXUTCreateWindow(L"Duos Renderer");

	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_1, true, WIDTH, HEIGHT);
	DXUTMainLoop();

	return DXUTGetExitCode();
}

const aiScene* InitAssimpScene(_In_ char *pSceneFile);

HRESULT CALLBACK OnDeviceCreated(_In_ ID3D11Device* pd3dDevice, _In_ const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, _In_opt_ void* pUserContext)
{
	g_pDevice = pd3dDevice;
	g_pImmediateContext = DXUTGetD3D11DeviceContext();
	g_pCanvas = new D3D11Canvas(g_pDevice, g_pImmediateContext, WIDTH, HEIGHT);
	g_pSwapChain = DXUTGetDXGISwapChain();

	HRESULT hr = g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, g_pImmediateContext);
	g_pTextWriter = new CDXUTTextHelper(pd3dDevice, g_pImmediateContext, &g_DialogResourceManager, 15);
	g_GUI.Init(&g_DialogResourceManager);

	g_GUI.SetCallback(OnGUIEvent); int iY = 10;
	g_GUI.AddButton(CMD_CHANGE_RENDERER, L"Change renderer (space)", 0, iY, 170, 22, VK_SPACE);

	FAIL_CHK(FAILED(hr), "Failed to create DXUT dialog manager");

	const aiScene* pAssimpScene = InitAssimpScene("");
	for (UINT i = 0; i < NUM_RENDERER_TYPES; i++)
	{
		switch (i)
		{
		case RENDERER_TYPE::D3D11:
			g_pRenderer[i] = new D3D11Renderer(g_hWnd, WIDTH, HEIGHT);
			break;
		case RENDERER_TYPE::RAYTRACER:
			g_pRenderer[i] = new RTRenderer(WIDTH, HEIGHT);
			break;
		default:
			break;
		}

		g_pRenderer[i]->SetCanvas(g_pCanvas);
		InitSceneAndCamera(g_pRenderer[i], *pAssimpScene, &g_pScene[i], &g_pCamera[i]);
	}
	return	S_OK;
}

const aiScene* InitAssimpScene(_In_ char *pSceneFile)
{
	// Create an instance of the Importer class
	Assimp::Importer importer;
	// And have it read the given file with some example postprocessing
	// Usually - if speed is not the most important aspect for you - you'll 
	// propably to request more postprocessing than we do in this example.
	const aiScene* AssimpScene = importer.ReadFile(pSceneFile,
		aiProcess_Triangulate |
		aiProcess_MakeLeftHanded |
		aiProcess_FlipUVs |
		aiProcess_FlipWindingOrder |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_PreTransformVertices);

	return AssimpScene;
}

Vec3 ConvertVec3(const aiVector3D &vec)
{
	return Vec3(vec.x, vec.y, vec.z);
}

Vec2 ConvertVec2(const aiVector3D &vec)
{
	return Vec2(vec.x, vec.y);
}

void InitSceneAndCamera(_In_ Renderer *pRenderer, _In_ const aiScene &assimpScene, _Out_ Scene **ppScene, _Out_ Camera **ppCamera)
{
	*ppScene = pRenderer->CreateScene();
	Scene *pScene = *ppScene;

	std::vector<Material *> materialList(assimpScene.mNumMaterials);
	for (UINT i = 0; i < assimpScene.mNumMaterials; i++)
	{
		aiMaterial *pMat = assimpScene.mMaterials[i];
		assert(pMat);

		aiString path;
		pMat->GetTexture(aiTextureType_DIFFUSE, 0, &path);

		CreateMaterialDescriptor CreateMaterialDescriptor;
		CreateMaterialDescriptor.m_TextureName = path.C_Str();
		materialList[i] = pRenderer->CreateMaterial(&CreateMaterialDescriptor);
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
			for (UINT vertIdx = 0; vertIdx < numVerts; vertIdx++)
			{
				Vertex &vertex = vertexList[vertIdx];
				vertex.m_Position = ConvertVec3(pMesh->mVertices[i]);
				vertex.m_Normal = ConvertVec3(pMesh->mNormals[vertIdx]);
				vertex.m_Tex = ConvertVec2(pMesh->mTextureCoords[0][vertIdx]);
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
			geometryDescriptor.m_pMaterial = materialList[pMesh->mMaterialIndex];
			Geometry *pGeometry = pRenderer->CreateGeometry(&geometryDescriptor);
			pScene->AddGeometry(pGeometry);
		}
	}
	
	{
		CreateLightDescriptor CreateLight;
		CreateDirectionalLight CreateDirectional;
		CreateDirectional.m_EmissionDirection = Vec3(1.0f, -1.0, 1.0);
		CreateLight.m_Color = Vec3(1.0f, 1.0f, 1.0f);
		CreateLight.m_LightType = CreateLightDescriptor::DIRECTIONAL_LIGHT;
		CreateLight.m_pCreateDirectionalLight = &CreateDirectional;

		Light *pLight = pRenderer->CreateLight(&CreateLight);
		pScene->AddLight(pLight);
	}

	if (assimpScene.HasCameras())
	{
		assert(assimpScene.mNumCameras == 1);
		auto pCam = assimpScene.mCameras[0];

		CreateCameraDescriptor CameraDescriptor = {};
		CameraDescriptor.m_Height = HEIGHT;
		CameraDescriptor.m_Width = WIDTH;
		CameraDescriptor.m_FocalPoint = ConvertVec3(pCam->mPosition);
		CameraDescriptor.m_LookAt = ConvertVec3(pCam->mLookAt);
		CameraDescriptor.m_Up = ConvertVec3(pCam->mUp);
		CameraDescriptor.m_NearClip = pCam->mClipPlaneNear;
		CameraDescriptor.m_FarClip = pCam->mClipPlaneFar;
		CameraDescriptor.m_FieldOfView = pCam->mHorizontalFOV * 2.0f / pCam->mAspect;

		*ppCamera = pRenderer->CreateCamera(&CameraDescriptor);
	}
	else
	{
		assert(false);
	}
}

void RenderText(float fps)
{
	g_pTextWriter->Begin();
	g_pTextWriter->SetInsertionPos(5, 5);
	g_pTextWriter->SetForegroundColor(Colors::Yellow);
	g_pTextWriter->DrawTextLine(g_ActiveRenderer == D3D11 ? L"Rasterizer" : L"Raytracer");
	g_pTextWriter->DrawFormattedTextLine(L"%f", fps);
	g_pTextWriter->End();
}

void CALLBACK OnFrameRender(_In_ ID3D11Device* pd3dDevice, _In_ ID3D11DeviceContext* pd3dImmediateContext, _In_ double fTime, _In_ float fElapsedTime, _In_opt_ void* pUserContext)
{
	g_pRenderer[g_ActiveRenderer]->DrawScene(g_pCamera[g_ActiveRenderer], g_pScene[g_ActiveRenderer]);


	ID3D11Texture2D* pBackBuffer = nullptr;
	HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	
	FAIL_CHK(FAILED(hr), "Failed to get the back buffer");

	g_pImmediateContext->CopyResource(pBackBuffer, g_pCanvas->GetCanvasResource());

	g_GUI.OnRender(fElapsedTime);

	float fps = 1.0f / fElapsedTime;
	RenderText(fps);

	hr = g_pSwapChain->Present(0, 0);
	FAIL_CHK(FAILED(hr), "Failed to present");
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

void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
	switch (nControlID)
	{
	case CMD_CHANGE_RENDERER:
		g_ActiveRenderer = (RENDERER_TYPE)((g_ActiveRenderer + 1) % NUM_RENDERER_TYPES);
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
		case VK_UP:
		case 'W':
			g_pCamera[i]->Translate(Vec3(0.0f, 0.0f, CAMERA_SPEED));
			break;
		case VK_DOWN:
		case 'S':
			g_pCamera[i]->Translate(Vec3(0.0f, 0.0f, -CAMERA_SPEED));
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
		const float CAMERA_ROTATION_SPEED = 3.14 / (WIDTH / 2.0);

		int deltaX = g_MouseX - xPos;
		int deltaY = g_MouseY - yPos;
		g_MouseX = xPos;
		g_MouseY = yPos;
		
		for (UINT i = 0; i < RENDERER_TYPE::NUM_RENDERER_TYPES; i++)
		{
			g_pCamera[i]->Rotate(0.0f, -CAMERA_ROTATION_SPEED * deltaX, CAMERA_ROTATION_SPEED * deltaY);
		}
	}
}

void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	g_DialogResourceManager.OnD3D11DestroyDevice();
	DXUTGetGlobalResourceCache().OnDestroyDevice();
	SAFE_DELETE(g_pTextWriter);
}