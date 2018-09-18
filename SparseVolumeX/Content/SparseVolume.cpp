//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#include "SharedConst.h"
#include "ObjLoader.h"
#include "SparseVolume.h"

using namespace DirectX;
using namespace DX;
using namespace std;
using namespace XSDX;

const auto g_pNullSRV = static_cast<LPDXShaderResourceView>(nullptr);	// Helper to Clear SRVs
const auto g_pNullUAV = static_cast<LPDXUnorderedAccessView>(nullptr);	// Helper to Clear UAVs
const auto g_uNullUint = 0u;											// Helper to Clear Buffers

CPDXInputLayout	SparseVolume::m_pVertexLayout;

SparseVolume::SparseVolume(const CPDXDevice &pDXDevice, const spShader &pShader, const spState &pState) :
	m_pDXDevice(pDXDevice),
	m_pShader(pShader),
	m_pState(pState),
	m_uVertexStride(0),
	m_uNumIndices(0)
{
	m_pDXDevice->GetImmediateContext(&m_pDXContext);
}

SparseVolume::~SparseVolume()
{
}

void SparseVolume::Init(const uint32_t uWidth, const uint32_t uHeight, const char *szFileName)
{
	m_vViewport.x = static_cast<float>(uWidth);
	m_vViewport.y = static_cast<float>(uHeight);

	ObjLoader objLoader;
	if (!objLoader.Import(szFileName, true, true)) return;

	createVB(objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices());
	createIB(objLoader.GetNumIndices(), objLoader.GetIndices());

	// Extract boundary
	const auto vCenter = objLoader.GetCenter();
	m_vBound = XMFLOAT4(vCenter.x, vCenter.y, vCenter.z, objLoader.GetRadius());

	createCBs();

	m_pTxKBufferDepth = make_unique<Texture2D>(m_pDXDevice);
	m_pTxKBufferDepth->Create(uWidth, uHeight, NUM_K_LAYERS, DXGI_FORMAT_R32_UINT);

	m_pTxKBufferDepthLS = make_unique<Texture2D>(m_pDXDevice);
	m_pTxKBufferDepthLS->Create(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, NUM_K_LAYERS, DXGI_FORMAT_R32_UINT);
}

