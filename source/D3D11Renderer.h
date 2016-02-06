#include "Renderer.h"

#include <d3d11_1.h>
#include <directxmath.h>
#include "D3D11Canvas.h"
#include <unordered_map>
#include <memory>

struct CBCamera
{
    DirectX::XMVECTOR m_Dimensions;
    DirectX::XMVECTOR m_ClipDistance;
};

struct CBViewProjectionTransforms
{
    DirectX::XMMATRIX m_View;
    DirectX::XMMATRIX m_Projection;
    DirectX::XMMATRIX m_InvTransView;
    DirectX::XMVECTOR m_CamPos;
};

struct CBDirectionalLight
{
    DirectX::XMVECTOR m_Direction;
    DirectX::XMVECTOR m_Color;
};

struct CBMaterial
{
    static const UINT REFLECTIVITY_INDEX = 0;
    static const UINT ROUGHNESS_INDEX = 1;

    DirectX::XMVECTOR m_Diffuse;
    DirectX::XMVECTOR m_MaterialProperties; // (Reflectivity (R0), Roughness, ,)
};

class D3D11Material : public Material
{
public:
    D3D11Material(_In_ ID3D11Device *pDevice, ID3D11DeviceContext *pContext, CreateMaterialDescriptor *pCreateMaterialDescriptor);
    ID3D11ShaderResourceView *GetShaderResourceView() const { return pTextureResourceView; }
    ID3D11ShaderResourceView *GetNormalMap() const { return pNormalMap; }

    bool HasTexture() { return pTextureResourceView != nullptr; }
    bool HasNormalMap() { return pNormalMap != nullptr; }
    ID3D11Buffer *GetMaterialBuffer() { return m_pMaterialBuffer; }

    float GetRoughness() const { return DirectX::XMVectorGetByIndex(m_CBMaterial.m_MaterialProperties, CBMaterial::ROUGHNESS_INDEX); }
    float GetReflectivity() const { return DirectX::XMVectorGetByIndex(m_CBMaterial.m_MaterialProperties, CBMaterial::REFLECTIVITY_INDEX); }

    void SetRoughness(float Roughness);
    void SetReflectivity(float Reflectivity);
private:
    void UpdateMaterialProperty(float value, unsigned int index);

    void UpdateMaterialBuffer();
    ID3D11DeviceContext *m_pContext;
    CBMaterial m_CBMaterial;
    ID3D11Buffer *m_pMaterialBuffer;
    ID3D11ShaderResourceView *pTextureResourceView;
    ID3D11ShaderResourceView *pNormalMap;
};

class D3D11Transformable : public Transformable
{
public:
    D3D11Transformable(_In_ ID3D11Device *pDevice, _In_ ID3D11DeviceContext *pImmediateContext);
    virtual void Translate(_In_ const Vec3 &translationVector);
    virtual void Rotate(float row, float yaw, float pitch) { assert(false); }


protected:
    ID3D11Buffer *GetWorldTransformBuffer();
    const DirectX::XMMATRIX &GetWorldTransform() { return m_WorldMatrix; }

private:
    ID3D11DeviceContext *m_pImmediateContext;
    ID3D11Buffer *m_pWorldTransform;
    DirectX::XMMATRIX m_WorldMatrix;
};

class D3D11Geometry : public D3D11Transformable, public Geometry
{
public:
    D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ ID3D11DeviceContext *pImmediateContext, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
    ~D3D11Geometry();

    ID3D11Buffer *GetVertexBuffer() const { return m_pVertexBuffer; }
    ID3D11Buffer *GetIndexBuffer() const { return m_pIndexBuffer; }
    unsigned int GetVertexCount() const { return m_VertexCount; }
    unsigned int GetIndexCount() const { return m_IndexCount; }
    D3D11Material *GetD3D11Material() const { return m_pMaterial; }
    Material *GetMaterial() const { return GetD3D11Material(); }

    DirectX::XMVECTOR GetMaxDimensions() const { return m_MaxDimensions; }
    DirectX::XMVECTOR GetMinDimensions() const { return m_MinDimensions; }

