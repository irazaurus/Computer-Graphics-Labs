#pragma once

#include "../Common/d3dUtil.h"

using Microsoft::WRL::ComPtr;

class Gbuffer {

    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

    ComPtr<ID3D12Resource> DiffuseTex       = nullptr;
    ComPtr<ID3D12Resource> ZWzanashihTex      = nullptr;
    ComPtr<ID3D12Resource> NormalTex        = nullptr;
    ComPtr<ID3D12Resource> MaterialAlbedoTex                = nullptr;
    ComPtr<ID3D12Resource> MaterialFresnelRoughnessTex      = nullptr;

    ComPtr<ID3D12Resource> AccumulationBuf  = nullptr;
    ComPtr<ID3D12Resource> BloomTex         = nullptr;


public:
    D3D12_CPU_DESCRIPTOR_HANDLE DiffuseSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE ZWzanashihSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE NormalSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE MaterialAlbedoSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE MaterialFresnelRoughnessSRV;

    D3D12_CPU_DESCRIPTOR_HANDLE DiffuseRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE ZWzanashihRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE NormalRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE MaterialAlbedoRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE MaterialFresnelRoughnessRTV;

    D3D12_CPU_DESCRIPTOR_HANDLE AccumulationSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE BloomSRV;

    D3D12_CPU_DESCRIPTOR_HANDLE AccumulationRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE BloomRTV;

    ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> m_SRVDescriptorHeap;

    const int NumBuffers = 7;
    int Channel0SRVHeapIndex;

    Gbuffer(int width, int height, Microsoft::WRL::ComPtr<ID3D12Device> device);

    ComPtr<ID3D12DescriptorHeap> getSRVDescriptorHeap() const { return m_SRVDescriptorHeap; }

    void TransitToOpaqueRenderingState(ComPtr<ID3D12GraphicsCommandList>& c);
    void TransitToLightsRenderingState(ComPtr<ID3D12GraphicsCommandList>& c);
    void TransitToTonemappingState(ComPtr<ID3D12GraphicsCommandList>& c);
    void TransitToCommon(ComPtr<ID3D12GraphicsCommandList>& c);
    void TransitFromRenderTargetToCommon(ComPtr<ID3D12GraphicsCommandList>& c);
    void TransitFromShaderResourceToCommon(ComPtr<ID3D12GraphicsCommandList>& c);
    void ClearRTVs(ComPtr<ID3D12GraphicsCommandList>& cmdList);

    void Resize(int width, int height, Microsoft::WRL::ComPtr<ID3D12Device> device);

    void Dispose();

    ComPtr<ID3D12Resource> getEmissiveTex() { return ZWzanashihTex; }
    ComPtr<ID3D12Resource> getNormalTex() { return NormalTex; }
};