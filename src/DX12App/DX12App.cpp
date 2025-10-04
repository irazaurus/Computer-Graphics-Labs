#define NOMINMAX

#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "../Common/Camera.h"
#include "FrameResource.h"
#include "ShadowMap.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

// #define DEBUG_VIEW
// #define DEBUG

const int gNumFrameResources = 3;

enum class RenderLayer : int
{
	Opaque = 0,
	Debug,
	Sky,
	Count
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
	BoundingBox Bounds;
	std::string geoName;
	int layer;
	std::vector<std::string> LODGeoNames;
	int currentLOD = 0;
};

struct LightObject
{
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;                          // point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.57735f, -0.57735f, 0.57735f };// directional/spot light only
	float FalloffEnd = 50.0f;                           // point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
	float SpotPower = 64.0f;                            // spot light only
	DirectX::XMFLOAT3 Color = { 1.f, 1.f, 1.f };        // rgb
	LightType LightType = LightType::Directional;
	int lightCBIndex = 0;
	int NumFramesDirty = gNumFrameResources;
	std::string GeoName;
	ShadowMap* shadowMap;
};

class DX12App : public D3DApp
{
public:
	DX12App(HINSTANCE hInstance);
	DX12App(const DX12App& rhs) = delete;
	DX12App& operator=(const DX12App& rhs) = delete;
	~DX12App();

	virtual bool Initialize()override;

private:
	virtual void CreateRtvAndDsvDescriptorHeaps()override;
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateLightCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdatePostProcessCB(const GameTimer& gt);

	void LoadTexture(std::string name, std::wstring filename, TextureType type = TextureType::TEXTURE2D);
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItem(std::string name, std::string material, XMMATRIX translate, std::vector<std::string>* LODGeoNames, int layer = (int)RenderLayer::Opaque, float scale = 1.f, float scaleTex = 1.f);
	void BuildRenderItems();
	void BuildLightObjects();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawDeferredGeometry();
	void DrawDeferredLights();
	void DrawSkyBox();
	void DrawPostProcess();
	void DrawShadowMaps();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> mRootSignature;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	int ObjCBIndex = 0;
	std::vector<std::unique_ptr<LightObject>> mAllLights;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<RenderItem*> mVisibleRitems[(int)RenderLayer::Count];

	PassConstants mMainPassCB;

	Camera mCamera;
	POINT mLastMousePos;

	UINT mShadowMapHeapIndex = 0;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, int nCmdShow)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		DX12App theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

DX12App::DX12App(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

DX12App::~DX12App()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool DX12App::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCamera.SetPosition(-30.0f, 70.0f, -20.0f);
	mCamera.Pitch(-5.3f);
	mCamera.RotateY(0.7f);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildLightObjects();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void DX12App::CreateRtvAndDsvDescriptorHeaps()
{
	// Add +6 RTV for cube render target.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +199 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 200;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void DX12App::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	mGBuffer->Resize(mClientWidth, mClientHeight, md3dDevice.Get());

	// copy gbuffer resources into the srv heap
	if (mSrvDescriptorHeap != nullptr)
	{
		auto srvGBuffer = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		srvGBuffer.Offset(mGBuffer->Channel0SRVHeapIndex, mCbvSrvUavDescriptorSize);
		md3dDevice->CopyDescriptorsSimple(mGBuffer->NumBuffers, srvGBuffer,
			mGBuffer->m_SRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

void DX12App::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateLightCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdatePostProcessCB(gt);
}

void DX12App::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	auto passCB = mCurrFrameResource->PassCB->Resource();
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));
	mCommandList->SetGraphicsRootSignature(mRootSignature["default"].Get());

	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	DrawShadowMaps();

	mGBuffer->TransitToOpaqueRenderingState(mCommandList);
	mGBuffer->ClearRTVs(mCommandList);

	DrawDeferredGeometry();

	mGBuffer->TransitToLightsRenderingState(mCommandList);
	DrawDeferredLights();
	DrawSkyBox();

	mGBuffer->TransitToTonemappingState(mCommandList);
	DrawPostProcess();

	mGBuffer->TransitFromShaderResourceToCommon(mCommandList);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void DX12App::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void DX12App::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void DX12App::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void DX12App::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(30.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-30.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-30.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(30.0f * dt);

	mCamera.UpdateViewMatrix();
}

void DX12App::AnimateMaterials(const GameTimer& gt)
{

}

void DX12App::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		XMMATRIX world = XMLoadFloat4x4(&e->World);
		
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
			e->Geo->DrawArgs[e->geoName].Bounds.Transform(e->Bounds, XMLoadFloat4x4(&e->World));

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}

		if (mCamera.Bounds.Intersects(e->Bounds))
		{
			mVisibleRitems[e->layer].push_back(e.get());

			XMVECTOR worldPos, temp;
			XMMatrixDecompose(&temp, &temp, &worldPos, world);
			float camToObjDistance;
			XMStoreFloat(&camToObjDistance, XMVector3Length(XMVectorSubtract(mCamera.GetPosition(), worldPos)));

			if (camToObjDistance > 150.f)
				e->currentLOD = 1;
			else e->currentLOD = 0;
		}
	}
}

