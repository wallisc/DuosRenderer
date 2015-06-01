#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "DDSTextureLoader.h"
#include "resource.h"
#include "Renderer.h"
#include "D3D11Renderer.h"

using namespace DirectX;

#define WIDTH 800
#define HEIGHT 600

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;

Renderer *g_pRenderer;
Scene *g_pScene;
Camera *g_pCamera;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
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

	g_pRenderer = new D3D11Renderer(g_hWnd, WIDTH, HEIGHT);

	g_pScene = g_pRenderer->CreateScene();

	CreateGeometryDescriptor CreateBoxDescriptor = {};
	Vertex BoxVertices[] =
	{
		{ Vec3(-1.0f, 1.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f),  Vec2(1.0f, 0.0f), },
		{ Vec3(1.0f, 1.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f)  ,Vec2(0.0f, 0.0f), },
		{ Vec3(1.0f, 1.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f)  ,Vec2(0.0f, 1.0f), },
		{ Vec3(-1.0f, 1.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f) , Vec2(1.0f, 1.0f), },

		{ Vec3(-1.0f, -1.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f),  Vec2(0.0f, 0.0f), },
		{ Vec3(1.0f, -1.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f) , Vec2(1.0f, 0.0f), },
		{ Vec3(1.0f, -1.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)  ,Vec2(1.0f, 1.0f), },
		{ Vec3(-1.0f, -1.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f) , Vec2(0.0f, 1.0f), },

		{ Vec3(-1.0f, -1.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f)  ,Vec2(0.0f, 1.0f), },
		{ Vec3(-1.0f, -1.0f, -1.0f), Vec3(-1.0f, 0.0f, 0.0f)  ,Vec2(1.0f, 1.0f), },
		{ Vec3(-1.0f, 1.0f, -1.0f), Vec3(-1.0f, 0.0f, 0.0f)  ,Vec2(1.0f, 0.0f), },
		{ Vec3(-1.0f, 1.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f)  ,Vec2(0.0f, 0.0f), },

		{ Vec3(1.0f, -1.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f)  ,Vec2(1.0f, 1.0f), },
		{ Vec3(1.0f, -1.0f, -1.0f), Vec3(1.0f, 0.0f, 0.0f) , Vec2(0.0f, 1.0f), },
		{ Vec3(1.0f, 1.0f, -1.0f), Vec3(1.0f, 0.0f, 0.0f) , Vec2(0.0f, 0.0f), },
		{ Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f) , Vec2(1.0f, 0.0f), },

		{ Vec3(-1.0f, -1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f),  Vec2(0.0f, 1.0f), },
		{ Vec3(1.0f, -1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f),  Vec2(1.0f, 1.0f), },
		{ Vec3(1.0f, 1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f)  ,Vec2(1.0f, 0.0f), },
		{ Vec3(-1.0f, 1.0f, -1.0f), Vec3(0.0f, 0.0f, -1.0f)  ,Vec2(0.0f, 0.0f), },

		{ Vec3(-1.0f, -1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f) ,  Vec2(1.0f, 1.0f), },
		{ Vec3(1.0f, -1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f)  , Vec2(0.0f, 1.0f), },
		{ Vec3(1.0f, 1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f) ,  Vec2(0.0f, 0.0f), },
		{ Vec3(-1.0f, 1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f) ,  Vec2(1.0f, 0.0f), },
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
	CreateBoxDescriptor.m_pVertices = BoxVertices;
	CreateBoxDescriptor.m_NumVertices = ARRAYSIZE(BoxVertices);
	CreateBoxDescriptor.m_pIndices = BoxIndices;
	CreateBoxDescriptor.m_NumIndices = ARRAYSIZE(BoxIndices);

	Geometry *pBox = g_pRenderer->CreateGeometry(&CreateBoxDescriptor);
	g_pScene->AddGeometry(pBox);

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
	CreatePlaneDescriptor.m_NumVertices = ARRAYSIZE(PlaneVertices);
	CreatePlaneDescriptor.m_pVertices = PlaneVertices;
	Geometry *pPlane = g_pRenderer->CreateGeometry(&CreatePlaneDescriptor);
	g_pScene->AddGeometry(pPlane);

	CreateCameraDescriptor CameraDescriptor = {};
	CameraDescriptor.m_Height = 600;
	CameraDescriptor.m_Width = 800;
	CameraDescriptor.m_Position = Vec3(0.0f, 3.0f, -11.0f);
	CameraDescriptor.m_LookAt = Vec3(0.0f, 1.0f, 0.0f);
	CameraDescriptor.m_Up = Vec3(0.0f, 1.0f, 0.0f);
	CameraDescriptor.m_NearClip = 0.01f;
	CameraDescriptor.m_FarClip = 100.0f;

	g_pCamera = g_pRenderer->CreateCamera(&CameraDescriptor);

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

	g_pRenderer->DestroyGeometry(pBox);
	g_pRenderer->DestroyGeometry(pPlane);
	g_pRenderer->DestroyScene(g_pScene);

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
    g_hWnd = CreateWindow( L"TutorialWindowClass", L"Direct3D 11 Tutorial 7",
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


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	g_pRenderer->DrawScene(g_pCamera, g_pScene);
}