void SparseVolume::UpdateFrame(CXMVECTOR vEyePt, CXMMATRIX mViewProj)
{
	// General matrices
	//const auto mWorld = XMMatrixScaling(m_vBound.w, m_vBound.w, m_vBound.w) *
	//	XMMatrixTranslation(m_vBound.x, m_vBound.y, m_vBound.z);
	const auto mWorld = XMMatrixIdentity();
	const auto mWorldI = XMMatrixInverse(nullptr, mWorld);
	const auto mWorldViewProj = mWorld * mViewProj;
	CBMatrices cbMatrices =
	{
		XMMatrixTranspose(mWorldViewProj),
		XMMatrixTranspose(mWorld),
		mWorldI
	};

	if (m_pCBMatrices) m_pDXContext->UpdateSubresource(m_pCBMatrices.Get(), 0, nullptr, &cbMatrices, 0, 0);

	// Light-space matrices
	const auto vLookAtPt = XMLoadFloat4(&m_vBound);
	const auto vLightPt = XMVectorSet(10.0f, 45.0f, 75.0f, 0.0f) + vLookAtPt;
	const auto mViewLS = XMMatrixLookAtLH(vLightPt, vLookAtPt, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	const auto mProjLS = XMMatrixOrthographicLH(m_vBound.w * 3.0f, m_vBound.w * 3.0f, g_fZNearLS, g_fZFarLS);
	const auto mViewProjLS = mViewLS * mProjLS;

	cbMatrices.mWorldViewProj = XMMatrixTranspose(mWorld * mViewProjLS);
	if (m_pCBMatricesLS) m_pDXContext->UpdateSubresource(m_pCBMatricesLS.Get(), 0, nullptr, &cbMatrices, 0, 0);

	// Screen space matrices
	CBPerObject cbPerObject;
	cbPerObject.mViewProjLS = XMMatrixTranspose(mViewProjLS);

	const auto mToScreen = XMMATRIX
	(
		0.5f * m_vViewport.x, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f * m_vViewport.y, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f * m_vViewport.x, 0.5f * m_vViewport.y, 0.0f, 1.0f
	);
	const auto mWorldToScreen = XMMatrixMultiply(mViewProj, mToScreen);
	const auto mScreenToWorld = XMMatrixInverse(nullptr, mWorldToScreen);
	cbPerObject.mScreenToWorld = XMMatrixTranspose(mScreenToWorld);

	if (m_pCBPerObject) m_pDXContext->UpdateSubresource(m_pCBPerObject.Get(), 0, nullptr, &cbPerObject, 0, 0);
}

void SparseVolume::Render(const CPDXUnorderedAccessView &pUAVSwapChain)
{
	depthPeelLightSpace();
	depthPeel();

	render(pUAVSwapChain);
}

void SparseVolume::RenderTest()
{
#if 0
	// Record current viewport
	auto uNumViewports = 1u;
	auto vpBack = D3D11_VIEWPORT();
	m_pDXContext->RSGetViewports(&uNumViewports, &vpBack);

	// Change viewport
	
	const auto vpLightSpace = CD3D11_VIEWPORT(0.0f, 0.0f, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	m_pDXContext->RSSetViewports(uNumViewports, &vpLightSpace);
#endif

	const auto uOffset = 0u;
	m_pDXContext->RSSetState(m_pState->CullNone().Get());

	// Set matrices
	m_pDXContext->VSSetConstantBuffers(0, 1, m_pCBMatrices.GetAddressOf());

	// Set IA
	m_pDXContext->IASetInputLayout(m_pVertexLayout.Get());
	m_pDXContext->IASetVertexBuffers(0, 1, m_pVB->GetBuffer().GetAddressOf(), &m_uVertexStride, &uOffset);
	m_pDXContext->IASetIndexBuffer(m_pIB->GetBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
	m_pDXContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set shaders
	m_pDXContext->VSSetShader(m_pShader->GetVertexShader(VS_BASEPASS).Get(), nullptr, 0);
	m_pDXContext->PSSetShader(m_pShader->GetPixelShader(PS_TEST).Get(), nullptr, 0);

	m_pDXContext->DrawIndexed(m_uNumIndices, 0, 0);

	// Reset states
	m_pDXContext->IASetInputLayout(nullptr);
	m_pDXContext->RSSetState(nullptr);
	//m_pDXContext->RSSetViewports(uNumViewports, &vpBack);
}

void SparseVolume::CreateVertexLayout(const CPDXDevice &pDXDevice, CPDXInputLayout &pVertexLayout, const spShader &pShader, const uint8_t uVS)
{
	// Define our vertex data layout for skinned objects
	const auto offset = D3D11_APPEND_ALIGNED_ELEMENT;
	const auto vLayout = vector<D3D11_INPUT_ELEMENT_DESC>
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0,		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,	0, offset,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	ThrowIfFailed(pDXDevice->CreateInputLayout(vLayout.data(), static_cast<uint32_t>(vLayout.size()),
		pShader->GetVertexShaderBuffer(uVS)->GetBufferPointer(),
		pShader->GetVertexShaderBuffer(uVS)->GetBufferSize(),
		&pVertexLayout));
}

CPDXInputLayout &SparseVolume::GetVertexLayout()
{
	return m_pVertexLayout;
}

void SparseVolume::createVB(const uint32_t uNumVert, const uint32_t uStride, const uint8_t *pData)
{
	m_uVertexStride = uStride;
	m_pVB = make_unique<RawBuffer>(m_pDXDevice);
	m_pVB->Create(uStride * uNumVert, D3D11_BIND_VERTEX_BUFFER, pData);
}

void SparseVolume::createIB(const uint32_t uNumIndices, const uint32_t *pData)
{
	m_uNumIndices = uNumIndices;
	m_pIB = make_unique<RawBuffer>(m_pDXDevice);
	m_pIB->Create(sizeof(uint32_t) * uNumIndices, D3D11_BIND_INDEX_BUFFER, pData);
}

void SparseVolume::createCBs()
{
	auto desc = CD3D11_BUFFER_DESC(sizeof(CBMatrices), D3D11_BIND_CONSTANT_BUFFER);
	ThrowIfFailed(m_pDXDevice->CreateBuffer(&desc, nullptr, &m_pCBMatrices));
	ThrowIfFailed(m_pDXDevice->CreateBuffer(&desc, nullptr, &m_pCBMatricesLS));

	desc.ByteWidth = sizeof(CBPerObject);
	ThrowIfFailed(m_pDXDevice->CreateBuffer(&desc, nullptr, &m_pCBPerObject));
}

void SparseVolume::depthPeel()
{
	// Record current RTV and DSV
	auto pRTV = CPDXRenderTargetView();
	auto pDSV = CPDXDepthStencilView();
	m_pDXContext->OMGetRenderTargets(1, &pRTV, &pDSV);

	// Change RT
	const auto uOffset = 0u;
	m_pDXContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr,
		0, 1, m_pTxKBufferDepth->GetUAV().GetAddressOf(), &g_uNullUint);

	m_pDXContext->RSSetState(m_pState->CullNone().Get());

	// Clear depth k-buffer
	const auto fClearDepth = 1.0f;
	const auto uClearDepth = reinterpret_cast<const uint32_t&>(fClearDepth);
	m_pDXContext->ClearUnorderedAccessViewUint(m_pTxKBufferDepth->GetUAV().Get(), XMVECTORU32{ { uClearDepth } }.u);

	// Set matrices
	m_pDXContext->VSSetConstantBuffers(0, 1, m_pCBMatrices.GetAddressOf());

	// Set IA
	m_pDXContext->IASetInputLayout(m_pVertexLayout.Get());
	m_pDXContext->IASetVertexBuffers(0, 1, m_pVB->GetBuffer().GetAddressOf(), &m_uVertexStride, &uOffset);
	m_pDXContext->IASetIndexBuffer(m_pIB->GetBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
	m_pDXContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set shaders
	m_pDXContext->VSSetShader(m_pShader->GetVertexShader(VS_BASEPASS).Get(), nullptr, 0);
	m_pDXContext->PSSetShader(m_pShader->GetPixelShader(PS_DEPTH_PEEL).Get(), nullptr, 0);

	m_pDXContext->DrawIndexed(m_uNumIndices, 0, 0);

	// Reset states
	m_pDXContext->IASetInputLayout(nullptr);
	m_pDXContext->RSSetState(nullptr);
	m_pDXContext->OMSetRenderTargets(1, pRTV.GetAddressOf(), pDSV.Get());
}

void SparseVolume::depthPeelLightSpace()
{
	// Record current RTV and DSV
	auto pRTV = CPDXRenderTargetView();
	auto pDSV = CPDXDepthStencilView();
	m_pDXContext->OMGetRenderTargets(1, &pRTV, &pDSV);

	// Record current viewport
	auto uNumViewports = 1u;
	auto vpBack = D3D11_VIEWPORT();
	m_pDXContext->RSGetViewports(&uNumViewports, &vpBack);

	// Change RT
	const auto uOffset = 0u;
	m_pDXContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr,
		0, 1, m_pTxKBufferDepthLS->GetUAV().GetAddressOf(), &g_uNullUint);

	// Change viewport
	const auto vpLightSpace = CD3D11_VIEWPORT(0.0f, 0.0f, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	m_pDXContext->RSSetViewports(uNumViewports, &vpLightSpace);
	m_pDXContext->RSSetState(m_pState->CullNone().Get());

	// Clear depth k-buffer
	const auto fClearDepth = 1.0f;
	const auto uClearDepth = reinterpret_cast<const uint32_t&>(fClearDepth);
	m_pDXContext->ClearUnorderedAccessViewUint(m_pTxKBufferDepthLS->GetUAV().Get(), XMVECTORU32{ { uClearDepth } }.u);

	// Set light-space matrices
	m_pDXContext->VSSetConstantBuffers(0, 1, m_pCBMatricesLS.GetAddressOf());

	// Set IA
	m_pDXContext->IASetInputLayout(m_pVertexLayout.Get());
	m_pDXContext->IASetVertexBuffers(0, 1, m_pVB->GetBuffer().GetAddressOf(), &m_uVertexStride, &uOffset);
	m_pDXContext->IASetIndexBuffer(m_pIB->GetBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
	m_pDXContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set shaders
	m_pDXContext->VSSetShader(m_pShader->GetVertexShader(VS_BASEPASS).Get(), nullptr, 0);
	m_pDXContext->PSSetShader(m_pShader->GetPixelShader(PS_DEPTH_PEEL).Get(), nullptr, 0);

	m_pDXContext->DrawIndexed(m_uNumIndices, 0, 0);

	// Reset states
	m_pDXContext->IASetInputLayout(nullptr);
	m_pDXContext->RSSetState(nullptr);
	m_pDXContext->RSSetViewports(uNumViewports, &vpBack);
	m_pDXContext->OMSetRenderTargets(1, pRTV.GetAddressOf(), pDSV.Get());
}

void SparseVolume::render(const CPDXUnorderedAccessView &pUAVSwapChain)
{
	auto pSrc = CPDXResource();
	auto desc = D3D11_TEXTURE2D_DESC();
	pUAVSwapChain->GetResource(&pSrc);
	static_cast<LPDXTexture2D>(pSrc.Get())->GetDesc(&desc);

	// Setup
	const auto pSRVs =
	{
		m_pTxKBufferDepth->GetSRV().Get(),
		m_pTxKBufferDepthLS->GetSRV().Get()
	};
	m_pDXContext->CSSetUnorderedAccessViews(0, 1, pUAVSwapChain.GetAddressOf(), &g_uNullUint);
	m_pDXContext->CSSetShaderResources(0, static_cast<uint32_t>(pSRVs.size()), pSRVs.begin());
	m_pDXContext->CSSetConstantBuffers(0, 1, m_pCBPerObject.GetAddressOf());

	// Dispatch
	m_pDXContext->CSSetShader(m_pShader->GetComputeShader(CS_RENDER).Get(), nullptr, 0);
	m_pDXContext->Dispatch(desc.Width >> 5, desc.Height >> 5, 1);

	// Unset
	const auto vpNullSRVs = vLPDXSRV(pSRVs.size(), nullptr);
	m_pDXContext->CSSetShaderResources(0, static_cast<uint32_t>(vpNullSRVs.size()), vpNullSRVs.data());
	m_pDXContext->CSSetUnorderedAccessViews(0, 1, &g_pNullUAV, &g_uNullUint);
}