void DX12App::UpdateLightCBs(const GameTimer& gt)
{
	auto currLightCB = mCurrFrameResource->LightCB.get();
	for (auto& e : mAllLights)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			LightConstants lightConstants;
			lightConstants.Strength = e->Strength;
			lightConstants.FalloffStart = e->FalloffStart;
			lightConstants.Direction = e->Direction;
			lightConstants.FalloffEnd = e->FalloffEnd;
			lightConstants.Position = e->Position;
			lightConstants.SpotPower = e->SpotPower;
			lightConstants.Color = e->Color;
			lightConstants.LightType = (int)e->LightType;

			XMVECTOR lightDir, lightPos, targetPos;
			XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			XMMATRIX lightView, lightProj;
			// to transform NDC space [-1,+1]^2 to texture space [0,1]^2
			XMMATRIX T(
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, -0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.5f, 0.5f, 0.0f, 1.0f);

			switch (e->LightType)
			{
			case LightType::Directional:
			{
				float SphereRadiuses[4] = { 100, 150, 200, 500 };

				//for each cascade
				for (int i = 0; i < 4; i++)
				{
					// Only the first "main" light casts a shadow. Why? Idk, you tell me.
					lightDir = XMLoadFloat3(&lightConstants.Direction);
					lightPos = mCamera.GetPosition() - 2.0f * SphereRadiuses[i] * lightDir;
					targetPos = mCamera.GetPosition();
					lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

					// Transform bounding sphere to light space.
					XMFLOAT3 sphereCenterLS;
					XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

					// Ortho frustum in light space encloses scene.
					float l = sphereCenterLS.x - SphereRadiuses[i];
					float b = sphereCenterLS.y - SphereRadiuses[i];
					float n = sphereCenterLS.z - SphereRadiuses[i];
					float r = sphereCenterLS.x + SphereRadiuses[i];
					float t = sphereCenterLS.y + SphereRadiuses[i];
					float f = sphereCenterLS.z + SphereRadiuses[i];

					lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

					XMMATRIX S = lightView * lightProj;
					XMMATRIX S1 = S * T;
					XMStoreFloat4x4(&lightConstants.ViewProj[i], XMMatrixTranspose(S));
					XMStoreFloat4x4(&lightConstants.ShadowTransform[i], XMMatrixTranspose(S1));
				}
				break;
			}
			case LightType::Spotlight:
			{
				XMFLOAT3 ConeScale;
				ConeScale.y = e->FalloffEnd;
				ConeScale.x = 20 / ConeScale.y;
				ConeScale.x = ConeScale.z = ConeScale.x * e->SpotPower * 8;
				//calculate rotation matrix from start and target direction vectors
				XMVECTOR StartDir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
				XMVECTOR TargetDir = XMVector3Normalize(XMLoadFloat3(&e->Direction));
				XMVECTOR RotationAxis = XMVector3Cross(StartDir, TargetDir);
				float RotAngle = acosf(XMVectorGetX(XMVector3Dot(StartDir, TargetDir)));

				XMStoreFloat4x4(&lightConstants.World, XMMatrixTranspose(
					XMMatrixScaling(ConeScale.x, ConeScale.y, ConeScale.z) *
					XMMatrixRotationAxis(XMVector3Normalize(RotationAxis), RotAngle) *
					XMMatrixTranslation(e->Position.x, e->Position.y, e->Position.z)));

				lightPos = XMLoadFloat3(&e->Position);
				lightDir = XMLoadFloat3(&e->Direction);
				lightPos = lightPos - 20 * lightDir;
				targetPos = lightPos + lightDir;
				lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);
				lightProj = XMMatrixPerspectiveFovLH(XM_PI / 2.5f, 1.0f, 10.f, e->FalloffEnd * 10);

				XMMATRIX S = lightView * lightProj;
				XMMATRIX S1 = S * T;

				XMStoreFloat4x4(&lightConstants.ViewProj[0], XMMatrixTranspose(S));
				XMStoreFloat4x4(&lightConstants.ShadowTransform[0], XMMatrixTranspose(S1));
				break;
			}
			case LightType::Pointlight:
			{
				XMStoreFloat4x4(&lightConstants.World,
					XMMatrixTranspose(XMMatrixScaling(e->FalloffEnd * e->Strength.x, e->FalloffEnd * e->Strength.y, e->FalloffEnd * e->Strength.z)
					* XMMatrixTranslation(e->Position.x, e->Position.y, e->Position.z)));

				lightPos = XMLoadFloat3(&e->Position);

				lightProj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, e->FalloffEnd);

				static const XMVECTOR directions[6] =
				{
				 XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),  // +X
				 XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f), // -X
				 XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  // +Y
				 XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), // -Y
				 XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),  // +Z
				 XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)  // -Z
				};

				static const XMVECTOR ups[6] =
				{
				 XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  // +X
				 XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  // -X
				 XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), // +Y
				 XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),  // -Y
				 XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  // +Z
				 XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // -Z
				};

				for (int i = 0; i < 6; ++i)
				{
					targetPos = lightPos + directions[i];
					lightView = XMMatrixLookAtLH(lightPos, targetPos, ups[i]);

					XMMATRIX S = lightView * lightProj;
					XMMATRIX S1 = S * T;
					XMStoreFloat4x4(&lightConstants.ViewProj[i], XMMatrixTranspose(S));
					XMStoreFloat4x4(&lightConstants.ShadowTransform[i], XMMatrixTranspose(S1));
				}
				break;
			}
			}

			currLightCB->CopyData(e->lightCBIndex, lightConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void DX12App::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));
			matConstants.Metallic = mat->Metallic;

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void DX12App::UpdatePostProcessCB(const GameTimer& gt)
{
	auto currPostProcessCB = mCurrFrameResource->PostProcessCB.get();
	PostProcessSettings postProcessSettings;

	postProcessSettings.FocusDistance = 0.95f;
	postProcessSettings.FocusRange = 0.1f;
	postProcessSettings.NearBlurStrength = 5.0f;
	postProcessSettings.FarBlurStrength = 5.0f;
	postProcessSettings.ChromaticDirection = XMFLOAT2(-1.0f, -1.0f);
	postProcessSettings.ChromaticIntensity = 2.0f;
	postProcessSettings.ChromaticDistanceScale = 1.5f;
	postProcessSettings.EffectIntensity = 0.0f;
	postProcessSettings.EffectType = 0;

	currPostProcessCB->CopyData(0, postProcessSettings);
}