    virtual void Translate(_In_ const Vec3 &translationVector) { D3D11Transformable::Translate(translationVector); }
    virtual void Rotate(float row, float yaw, float pitch) { D3D11Transformable::Rotate(row, yaw, pitch); }

private:
    bool m_UsesIndexBuffer;
    unsigned int m_VertexCount;
    unsigned int m_IndexCount;
    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_pIndexBuffer;
    D3D11Material *m_pMaterial;

    DirectX::XMVECTOR m_MaxDimensions;
    DirectX::XMVECTOR m_MinDimensions;
};

class D3D11Light : public Light
{
public:
    D3D11Light(CreateLightDescriptor::LightType Type) :
        m_Type(Type) {}

    CreateLightDescriptor::LightType GetLightType() const { return m_Type; }
    virtual void UpdateLight(ID3D11DeviceContext *pContext, const DirectX::XMMATRIX &viewTransformation) = 0;
private:
    CreateLightDescriptor::LightType m_Type;
};

class D3D11Scene; // Forward Declaration
class D3D11Camera; // Forward Declaration

class D3D11DirectionalLight : public D3D11Light
{
public:
    D3D11DirectionalLight(_In_ ID3D11Device *pDevice, _In_ CreateLightDescriptor *pCreateDirectionalLight);

    ID3D11Buffer *GetDirectionalLightConstantBuffer() const { return m_pLightConstantBuffer; }
    
    // The position from where we render the shadow buffer depends on the scene
    ID3D11Buffer *GetViewProjBuffer(ID3D11DeviceContext *pContext, D3D11Scene *pScene, D3D11Camera *pCamera);
    virtual void UpdateLight(ID3D11DeviceContext *pContext, const DirectX::XMMATRIX &viewTransformation);
private:
    DirectX::XMVECTOR m_lightDirection;
    CBDirectionalLight m_CpuLightData;
    CBViewProjectionTransforms m_ViewProjCpuData;

    ID3D11Buffer* m_pLightConstantBuffer;
    ID3D11Buffer *m_pViewProjBuffer;
};

class D3D11Camera : public Camera
{
public:
    D3D11Camera(ID3D11Device *pDevice, ID3D11DeviceContext *pContext, CreateCameraDescriptor *pCreateCameraDescriptor);
    ~D3D11Camera();

    virtual void Translate(_In_ const Vec3 &translationVector);
    virtual void Rotate(float row, float yaw, float pitch);

    ID3D11Buffer *GetCameraConstantBuffer();
    ID3D11Buffer *GetViewProjBuffer();
    const DirectX::XMMATRIX &GetViewMatrix() { return m_ViewProjCpuData.m_View; }
    const DirectX::XMMATRIX &GetInvTransViewMatrix() { return m_ViewProjCpuData.m_InvTransView; }
    const DirectX::XMVECTOR &GetViewDirection() { return DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(m_LookAt, m_Position)); }
    const DirectX::XMVECTOR &GetUp() { return m_Up; }
    const DirectX::XMVECTOR &GetRight() { return DirectX::XMVector3Cross(GetViewDirection(), m_Up); }
    float GetAspectRatio() { return m_Width / m_Height; }
    float GetVerticalFieldOfView() { return m_VerticalFieldOfView; }
    float GetHorizontalFieldOfView() { return m_HorizontalFieldOfView; }

private:
    CBCamera m_CameraCpuData;
    CBViewProjectionTransforms m_ViewProjCpuData;

    ID3D11DeviceContext *m_pContext;

    ID3D11Buffer *m_pCameraBuffer;
    ID3D11Buffer *m_pViewProjBuffer;

    REAL m_Width;
    REAL m_Height;
    
    REAL m_VerticalFieldOfView;
    REAL m_HorizontalFieldOfView;

    bool m_ViewMatrixNeedsUpdate;

    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMVECTOR m_Position;
    DirectX::XMVECTOR m_LookAt;
    DirectX::XMVECTOR m_Up;
};

class D3D11EnvironmentMap : public EnvironmentMap
{
public:
    virtual void DrawEnvironmentMap(_In_ ID3D11DeviceContext *pImmediateContext, D3D11Camera *pCamera, ID3D11RenderTargetView *pRenderTarget) = 0;
    virtual ID3D11ShaderResourceView *GetEnvironmentMapSRV() = 0;
    virtual ID3D11ShaderResourceView *GetIrradianceMapSRV() = 0;
};

