//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XSDXShader.h"
#include "XSDXState.h"
#include "XSDXResource.h"

class SparseVolume
{
public:
	enum VertexShaderID : uint32_t
	{
		VS_BASEPASS
	};

	enum PixelShaderID : uint32_t
	{
		PS_DEPTH_PEEL,
		PS_TEST
	};

	enum ComputeShaderID : uint32_t
	{
		CS_RENDER
	};

	SparseVolume(const XSDX::CPDXDevice &pDXDevice, const XSDX::spShader &pShader, const XSDX::spState &pState);
	virtual ~SparseVolume();

	void Init(const uint32_t uWidth, const uint32_t uHeight, const char *szFileName = "Media\\bunny.obj");
	void UpdateFrame(DirectX::CXMVECTOR vEyePt, DirectX::CXMMATRIX mViewProj);
	void Render(const XSDX::CPDXUnorderedAccessView &pUAVSwapChain);
	void RenderTest();

	static void CreateVertexLayout(const XSDX::CPDXDevice &pDXDevice, XSDX::CPDXInputLayout &pVertexLayout,
		const XSDX::spShader &pShader, const uint8_t uVS);

	static XSDX::CPDXInputLayout &GetVertexLayout();

protected:
	struct CBMatrices
	{
		DirectX::XMMATRIX mWorldViewProj;
		DirectX::XMMATRIX mWorld;
		DirectX::XMMATRIX mWorldIT;
	};

	struct CBPerObject
	{
		DirectX::XMMATRIX mViewProjLS;
		DirectX::XMMATRIX mScreenToWorld;
	};

	void createVB(const uint32_t uNumVert, const uint32_t uStride, const uint8_t *pData);
	void createIB(const uint32_t uNumIndices, const uint32_t *pData);
	void createCBs();

	void depthPeel();
	void depthPeelLightSpace();
	void render(const XSDX::CPDXUnorderedAccessView &pUAVSwapChain);

	uint32_t						m_uVertexStride;
	uint32_t						m_uNumIndices;

	DirectX::XMFLOAT4				m_vBound;
	DirectX::XMFLOAT2				m_vViewport;

	XSDX::upRawBuffer				m_pVB;
	XSDX::upRawBuffer				m_pIB;
	XSDX::CPDXBuffer				m_pCBMatrices;
	XSDX::CPDXBuffer				m_pCBMatricesLS;
	XSDX::CPDXBuffer				m_pCBPerObject;
	
	XSDX::upTexture2D				m_pTxKBufferDepth;		// View-screen space
	XSDX::upTexture2D				m_pTxKBufferDepthLS;	// Light space

	XSDX::spShader					m_pShader;
	XSDX::spState					m_pState;

	XSDX::CPDXDevice				m_pDXDevice;
	XSDX::CPDXContext				m_pDXContext;

	static XSDX::CPDXInputLayout	m_pVertexLayout;
};

using upSparseVolume = std::unique_ptr<SparseVolume>;
using spSparseVolume = std::shared_ptr<SparseVolume>;
using vuSparseVolume = std::vector<upSparseVolume>;
using vpSparseVolume = std::vector<spSparseVolume>;