void DX12App::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void DX12App::LoadTexture(std::string name, std::wstring filename, TextureType type)
{
	auto tex = std::make_unique<Texture>();
	tex->Filename = filename;
	tex->Type = type;
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tex->Filename.c_str(),
		tex->Resource, tex->UploadHeap));
	mTextures[name] = std::move(tex);
}

void DX12App::LoadTextures()
{
	// Defaults
	LoadTexture("black", L"../Textures/black.dds");			 // always in 0 slot
	LoadTexture("diffuse", L"../Textures/white1x1.dds");

	// Bricks
	LoadTexture("bricks_diffuse", L"../Textures/grass.dds");
	LoadTexture("bricks_norm", L"../Textures/tile_nmap.dds");
	LoadTexture("bricks_disp", L"../Textures/checkboard.dds");

	// Baronyx
	LoadTexture("baryonyx_diffuse", L"../Textures/baryonyx_diffuse.dds");

	// trex
	LoadTexture("trex_diffuse", L"../Textures/trex_diffuse.dds");
	LoadTexture("trex_nmap", L"../Textures/trex_nmap.dds");

	// Last textures for sky
	LoadTexture("skyBrdf", L"../Textures/skyBrdf.dds");
	LoadTexture("skyDiffuseCube", L"../Textures/skyDiffuseCube.dds", TextureType::CUBEMAP);
	LoadTexture("skyIrradianceCube", L"../Textures/skyIrradianceCube.dds", TextureType::CUBEMAP);
}

void DX12App::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTables[10];
	for (int i = 0; i < 10; i++) {
		texTables[i].Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			1,  // number of descriptors
			i); // register ti
	}

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[20];

	// Perfomance TIP: Order from most frequent to least frequent.
	for (int i = 0; i < 10; i++) {
		slotRootParameter[i].InitAsDescriptorTable(1, &texTables[i], D3D12_SHADER_VISIBILITY_ALL); // 0-9   = textures
		slotRootParameter[i + 10].InitAsConstantBufferView(i);									   // 10-19 = CBs
	}

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(20, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature["default"].GetAddressOf())));
}