class D3D11EnvironmentTextureCube : public D3D11EnvironmentMap
{
public:
    D3D11EnvironmentTextureCube(_In_ ID3D11Device *pDevice, _In_ ID3D11DeviceContext *pImmediateContext, _In_ const CreateEnvironmentTextureCube *pCreateTextureCube);
    void DrawEnvironmentMap(_In_ ID3D11DeviceContext *pImmediateContext, D3D11Camera *pCamera, ID3D11RenderTargetView *pRenderTarget);
    ID3D11ShaderResourceView *GetEnvironmentMapSRV() { return m_pEnvironmentTextureCube; }
    ID3D11ShaderResourceView *GetIrradianceMapSRV() { return m_pIrradianceTextureCube; }
private:
    static ID3D11ShaderResourceView *CreateTextureCube(_In_ ID3D11Device *pDevice, _In_ ID3D11DeviceContext *pImmediateContext, _In_reads_(TEXTURES_PER_CUBE) char * const *textureNames);
    ID3D11ShaderResourceView *m_pEnvironmentTextureCube;
    ID3D11ShaderResourceView *m_pIrradianceTextureCube;

    struct CameraPlaneVertex
    {
        Vec3 m_Position;
        Vec3 m_ViewVector;
    };
    ID3D11Buffer *m_pCameraVertexBuffer;
    ID3D11PixelShader *m_pEnvironmentPixelShader;
    ID3D11VertexShader *m_pEnvironmentVertexShader;
    ID3D11InputLayout *m_pEnvironmentInputLayout;
    const UINT VerticesInCameraPlane = 6;
};

class D3D11EnvironmentColor : public D3D11EnvironmentMap
{
public:
    D3D11EnvironmentColor(_In_ ID3D11Device *pDevice, const CreateEnvironmentColor *pCreateEnvironmnetColor) {
        assert(false);
    }
    ID3D11ShaderResourceView *GetEnvironmentMapSRV() { return nullptr; }
    ID3D11ShaderResourceView *GetIrradianceMapSRV() { return nullptr; }

    void DrawEnvironmentMap(_In_ ID3D11DeviceContext *pImmediateContext, D3D11Camera *pCamera, ID3D11RenderTargetView *pRenderTarget) {}
};

class D3D11Scene : public Scene
{
public:
    D3D11Scene(D3D11EnvironmentMap *m_pEnvironmentMap);

    void AddGeometry(_In_ Geometry *pGeometry);
    void AddLight(_In_ Light *pLight);

    DirectX::XMVECTOR GetMaxDimensions() const { return m_MaxDimensions; }
    DirectX::XMVECTOR GetMinDimensions() const { return m_MinDimensions; }

    D3D11EnvironmentMap *GetEnvironmentMap() { return m_pEnvironmentMap; }
    friend class D3D11Renderer;
protected:
    std::vector<D3D11Geometry *> &GetGeometryList() { return m_GeometryList; }
    std::vector<D3D11DirectionalLight *> &GetDirectionalLightList() { return m_DirectionalLightList; }

private:
    D3D11EnvironmentMap *m_pEnvironmentMap;

    std::vector<D3D11Geometry *> m_GeometryList;
    std::vector<D3D11DirectionalLight *> m_DirectionalLightList;

    DirectX::XMVECTOR m_MaxDimensions;
    DirectX::XMVECTOR m_MinDimensions;
};

template <typename ShaderType>
class D3D11ShaderCache
{
protected:
    D3D11ShaderCache(const char *shaderFile, const char *entryName, const char *shaderModel) :
        m_pShaderFile(shaderFile), m_pEntryName(entryName), m_pShaderModel(shaderModel) {}

    virtual std::vector<D3D_SHADER_MACRO> GetMacrosFromShaderValue(UINT input) = 0;
    virtual ShaderType *CreateShader(ID3D10Blob *pBlob) = 0;

    typename ShaderType *GetShader(UINT shaderValue)
    {
        auto entry = m_shaderCache.find(shaderValue);
        if (shaderValue == m_shaderCache.end())
        {
            ID3D10Blob *pShaderBlob;
            CompileShaderHelper(m_pShaderFile, m_pEntryName, m_pShaderModel, &GetMacrosFromShaderValue(shaderValue)[0], &pShaderBlob);
            FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

            ShaderType *pShader = CreateShader(pBlob);
            FAIL_CHK(!pShader, "Failed to CreateVertexShader");
        }
        else
        {
            return entry->second;
        }
    }

