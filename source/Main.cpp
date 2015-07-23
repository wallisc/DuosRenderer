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


using namespace DirectX;

#define WIDTH 800
#define HEIGHT 600

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

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

	UINT CeateDeviceFlags = 0;
#ifdef DEBUG
	CeateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL FeatureLevels = D3D_FEATURE_LEVEL_11_1;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, CeateDeviceFlags, &FeatureLevels, 1,
		D3D11_SDK_VERSION, &g_pDevice, nullptr, &g_pImmediateContext);
	FAIL_CHK(FAILED(hr), "Failed D3D11CreateDevice");

	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		FAIL_CHK(FAILED(hr), "Failed to query interface for DXGIDevice");
		hr = g_pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		IDXGIAdapter* adapter = nullptr;
		hr = dxgiDevice->GetAdapter(&adapter);
		if (SUCCEEDED(hr))
		{
			hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
			adapter->Release();
		}
		dxgiDevice->Release();
	}

	g_pCanvas = new D3D11Canvas(g_pDevice, g_pImmediateContext, WIDTH, HEIGHT);

	// Create swap chain
	CComPtr<IDXGIFactory2> dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	FAIL_CHK(!dxgiFactory2, "DXGIFactory2 is null");
	DXGI_SWAP_CHAIN_DESC1 sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.Width = WIDTH;
	sd.Height = HEIGHT;
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;

	hr = dxgiFactory2->CreateSwapChainForHwnd(g_pDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
	if (SUCCEEDED(hr))
	{
		hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
	}

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


    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
		else
        {
            Render();
        }
    }

	// TODO: Memory leak
	//g_pRenderer->DestroyGeometry(pBox);
	//g_pRenderer->DestroyGeometry(pPlane);
	//g_pRenderer->DestroyScene(g_pScene);

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, WIDTH, HEIGHT};
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( L"TutorialWindowClass", L"Duos Renderer",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
                           nullptr );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message )
    {
    case WM_PAINT:
        hdc = BeginPaint( hWnd, &ps );
        EndPaint( hWnd, &ps );
        break;
	case WM_KEYDOWN:
		g_ActiveRenderer = (RENDERER_TYPE)((g_ActiveRenderer + 1) % NUM_RENDERER_TYPES);
		break;
    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

        // Note that this tutorial does not handle resizing (WM_SIZE) requests,
        // so we created the window without the resize border.

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
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

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------g_ActiveRenderer
void Render()
{
	g_pRenderer[g_ActiveRenderer]->DrawScene(g_pCamera[g_ActiveRenderer], g_pScene[g_ActiveRenderer]);


	ID3D11Texture2D* pBackBuffer = nullptr;
	HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	
	FAIL_CHK(FAILED(hr), "Failed to get the back buffer");

	g_pImmediateContext->CopyResource(pBackBuffer, g_pCanvas->GetCanvasResource());
	hr = g_pSwapChain->Present(0, 0);
	FAIL_CHK(FAILED(hr), "Failed to present");
}