void DX12App::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 200;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	int i = 0;
	mTextures["black"]->SrvHeapIndex = i++;
	auto& tex = mTextures["black"]->Resource;
	srvDesc.Format = tex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	// texture descriptors except default "black"
	for (auto &Tex : mTextures)
	{
		if (Tex.first == "black") continue;

		Tex.second->SrvHeapIndex = i++;
		auto& tex = Tex.second->Resource;
		srvDesc.Format = tex->GetDesc().Format;

		switch (Tex.second->Type)
		{
		case TextureType::TEXTURE2D:
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
			break;
			
		case TextureType::CUBEMAP:
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			srvDesc.TextureCube.MipLevels = tex->GetDesc().MipLevels;
			break;
		}

		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}

	mGBuffer->Channel0SRVHeapIndex = (UINT)mTextures.size();
	mShadowMapHeapIndex = mGBuffer->Channel0SRVHeapIndex + mGBuffer->NumBuffers + 1;

	// copy gbuffer resources into the srv heap
	auto srvGBuffer = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srvGBuffer.Offset(mGBuffer->Channel0SRVHeapIndex, mCbvSrvUavDescriptorSize);
	md3dDevice->CopyDescriptorsSimple(mGBuffer->NumBuffers, srvGBuffer,
		mGBuffer->m_SRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DX12App::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["deferredVS"] = d3dUtil::CompileShader(L"Shaders\\DeferredGeometry.hlsl", nullptr, "VSWithoutTess", "vs_5_0");
	mShaders["tessVS"] = d3dUtil::CompileShader(L"Shaders\\DeferredGeometry.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["tessHS"] = d3dUtil::CompileShader(L"Shaders\\DeferredGeometry.hlsl", nullptr, "HS", "hs_5_0");
	mShaders["tessDS"] = d3dUtil::CompileShader(L"Shaders\\DeferredGeometry.hlsl", nullptr, "DS", "ds_5_0");
	mShaders["deferredPS"] = d3dUtil::CompileShader(L"Shaders\\DeferredGeometry.hlsl", nullptr, "DeferredPS", "ps_5_0");
	
	mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowGS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["deferredLightsVS"] = d3dUtil::CompileShader(L"Shaders\\DeferredLights.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["deferredLightsPS"] = d3dUtil::CompileShader(L"Shaders\\DeferredLights.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["deferredLightsGeometryVS"] = d3dUtil::CompileShader(L"Shaders\\DeferredLights.hlsl", nullptr, "LightsGeometryVS", "vs_5_1");
	mShaders["deferredAmbientPS"] = d3dUtil::CompileShader(L"Shaders\\DeferredLights.hlsl", nullptr, "AmbientPS", "ps_5_1");
	
	mShaders["postVS"] = d3dUtil::CompileShader(L"Shaders\\PostProcessing.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["postPS"] = d3dUtil::CompileShader(L"Shaders\\PostProcessing.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void DX12App::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	std::vector<GeometryGenerator::MeshData> allMeshData;

	// if you want to generate new model -- generate it here
	allMeshData.push_back( geoGen.CreateGrid(50.0f, 50.0f, 50, 50, 1.0f) );           // grid
	allMeshData.push_back( geoGen.CreateBox(10.0f, 10.0f, 10.0f, 3) );                // box
	allMeshData.push_back( geoGen.LoadModel("..\\Models\\trex.obj"));             // trex
	allMeshData.push_back( geoGen.LoadModel("..\\Models\\Baryonyx.obj"));         // baryonyx
	allMeshData.push_back( geoGen.CreateCone(1.f, 3.f, 20, 20) );					  // cone for spot
	allMeshData.push_back( geoGen.CreateSphere(1.f, 20, 20) );					      // sphere for point

	// 
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex and index buffer.
	std::vector<UINT> vertexOffsets;
	vertexOffsets.push_back(0);
	std::vector<UINT> indexOffsets;
	indexOffsets.push_back(0);
	for (size_t i = 1; i < allMeshData.size(); i++)
	{
		vertexOffsets.push_back(vertexOffsets.at(i - 1) + (UINT) allMeshData.at(i - 1).Vertices.size());
		indexOffsets.push_back(indexOffsets.at(i - 1) + (UINT) allMeshData.at(i - 1).Indices32.size());
	}
	
	// generating submeshes
	size_t totalVertexCount = 0;
	std::vector<SubmeshGeometry> allSubmeshes;
	for (size_t i = 0; i < allMeshData.size(); i++)
	{
		SubmeshGeometry submesh;
		auto& mesh = allMeshData.at(i);
		submesh.IndexCount = (UINT)mesh.Indices32.size();
		submesh.StartIndexLocation = indexOffsets.at(i);
		submesh.BaseVertexLocation = vertexOffsets.at(i);

		XMFLOAT3 vMin = { FLT_MAX, FLT_MAX, FLT_MAX };
		XMFLOAT3 vMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		for (size_t j = 0; j < mesh.Vertices.size(); ++j)
		{
			auto& vertex = mesh.Vertices[j];
			vMin.x = std::min(vMin.x, vertex.Position.x);
			vMin.y = std::min(vMin.y, vertex.Position.y);
			vMin.z = std::min(vMin.z, vertex.Position.z);

			vMax.x = std::max(vMax.x, vertex.Position.x);
			vMax.y = std::max(vMax.y, vertex.Position.y);
			vMax.z = std::max(vMax.z, vertex.Position.z);
		}

		// generating bounding box
		XMFLOAT3 center = {
		  0.5f * (vMin.x + vMax.x),
		  0.5f * (vMin.y + vMax.y),
		  0.5f * (vMin.z + vMax.z)
		};
		XMFLOAT3 extents = {
		  0.5f * (vMax.x - vMin.x),
		  0.5f * (vMax.y - vMin.y),
		  0.5f * (vMax.z - vMin.z)
		};

		BoundingBox box(center, extents);
		submesh.Bounds = box;

		allSubmeshes.push_back(submesh);
		totalVertexCount += mesh.Vertices.size();
	}

	// pack the vertices of all the meshes into one vertex buffer
	std::vector<Vertex> vertices(totalVertexCount);
	UINT k = 0;
	for (GeometryGenerator::MeshData mesh : allMeshData) {
		for (size_t i = 0; i < mesh.Vertices.size(); ++i, ++k)
		{
			vertices[k].Tangent = mesh.Vertices[i].TangentU;
			vertices[k].Pos = mesh.Vertices[i].Position;
			vertices[k].Normal = mesh.Vertices[i].Normal;
			vertices[k].TexC = mesh.Vertices[i].TexC;
		}
	}

	std::vector<std::uint16_t> indices;
	for (GeometryGenerator::MeshData mesh : allMeshData)
		indices.insert(indices.end(), std::begin(mesh.GetIndices16()), std::end(mesh.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	for (size_t i = 0; i < allMeshData.size(); i++)
	{
		geo->DrawArgs[allMeshData.at(i).name] = allSubmeshes.at(i);
	}

	mGeometries[geo->Name] = std::move(geo);
}

void DX12App::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature["default"].Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["deferredVS"]->GetBufferPointer()),
		mShaders["deferredVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		nullptr,
		0
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
#ifdef DEBUG_VIEW
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
#else
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
#endif // DEBUG_VIEW
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;

	//
	// PSO for shadow map pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc;
	ZeroMemory(&smapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	smapPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	smapPsoDesc.pRootSignature = mRootSignature["default"].Get();
	smapPsoDesc.VS =
	{
	  reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
	  mShaders["shadowVS"]->GetBufferSize()
	};
	smapPsoDesc.GS =
	{
	  reinterpret_cast<BYTE*>(mShaders["shadowGS"]->GetBufferPointer()),
	  mShaders["shadowGS"]->GetBufferSize()
	};
	smapPsoDesc.PS = { nullptr, 0 };

	smapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	smapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	smapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	smapPsoDesc.SampleMask = UINT_MAX;
	smapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	smapPsoDesc.SampleDesc.Count = 1;
	smapPsoDesc.SampleDesc.Quality = 0;
	smapPsoDesc.DSVFormat = mDepthStencilFormat;
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	smapPsoDesc.RasterizerState.DepthBias = 1000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

	//
	// PSO for sky.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature["default"].Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.DS = { nullptr, 0 };
	skyPsoDesc.HS = { nullptr, 0 };
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	//
	//	PSO for deferred geometry pass
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredGeometryPsoDesc = opaquePsoDesc;
	deferredGeometryPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["deferredPS"]->GetBufferPointer()),
		mShaders["deferredPS"]->GetBufferSize()
	};
#ifdef DEBUG_VIEW
	deferredGeometryPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
#else
	deferredGeometryPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
#endif // DEBUG_VIEW
	deferredGeometryPsoDesc.NumRenderTargets = 5;
	deferredGeometryPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;		   // diffuse
	deferredGeometryPsoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;    // zwzanashih
	deferredGeometryPsoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_SNORM;	   // normal
	deferredGeometryPsoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;        // diffuse albedo
	deferredGeometryPsoDesc.RTVFormats[4] = DXGI_FORMAT_R8G8B8A8_UNORM;        // fresnel & roughness
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&deferredGeometryPsoDesc, IID_PPV_ARGS(&mPSOs["deferredGeometry"])));

	deferredGeometryPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["tessVS"]->GetBufferPointer()),
		mShaders["tessVS"]->GetBufferSize()
	};
	deferredGeometryPsoDesc.HS =
	{
		reinterpret_cast<BYTE*>(mShaders["tessHS"]->GetBufferPointer()),
		mShaders["tessHS"]->GetBufferSize()
	};
	deferredGeometryPsoDesc.DS =
	{
		reinterpret_cast<BYTE*>(mShaders["tessDS"]->GetBufferPointer()),
		mShaders["tessDS"]->GetBufferSize()
	};
	deferredGeometryPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&deferredGeometryPsoDesc, IID_PPV_ARGS(&mPSOs["tessGeometry"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredPsoDesc = {};
	deferredPsoDesc.InputLayout = { nullptr, 0 };
	deferredPsoDesc.pRootSignature = mRootSignature["default"].Get();
	deferredPsoDesc.VS =
	{
	 reinterpret_cast<BYTE*>(mShaders["deferredLightsVS"]->GetBufferPointer()),
	 mShaders["deferredLightsVS"]->GetBufferSize()
	};
	deferredPsoDesc.PS =
	{
	 reinterpret_cast<BYTE*>(mShaders["deferredLightsPS"]->GetBufferPointer()),
	 mShaders["deferredLightsPS"]->GetBufferSize()
	};
	deferredPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].LogicOpEnable = false;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	deferredPsoDesc.BlendState = blendDesc;

	deferredPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	deferredPsoDesc.DepthStencilState.DepthEnable = false;
	deferredPsoDesc.DepthStencilState.StencilEnable = false;
	deferredPsoDesc.SampleMask = UINT_MAX;
	deferredPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	deferredPsoDesc.NumRenderTargets = 1;
	deferredPsoDesc.RTVFormats[0] = mBackBufferFormat;
	deferredPsoDesc.SampleDesc.Count = 1;
	deferredPsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&deferredPsoDesc, IID_PPV_ARGS(&mPSOs["deferredLights"])));

	//
	// PSO for ambient
	//
	deferredPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["deferredAmbientPS"]->GetBufferPointer()),
		mShaders["deferredAmbientPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&deferredPsoDesc, IID_PPV_ARGS(&mPSOs["deferredAmbient"])));

	//
	// PSO for using geometry for lights
	//
	deferredPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	deferredPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	//deferredPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // for debug
	deferredPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
	deferredPsoDesc.DepthStencilState.DepthEnable = true;
	deferredPsoDesc.DepthStencilState.StencilEnable = true;
	deferredPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	deferredPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["deferredLightsGeometryVS"]->GetBufferPointer()),
		mShaders["deferredLightsGeometryVS"]->GetBufferSize()
	};
	deferredPsoDesc.PS =
	{
	 reinterpret_cast<BYTE*>(mShaders["deferredLightsPS"]->GetBufferPointer()),
	 mShaders["deferredLightsPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&deferredPsoDesc, IID_PPV_ARGS(&mPSOs["deferredLightsGeometry"])));

	//
	// PSO for post process
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.pRootSignature = mRootSignature["default"].Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["postVS"]->GetBufferPointer()),
		mShaders["postVS"]->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["postPS"]->GetBufferPointer()),
		mShaders["postPS"]->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.StencilEnable = false;
	psoDesc.DSVFormat = mDepthStencilFormat;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["PostProcessPSO"])));
}

