#include "Gbuffer.h"
#include "../Common/d3dx12.h"
#include <stdexcept>

Gbuffer::Gbuffer(int width, int height, Microsoft::WRL::ComPtr<ID3D12Device> device)
{

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = NumBuffers;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVDescriptorHeap))))
        throw std::runtime_error("Failed to create RTV Descriptor Heap");

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = NumBuffers;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_SRVDescriptorHeap))))
        throw std::runtime_error("Failed to create SRV Descriptor Heap");

    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_SRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    HRESULT hr = S_OK;

    D3D12_CLEAR_VALUE clearValue_UNORM = {};
    clearValue_UNORM.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue_UNORM.Color[0] = 0.0f;
    clearValue_UNORM.Color[1] = 0.0f;
    clearValue_UNORM.Color[2] = 0.0f;
    clearValue_UNORM.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue_SNORM = {};
    clearValue_SNORM.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
    clearValue_SNORM.Color[0] = 0.0f;
    clearValue_SNORM.Color[1] = 0.0f;
    clearValue_SNORM.Color[2] = 0.0f;
    clearValue_SNORM.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue_FLOAT = {};
    clearValue_FLOAT.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    clearValue_FLOAT.Color[0] = 0.0f;
    clearValue_FLOAT.Color[1] = 0.0f;
    clearValue_FLOAT.Color[2] = 0.0f;
    clearValue_FLOAT.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue2 = {};
    clearValue2.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    clearValue2.Color[0] = 0.0f;
    clearValue2.Color[1] = 0.0f;
    clearValue2.Color[2] = 0.0f;
    clearValue2.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue3 = {};
    clearValue3.Format = DXGI_FORMAT_R16G16B16A16_SNORM;
    clearValue3.Color[0] = 0.0f;
    clearValue3.Color[1] = 0.0f;
    clearValue3.Color[2] = 0.0f;
    clearValue3.Color[3] = 1.0f;

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&DiffuseTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DiffuseTex");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_FLOAT,
        IID_PPV_ARGS(&ZWzanashihTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create EmissiveTex");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_SNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue3,
        IID_PPV_ARGS(&NormalTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create NormalTex");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&MaterialAlbedoTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create MaterialAlbedoTex");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&MaterialFresnelRoughnessTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create MaterialFresnelRoughnessTex");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue2,
        IID_PPV_ARGS(&AccumulationBuf)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create AccumulationBuf");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&BloomTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create BloomTex");

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(DiffuseTex.Get(), &rtvDesc, rtvHandle);
    DiffuseRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    device->CreateRenderTargetView(ZWzanashihTex.Get(), &rtvDesc, rtvHandle);
    ZWzanashihRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_SNORM;
    device->CreateRenderTargetView(NormalTex.Get(), &rtvDesc, rtvHandle);
    NormalRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(MaterialAlbedoTex.Get(), &rtvDesc, rtvHandle);
    MaterialAlbedoRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    device->CreateRenderTargetView(MaterialFresnelRoughnessTex.Get(), &rtvDesc, rtvHandle);
    MaterialFresnelRoughnessRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    device->CreateRenderTargetView(AccumulationBuf.Get(), &rtvDesc, rtvHandle);
    AccumulationRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(BloomTex.Get(), &rtvDesc, rtvHandle);
    BloomRTV = rtvHandle;


    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(DiffuseTex.Get(), &srvDesc, srvHandle);
    DiffuseSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    device->CreateShaderResourceView(ZWzanashihTex.Get(), &srvDesc, srvHandle);
    ZWzanashihSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_SNORM;
    device->CreateShaderResourceView(NormalTex.Get(), &srvDesc, srvHandle);
    NormalSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(MaterialAlbedoTex.Get(), &srvDesc, srvHandle);
    MaterialAlbedoSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    device->CreateShaderResourceView(MaterialFresnelRoughnessTex.Get(), &srvDesc, srvHandle);
    MaterialFresnelRoughnessSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    device->CreateShaderResourceView(AccumulationBuf.Get(), &srvDesc, srvHandle);
    AccumulationSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(BloomTex.Get(), &srvDesc, srvHandle);
    BloomSRV = srvHandle;

    md3dDevice = device;
}

