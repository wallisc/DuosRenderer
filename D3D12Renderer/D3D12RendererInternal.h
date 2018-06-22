#pragma once
#include "Renderer.h"
#include <cassert> // TODO remove when everything is implemented
#include <atlbase.h>

class D3D12Renderer : public Renderer
{
public:
    D3D12Renderer(ID3D12Device *pDevice);

    virtual void SetCanvas(Canvas *pCanvas);

    virtual void DrawScene(Camera *pCamera, Scene *pScene, const RenderSettings &RenderFlags){ assert(false); }
    virtual Geometry *GetGeometryAtPixel(Camera *pCamera, Scene *pScene, Vec2 PixelCoord){ assert(false); }

    virtual Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor){ assert(false); }
    virtual void DestroyGeometry(_In_ Geometry *pGeometry){ assert(false); }

    virtual Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor){ assert(false); }
    virtual void DestroyLight(Light *pLight){ assert(false); }

    virtual Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor){ assert(false); }
    virtual void DestroyCamera(Camera *pCamera){ assert(false); }

    virtual Material *CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor){ assert(false); }
    virtual void DestroyMaterial(Material* pMaterial){ assert(false); }

    virtual EnvironmentMap *CreateEnvironmentMap(CreateEnvironmentMapDescriptor *pCreateEnvironmnetMapDescriptor){ assert(false); }
    virtual void DestroyEnviromentMap(EnvironmentMap *pEnvironmentMap){ assert(false); }

    virtual Scene *CreateScene(EnvironmentMap *pEnvironmentMap){ assert(false); }
    virtual void DestroyScene(Scene *pScene){ assert(false); }
private:
    CComPtr<ID3D12Device> m_pDevice;
};