void DX12App::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			2, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), (UINT)mAllLights.size()));
	}
}

void DX12App::BuildMaterials()
{
	int matCBI = 0;

	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = matCBI++;
	bricks0->DiffuseSrvHeapIndex = mTextures["bricks_diffuse"]->SrvHeapIndex;
	//bricks0->NormalSrvHeapIndex = mTextures["bricks_norm"]->SrvHeapIndex;
	//bricks0->DisplaceSrvHeapIndex = mTextures["bricks_disp"]->SrvHeapIndex;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	mMaterials[bricks0->Name] = std::move(bricks0);

	auto gorg = std::make_unique<Material>();
	gorg->Name = "gorg";
	gorg->MatCBIndex = matCBI++;
	gorg->DiffuseSrvHeapIndex = mTextures["baryonyx_diffuse"]->SrvHeapIndex;
	gorg->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	gorg->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	gorg->Roughness = 0.3f;

	mMaterials[gorg->Name] = std::move(gorg);

	auto trex = std::make_unique<Material>();
	trex->Name = "trex";
	trex->MatCBIndex = matCBI++;
	trex->DiffuseSrvHeapIndex = mTextures["trex_diffuse"]->SrvHeapIndex;
	trex->NormalSrvHeapIndex = mTextures["trex_nmap"]->SrvHeapIndex;
	trex->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	trex->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	trex->Roughness = 1.0f;

	mMaterials[trex->Name] = std::move(trex);

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MatCBIndex = matCBI++;
	sky->DiffuseSrvHeapIndex = mTextures["skyDiffuseCube"]->SrvHeapIndex;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials[sky->Name] = std::move(sky);

	for (int i = 0; i < 11; i++)
	{
		for (int j = 0; j < 11; j++)
		{
			auto sphere = std::make_unique<Material>();
			sphere->Name = "sphere_" + std::to_string(i) + "_" + std::to_string(j);
			sphere->MatCBIndex = matCBI++;
			sphere->DiffuseSrvHeapIndex = mTextures["diffuse"]->SrvHeapIndex;
			sphere->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			sphere->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
			sphere->Roughness = j / 10.f;
			sphere->Metallic = i / 10.f;

			mMaterials[sphere->Name] = std::move(sphere);
		}
	}
}

