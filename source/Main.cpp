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
#if 0
#include "assimp/include/Importer.hpp"      // C++ importer interface
#include "assimp/include/postprocess.h"     // Post processing flags
#endif
using namespace DirectX;

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
void InitSceneAndCamera(_In_ Renderer *, _Out_ Scene **, _Out_ Camera **);
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
		InitSceneAndCamera(g_pRenderer[i], &g_pScene[i], &g_pCamera[i]);
	}
	return	S_OK;
}


void InitSceneAndCamera(_In_ Renderer *pRenderer, _Out_ Scene **ppScene, _Out_ Camera **ppCamera)
{
	*ppScene = pRenderer->CreateScene();
	Scene *pScene = *ppScene;

	{
		CreateGeometryDescriptor CreateBoxDescriptor = {};
		Vertex BoxVertices[] =
		{
			{ Vec3(-1.0f, 1.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 0.0f), },
			{ Vec3(1.0f, 1.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 0.0f), },
			{ Vec3(1.0f, 1.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 1.0f), },
			{ Vec3(-1.0f, 1.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 1.0f), },

			{ Vec3(-1.0f, -1.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f), Vec2(0.0f, 0.0f), },
			{ Vec3(1.0f, -1.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f), Vec2(1.0f, 0.0f), },
			{ Vec3(1.0f, -1.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f), Vec2(1.0f, 1.0f), },
			{ Vec3(-1.0f, -1.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f), Vec2(0.0f, 1.0f), },

			{ Vec3(-1.0f, -1.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(0.0f, 1.0f), },
			{ Vec3(-1.0f, -1.0f, -1.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(1.0f, 1.0f), },
			{ Vec3(-1.0f, 1.0f, -1.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(1.0f, 0.0f), },
			{ Vec3(-1.0f, 1.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(0.0f, 0.0f), },

			{ Vec3(1.0f, -1.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f), Vec2(1.0f, 1.0f), },
			{ Vec3(1.0f, -1.0f, -1.0f), Vec3(1.0f, 0.0f, 0.0f), Vec2(0.0f, 1.0f), },
			{ Vec3(1.0f, 1.0f, -1.0f), Vec3(1.0f, 0.0f, 0.0f), Vec2(0.0f, 0.0f), },
			{ Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f), Vec2(1.0f, 0.0f), },

			{ Vec3(-1.0f, -1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f), Vec2(0.0f, 1.0f), },
			{ Vec3(1.0f, -1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f), Vec2(1.0f, 1.0f), },
			{ Vec3(1.0f, 1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f), Vec2(1.0f, 0.0f), },
			{ Vec3(-1.0f, 1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f), Vec2(0.0f, 0.0f), },

			{ Vec3(-1.0f, -1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), Vec2(1.0f, 1.0f), },
			{ Vec3(1.0f, -1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f), },
			{ Vec3(1.0f, 1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), Vec2(0.0f, 0.0f), },
			{ Vec3(-1.0f, 1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f), },
		};

		unsigned int BoxIndices[] =
		{
			3, 1, 0,
			2, 1, 3,

			6, 4, 5,
			7, 4, 6,

			11, 9, 8,
			10, 9, 11,

			14, 12, 13,
			15, 12, 14,

			19, 17, 16,
			18, 17, 19,

			22, 20, 21,
			23, 20, 22
		};

		CreateMaterialDescriptor CreateBoxMaterialDescriptor;
		CreateBoxMaterialDescriptor.m_TextureName = "crate.png";
		Material *pBoxMaterial = pRenderer->CreateMaterial(&CreateBoxMaterialDescriptor);
		CreateBoxDescriptor.m_pVertices = BoxVertices;
		CreateBoxDescriptor.m_NumVertices = ARRAYSIZE(BoxVertices);
		CreateBoxDescriptor.m_pIndices = BoxIndices;
		CreateBoxDescriptor.m_NumIndices = ARRAYSIZE(BoxIndices);
		CreateBoxDescriptor.m_pMaterial = pBoxMaterial;

		Geometry *pBox = pRenderer->CreateGeometry(&CreateBoxDescriptor);
		pScene->AddGeometry(pBox);
	}

	{
		CreateGeometryDescriptor CreatePlaneDescriptor = {};
		Vertex PlaneVertices[] =
		{
			{ Vec3(-5.0f, -1.0f, 5.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 1.0f), },
			{ Vec3(5.0f, -1.0f, -5.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 0.0f), },
			{ Vec3(-5.0f, -1.0f, -5.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 0.0f), },

			{ Vec3(5.0f, -1.0f, 5.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 1.0f), },
			{ Vec3(5.0f, -1.0f, -5.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 0.0f), },
			{ Vec3(-5.0f, -1.0f, 5.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 1.0f), },
		};
		CreateMaterialDescriptor CreatePlaneMaterialDescriptor;
		CreatePlaneMaterialDescriptor.m_TextureName = "tile.png";
		Material *pPlaneMaterial = pRenderer->CreateMaterial(&CreatePlaneMaterialDescriptor);

		CreatePlaneDescriptor.m_pMaterial = pPlaneMaterial;
		CreatePlaneDescriptor.m_NumVertices = ARRAYSIZE(PlaneVertices);
		CreatePlaneDescriptor.m_pVertices = PlaneVertices;
		Geometry *pPlane = pRenderer->CreateGeometry(&CreatePlaneDescriptor);
		pScene->AddGeometry(pPlane);
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

	CreateCameraDescriptor CameraDescriptor = {};
	CameraDescriptor.m_Height = HEIGHT;
	CameraDescriptor.m_Width = WIDTH;
	CameraDescriptor.m_FocalPoint = Vec3(0.0f, 2.0f, -13.0f);
	CameraDescriptor.m_LookAt = Vec3(0.0f, 2.0f, 0.0f);
	CameraDescriptor.m_Up = Vec3(0.0f, 1.0f, 0.0f);
	CameraDescriptor.m_NearClip = 0.01f;
	CameraDescriptor.m_FarClip = 100.0f;
	CameraDescriptor.m_FieldOfView = .78f;

	*ppCamera = pRenderer->CreateCamera(&CameraDescriptor);
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