void Gbuffer::TransitToOpaqueRenderingState(ComPtr<ID3D12GraphicsCommandList>& cmdList)
{
    CD3DX12_RESOURCE_BARRIER barriers[7];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        DiffuseTex.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        //D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        ZWzanashihTex.Get(),
        //D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
        NormalTex.Get(),
        //D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialAlbedoTex.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialFresnelRoughnessTex.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(
        AccumulationBuf.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[6] = CD3DX12_RESOURCE_BARRIER::Transition(
        BloomTex.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(7, barriers);
}

void Gbuffer::TransitToLightsRenderingState(ComPtr<ID3D12GraphicsCommandList>& cmdList)
{
    CD3DX12_RESOURCE_BARRIER barriers[5];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        DiffuseTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        ZWzanashihTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
        NormalTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialAlbedoTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialFresnelRoughnessTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(5, barriers);
}

void Gbuffer::TransitToTonemappingState(ComPtr<ID3D12GraphicsCommandList>& cmdList)
{
    CD3DX12_RESOURCE_BARRIER barriers[2];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        AccumulationBuf.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        BloomTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(2, barriers);
}

void Gbuffer::TransitToCommon(ComPtr<ID3D12GraphicsCommandList>& cmdList)
{
    CD3DX12_RESOURCE_BARRIER barriers[7];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        DiffuseTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        ZWzanashihTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
        NormalTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialAlbedoTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialFresnelRoughnessTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(
        AccumulationBuf.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[6] = CD3DX12_RESOURCE_BARRIER::Transition(
        BloomTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    cmdList->ResourceBarrier(7, barriers);
}

void Gbuffer::TransitFromRenderTargetToCommon(ComPtr<ID3D12GraphicsCommandList>& cmdList)
{
    CD3DX12_RESOURCE_BARRIER barriers[5];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        DiffuseTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        ZWzanashihTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
        NormalTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialAlbedoTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialFresnelRoughnessTex.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    cmdList->ResourceBarrier(5, barriers);
}

void Gbuffer::TransitFromShaderResourceToCommon(ComPtr<ID3D12GraphicsCommandList>& cmdList)
{
    CD3DX12_RESOURCE_BARRIER barriers[7];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        DiffuseTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        ZWzanashihTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
        NormalTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialAlbedoTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(
        MaterialFresnelRoughnessTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(
        AccumulationBuf.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[6] = CD3DX12_RESOURCE_BARRIER::Transition(
        BloomTex.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PRESENT);
    cmdList->ResourceBarrier(7, barriers);
}

void Gbuffer::ClearRTVs(ComPtr<ID3D12GraphicsCommandList>& cmdList)
{
    const FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmdList->ClearRenderTargetView(DiffuseRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(ZWzanashihRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(NormalRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(MaterialAlbedoRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(MaterialFresnelRoughnessRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(AccumulationRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(BloomRTV, clearColor, 0, nullptr);
}

void Gbuffer::Resize(int width, int height, Microsoft::WRL::ComPtr<ID3D12Device> device)
{
    /*DiffuseTex.Reset();
    EmissiveTex.Reset();
    NormalTex.Reset();
    MaterialAlbedoTex.Reset();
    MaterialFresnelRoughnessTex.Reset();
    AccumulationBuf.Reset();
    BloomTex.Reset();*/

    HRESULT hr = S_OK;


    D3D12_CLEAR_VALUE clearValue_UNORM = {};
    clearValue_UNORM.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue_UNORM.Color[0] = 0.0f;
    clearValue_UNORM.Color[1] = 0.0f;
    clearValue_UNORM.Color[2] = 0.0f;
    clearValue_UNORM.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue_SNORM = {};
    clearValue_SNORM.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
    clearValue_SNORM.Color[0] = 0.0f;
    clearValue_SNORM.Color[1] = 0.0f;
    clearValue_SNORM.Color[2] = 0.0f;
    clearValue_SNORM.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue_FLOAT = {};
    clearValue_FLOAT.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    clearValue_FLOAT.Color[0] = 0.0f;
    clearValue_FLOAT.Color[1] = 0.0f;
    clearValue_FLOAT.Color[2] = 0.0f;
    clearValue_FLOAT.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue2 = {};
    clearValue2.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    clearValue2.Color[0] = 0.0f;
    clearValue2.Color[1] = 0.0f;
    clearValue2.Color[2] = 0.0f;
    clearValue2.Color[3] = 1.0f;

    D3D12_CLEAR_VALUE clearValue3 = {};
    clearValue3.Format = DXGI_FORMAT_R16G16B16A16_SNORM;
    clearValue3.Color[0] = 0.0f;
    clearValue3.Color[1] = 0.0f;
    clearValue3.Color[2] = 0.0f;
    clearValue3.Color[3] = 1.0f;

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&DiffuseTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to recreate DiffuseTex during Resize");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_FLOAT,
        IID_PPV_ARGS(&ZWzanashihTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to recreate EmissiveTex during Resize");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_SNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue3,
        IID_PPV_ARGS(&NormalTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to recreate NormalTex during Resize");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&MaterialAlbedoTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to recreate MaterialAlbedoTex during Resize");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&MaterialFresnelRoughnessTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to recreate MaterialFresnelRoughnessTex during Resize");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue2,
        IID_PPV_ARGS(&AccumulationBuf)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to recreate AccumulationBuf during Resize");

    hr = device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_COMMON,
        &clearValue_UNORM,
        IID_PPV_ARGS(&BloomTex)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to recreate BloomTex during Resize");

    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_SRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(DiffuseTex.Get(), &rtvDesc, rtvHandle);
    DiffuseRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    device->CreateRenderTargetView(ZWzanashihTex.Get(), &rtvDesc, rtvHandle);
    ZWzanashihRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_SNORM;
    device->CreateRenderTargetView(NormalTex.Get(), &rtvDesc, rtvHandle);
    NormalRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(MaterialAlbedoTex.Get(), &rtvDesc, rtvHandle);
    MaterialAlbedoRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    device->CreateRenderTargetView(MaterialFresnelRoughnessTex.Get(), &rtvDesc, rtvHandle);
    MaterialFresnelRoughnessRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    device->CreateRenderTargetView(AccumulationBuf.Get(), &rtvDesc, rtvHandle);
    AccumulationRTV = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(BloomTex.Get(), &rtvDesc, rtvHandle);
    BloomRTV = rtvHandle;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(DiffuseTex.Get(), &srvDesc, srvHandle);
    DiffuseSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    device->CreateShaderResourceView(ZWzanashihTex.Get(), &srvDesc, srvHandle);
    ZWzanashihSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_SNORM;
    device->CreateShaderResourceView(NormalTex.Get(), &srvDesc, srvHandle);
    NormalSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(MaterialAlbedoTex.Get(), &srvDesc, srvHandle);
    MaterialAlbedoSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    device->CreateShaderResourceView(MaterialFresnelRoughnessTex.Get(), &srvDesc, srvHandle);
    MaterialFresnelRoughnessSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    device->CreateShaderResourceView(AccumulationBuf.Get(), &srvDesc, srvHandle);
    AccumulationSRV = srvHandle;
    srvHandle.ptr += srvDescriptorSize;

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(BloomTex.Get(), &srvDesc, srvHandle);
    BloomSRV = srvHandle;
}

void Gbuffer::Dispose()
{
    DiffuseTex.Reset();
    ZWzanashihTex.Reset();
    NormalTex.Reset();
    MaterialAlbedoTex.Reset();
    MaterialFresnelRoughnessTex.Reset();
    AccumulationBuf.Reset();
    BloomTex.Reset();

    m_RTVDescriptorHeap.Reset();
    m_SRVDescriptorHeap.Reset();
}