void DX12App::BuildRenderItem(std::string name, std::string material, XMMATRIX translate, std::vector<std::string>* LODGeoNames, int layer, float scale, float scaleTex)
{
	auto ptr = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&ptr->World, XMMatrixScaling(scale, scale, scale) * translate);
	XMStoreFloat4x4(&ptr->TexTransform, XMMatrixScaling(scaleTex, scaleTex, scaleTex));
	ptr->ObjCBIndex = ObjCBIndex++;
	ptr->Mat = mMaterials[material].get();
	ptr->Geo = mGeometries["shapeGeo"].get();
	ptr->geoName = name;
	if (layer == 0)
		ptr->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	else
		ptr->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ptr->IndexCount = ptr->Geo->DrawArgs[name].IndexCount;
	ptr->Geo->DrawArgs[name].Bounds.Transform(ptr->Bounds, XMLoadFloat4x4(&ptr->World));
	ptr->StartIndexLocation = ptr->Geo->DrawArgs[name].StartIndexLocation;
	ptr->BaseVertexLocation = ptr->Geo->DrawArgs[name].BaseVertexLocation;
	ptr->layer = layer;
	if (LODGeoNames != nullptr)
		ptr->LODGeoNames = *LODGeoNames;

	mRitemLayer[layer].push_back(ptr.get());
	mAllRitems.push_back(std::move(ptr));
}

void DX12App::BuildRenderItems()
{
	BuildRenderItem("box", "sky", XMMatrixIdentity(), nullptr, (int) RenderLayer::Sky, 5000.0f);
	BuildRenderItem("quad", "bricks0", XMMatrixIdentity(), nullptr, (int)RenderLayer::Debug);

	BuildRenderItem("box", "bricks0", XMMatrixTranslation(15.f, 0.f, 0.f), nullptr);
	BuildRenderItem("box", "bricks0", XMMatrixScaling(50.f, 0.1f, 50.f) * XMMatrixTranslation(0.f, -5.f, 10.f), nullptr, 0, 1.f, 10.f);
	BuildRenderItem("trex", "trex", XMMatrixTranslation(40.f, -5.f, -60.f), nullptr, 0, 2.f);

	std::vector<std::string> BaryonyxLODs = {"Baryonyx", "box"};
	BuildRenderItem("Baryonyx", "gorg", XMMatrixTranslation(0.f, -5.f, 20.f), &BaryonyxLODs);
	BuildRenderItem("Baryonyx", "gorg", XMMatrixTranslation(-30.f, -5.f, 40.f), &BaryonyxLODs);
	BuildRenderItem("Baryonyx", "gorg", XMMatrixTranslation(30.f, -5.f, 0.f), &BaryonyxLODs);

	float spacing = 7.f;
	for (int i = 0; i < 11; i++)
	{
		for (int j = 0; j < 11; j++)
		{
			BuildRenderItem("sphere", "sphere_" + std::to_string(i) + "_" + std::to_string(j),
				XMMatrixTranslation(i * spacing - 80.f, 0.f, j * spacing - 80.f), nullptr, 0, 3);
		}
	}
}