    const char *m_pShaderFile, *m_pEntryName, *m_pShaderModel;
    std::unordered_map<UINT, typename ShaderType*> m_shaderCache;
};

class PixelShaderCache : public D3D11ShaderCache<ID3D11PixelShader>
{
protected:
    PixelShaderCache(ID3D11Device *pDevice, char *pFileName, char *pEntryName) :
        D3D11ShaderCache<ID3D11PixelShader>(pFileName, pEntryName, "ps_5_0"),
        m_pDevice(pDevice)
    {}

    ID3D11Device *m_pDevice;
    virtual std::vector<D3D_SHADER_MACRO> GetMacrosFromShaderValue(UINT input) = 0;
    ID3D11PixelShader *CreateShader(ID3D10Blob *pBlob)
    {
        ID3D11PixelShader *pShader;
        m_pDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &pShader);
        return pShader;
    }
};

class ReadWriteTexture
{
public:
    ReadWriteTexture(ID3D11Device *pDevice, UINT Width, UINT Height, DXGI_FORMAT Format);

    ID3D11ShaderResourceView *GetShaderResourceView() { return m_pShaderResourceView; }
    ID3D11RenderTargetView *GetRenderTargetView() { return m_pRenderTargetView; }
private:
    ID3D11ShaderResourceView *m_pShaderResourceView;
    ID3D11RenderTargetView *m_pRenderTargetView;
    ID3D11Texture2D *m_pResource;
};

class D3D11Renderer : public Renderer
{
public:
    D3D11Renderer(HWND WindowHandle, unsigned int width, unsigned int height);
    
    void SetCanvas(Canvas* pCanvas);

    Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
    void DestroyGeometry(_In_ Geometry *pGeometry);
    
    Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor);
    void DestroyLight(Light *pLight);

    Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor);
    void DestroyCamera(Camera *pCamera);

    Material *CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor);
    void DestroyMaterial(Material* pMaterial);

    EnvironmentMap *CreateEnvironmentMap(CreateEnvironmentMapDescriptor *pCreateEnvironmnetMapDescriptor);
    void DestroyEnviromentMap(EnvironmentMap *pEnvironmentMap);

    Scene *CreateScene(EnvironmentMap *pEnvironmentMap);
    void DestroyScene(Scene *pScene);

    void DrawScene(Camera *pCamera, Scene *pScene, const RenderSettings &RenderFlags);
    Geometry *GetGeometryAtPixel(Camera *pCamera, Scene *pScene, Vec2 PixelCoord) {
        assert(false);
        return nullptr;
    }


private:
    void SetDefaultState();
    void CompileShaders();
    void InitializeSwapchain(HWND WindowHandle, unsigned int width, unsigned int height);
    void PostProcess(ReadWriteTexture *pInput, ID3D11RenderTargetView *pOutput, const RenderSettings &RenderFlags);

    ID3D11Device *m_pDevice;
    ID3D11Device1 *m_pDevice1;
    ID3D11DeviceContext *m_pImmediateContext;

    D3D11Canvas *m_pCanvas;

    std::unique_ptr<ReadWriteTexture> m_pBasePassTexture;
    ID3D11RenderTargetView* m_pSwapchainRenderTargetView;
    ID3D11DepthStencilView* m_pDepthBuffer;

    ID3D11DepthStencilView* m_pShadowDepthBuffer;
    ID3D11ShaderResourceView *m_pShadowResourceView;

    ID3D11VertexShader* m_pForwardVertexShader;
    ID3D11PixelShader* m_pForwardMattePixelShader;
    ID3D11PixelShader* m_pForwardTexturedPixelShader;
    ID3D11PixelShader* m_pForwardTexturedBumpPixelShader;
    ID3D11InputLayout* m_pForwardInputLayout;

    ID3D11SamplerState *m_pSamplerState;

    ID3D11PixelShader* m_pGammaCorrectPS;
    ID3D11PixelShader* m_pPassThroughPS;
    ID3D11VertexShader *m_pPostProcessVS;
};

#define D3D11_RENDERER_CAST reinterpret_cast