void DX12App::BuildLightObjects()
{
	auto dir1 = std::make_unique<LightObject>();
	dir1->LightType = LightType::Directional;
	dir1->Strength = { 1.f, 1.f, 1.f };
	dir1->Direction = { 0.57735f, -0.57735f, 0.57735f };
	mAllLights.push_back(std::move(dir1));
	
	auto spot1 = std::make_unique<LightObject>();
	spot1->LightType = LightType::Spotlight;
	spot1->Color = { 1.f, 0.243f, 0.584f };  // pink
	spot1->Direction = { -0.57735f, -0.57735f, 0.57735f };
	spot1->Position = { 0.f, 50.f, -10.f };
	spot1->Strength = { 10.f, 10.f, 10.f };
	spot1->FalloffEnd = 150.f;
	spot1->SpotPower = 100.f;
	mAllLights.push_back(std::move(spot1));

	auto point1 = std::make_unique<LightObject>();
	point1->LightType = LightType::Pointlight;
	point1->Color = { 0.243f, 1.f, 0.91f }; // blue
	point1->Position = { -10.f, 1.f, 0.f };
	point1->Strength = { 2.f, 2.f, 2.f };
	mAllLights.push_back(std::move(point1));

	auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

	for (size_t i = 0; i < mAllLights.size(); i++)
	{
		switch (mAllLights.at(i)->LightType)
		{
		case LightType::Directional:
			break;

		case LightType::Pointlight:
			mAllLights.at(i)->GeoName = "sphere";
			break;

		case LightType::Spotlight:
			mAllLights.at(i)->GeoName = "cone";
			break;
		}

		mAllLights.at(i)->lightCBIndex = i;
		mAllLights.at(i)->shadowMap = new ShadowMap(md3dDevice.Get(), 2048, 2048);

		mAllLights.at(i)->shadowMap->BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapHeapIndex + i, mCbvSrvUavDescriptorSize),
			CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex + i, mCbvSrvUavDescriptorSize),
			CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1 + i, mDsvDescriptorSize));
	}
}

void DX12App::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for (const auto& ri : ritems)
	{
		Material* mat = ri->Mat;
		UINT textureIndex = mat->DiffuseSrvHeapIndex;
		UINT normalIndex = mat->NormalSrvHeapIndex;
		UINT displaceIndex = mat->DisplaceSrvHeapIndex;

		// register texture in t0
		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			textureIndex,  // Смещение в куче дескрипторов
			mCbvSrvDescriptorSize
		);
		cmdList->SetGraphicsRootDescriptorTable(0, texHandle);

		// register texture in t1
		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle1(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			normalIndex,  // Смещение в куче дескрипторов
			mCbvSrvDescriptorSize
		);
		cmdList->SetGraphicsRootDescriptorTable(1, texHandle1);

		// register texture in t2
		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle2(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			displaceIndex,  // Смещение в куче дескрипторов
			mCbvSrvDescriptorSize
		);
		cmdList->SetGraphicsRootDescriptorTable(2, texHandle2);

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());


		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(10, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(12, matCBAddress);

		if (ri->LODGeoNames.empty())
			cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
		else if (ri->currentLOD < ri->LODGeoNames.size())
		{
			auto &item = ri->Geo->DrawArgs[ri->LODGeoNames.at(ri->currentLOD)];
			cmdList->DrawIndexedInstanced(item.IndexCount, 1, item.StartIndexLocation, item.BaseVertexLocation, 0);
		}
		else
		{
			auto& item = ri->Geo->DrawArgs[ri->LODGeoNames.back()];
			cmdList->DrawIndexedInstanced(item.IndexCount, 1, item.StartIndexLocation, item.BaseVertexLocation, 0);
		}
	}
}

void DX12App::DrawDeferredGeometry()
{
	auto passCB = mCurrFrameResource->PassCB->Resource();
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);
	mCommandList->SetPipelineState(mPSOs["deferredGeometry"].Get());

	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[5] = {
		 mGBuffer->DiffuseRTV,
		 mGBuffer->ZWzanashihRTV,
		 mGBuffer->NormalRTV,
		 mGBuffer->MaterialAlbedoRTV,
		 mGBuffer->MaterialFresnelRoughnessRTV
	};
	mCommandList->OMSetRenderTargets(5, rtvs, false, &DepthStencilView());

	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	mCommandList->SetGraphicsRootConstantBufferView(11, passCB->GetGPUVirtualAddress());

	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DrawRenderItems(mCommandList.Get(), mVisibleRitems[(int)RenderLayer::Opaque]);
	
	for (int i = 0; i < mVisibleRitems->size(); i++)
	{
		mVisibleRitems[i].clear();
	}
}

void DX12App::DrawDeferredLights()
{
	auto passCB = mCurrFrameResource->PassCB->Resource();

	UINT lightCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(LightConstants));
	auto lightCB = mCurrFrameResource->LightCB->Resource();

	mCommandList->SetGraphicsRootConstantBufferView(10, passCB->GetGPUVirtualAddress());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);
	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &mGBuffer->BloomRTV, true, &DepthStencilView());


	for (int i = 0; i < 5; i++)
	{
		// register texture
		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			mGBuffer->Channel0SRVHeapIndex + i,
			mCbvSrvDescriptorSize
		);
		mCommandList->SetGraphicsRootDescriptorTable(i + 1, texHandle);
	}

	// sky diffuse
	mCommandList->SetGraphicsRootDescriptorTable(6, CD3DX12_GPU_DESCRIPTOR_HANDLE(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		mTextures["skyDiffuseCube"]->SrvHeapIndex,
		mCbvSrvDescriptorSize
	));
	// sky irradiance
	mCommandList->SetGraphicsRootDescriptorTable(7, CD3DX12_GPU_DESCRIPTOR_HANDLE(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		mTextures["skyIrradianceCube"]->SrvHeapIndex,
		mCbvSrvDescriptorSize
	));
	// sky brdf
	mCommandList->SetGraphicsRootDescriptorTable(8, CD3DX12_GPU_DESCRIPTOR_HANDLE(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		mTextures["skyBrdf"]->SrvHeapIndex,
		mCbvSrvDescriptorSize
	));



	for (auto& Light : mAllLights)
	{
		auto shadowMap = Light->shadowMap;
		mCommandList->SetGraphicsRootDescriptorTable(0, shadowMap->Srv());

		D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress = lightCB->GetGPUVirtualAddress() + Light->lightCBIndex * lightCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(11, lightCBAddress);

		if (Light->LightType == LightType::Directional)
		{
			mCommandList->SetPipelineState(mPSOs["deferredLights"].Get());
			mCommandList->DrawInstanced(6, 1, 0, 0);
		}
		else
		{
			mCommandList->SetPipelineState(mPSOs["deferredLightsGeometry"].Get());

			mCommandList->IASetVertexBuffers(0, 1, &mGeometries["shapeGeo"]->VertexBufferView());
			mCommandList->IASetIndexBuffer(&mGeometries["shapeGeo"]->IndexBufferView());

			mCommandList->DrawIndexedInstanced(mGeometries["shapeGeo"]->DrawArgs[Light->GeoName].IndexCount, 1,
				mGeometries["shapeGeo"]->DrawArgs[Light->GeoName].StartIndexLocation,
				mGeometries["shapeGeo"]->DrawArgs[Light->GeoName].BaseVertexLocation, 0);
		}
	}

	mCommandList->SetPipelineState(mPSOs["deferredAmbient"].Get());
	mCommandList->DrawInstanced(6, 1, 0, 0); // todo 3?
}

void DX12App::DrawSkyBox()
{
	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(11, passCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);
}

void DX12App::DrawPostProcess()
{
	// register input texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		mGBuffer->Channel0SRVHeapIndex + 6,
		mCbvSrvDescriptorSize
	);
	mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);

	// register depth texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle1(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		mGBuffer->Channel0SRVHeapIndex + 1, //zw
		mCbvSrvDescriptorSize
	);
	mCommandList->SetGraphicsRootDescriptorTable(1, texHandle1);

	// register normal texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle2(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		mGBuffer->Channel0SRVHeapIndex + 2, // normal
		mCbvSrvDescriptorSize
	);
	mCommandList->SetGraphicsRootDescriptorTable(2, texHandle2);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	auto postProcessCB = mCurrFrameResource->PostProcessCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(10,
		postProcessCB->GetGPUVirtualAddress()); // PostProcess Settings

	mCommandList->SetPipelineState(mPSOs["PostProcessPSO"].Get());
	mCommandList->DrawInstanced(3, 1, 0, 0);
}

void DX12App::DrawShadowMaps()
{
	auto passCB = mCurrFrameResource->PassCB->Resource();


	UINT lightCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(LightConstants));
	auto lightCB = mCurrFrameResource->LightCB->Resource();

	mCommandList->SetGraphicsRootConstantBufferView(11, passCB->GetGPUVirtualAddress());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());

	for (auto &Light : mAllLights)
	{
		auto shadowMap = Light->shadowMap;
		mCommandList->RSSetViewports(1, &shadowMap->Viewport());
		mCommandList->RSSetScissorRects(1, &shadowMap->ScissorRect());

		// Transition render target to dsv
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			shadowMap->Resource(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE));
		// Clear depth stencil
		mCommandList->ClearDepthStencilView(shadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		// Specify the buffers we are going to render to.
		mCommandList->OMSetRenderTargets(0, nullptr, true, &shadowMap->Dsv());


		D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress = lightCB->GetGPUVirtualAddress() + Light->lightCBIndex * lightCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(13, lightCBAddress);

		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

		// Transition dsv to rtv
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			shadowMap->Resource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_GENERIC_READ));
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> DX12App::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow};
}

