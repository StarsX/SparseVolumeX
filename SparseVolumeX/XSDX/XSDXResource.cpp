//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "XSDXResource.h"

using namespace DirectX;
using namespace DX;
using namespace XSDX;

Resource::Resource(const CPDXDevice &pDXDevice) :
	m_pSRV(nullptr),
	m_pDXDevice(pDXDevice)
{
}

Resource::~Resource(void)
{
}

const CPDXShaderResourceView &Resource::GetSRV() const
{
	return m_pSRV;
}

void Resource::CreateReadBuffer(const CPDXDevice &pDXDevice, CPDXBuffer &pDstBuffer, const CPDXBuffer &pSrcBuffer)
{
	auto desc = D3D11_BUFFER_DESC();
	pSrcBuffer->GetDesc(&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;

	ThrowIfFailed(pDXDevice->CreateBuffer(&desc, nullptr, &pDstBuffer));
}

//--------------------------------------------------------------------------------------
// 2D Texture
//--------------------------------------------------------------------------------------

Texture2D::Texture2D(const CPDXDevice &pDXDevice) :
	Resource(pDXDevice),
	m_pTexture(nullptr),
	m_vpUAVs(0),
	m_vpSRVs(0),
	m_vpSubSRVs(0)
{
}

Texture2D::~Texture2D()
{
}

void Texture2D::Create(const uint32_t uWidth, const uint32_t uHeight, const uint32_t uArraySize,
	const DXGI_FORMAT eFormat, const uint8_t uBindFlags, const uint8_t uMips,
	const lpcvoid pInitialData, const uint8_t uStride, const D3D11_USAGE eUsage)
{
	// Setup the texture description.
	const auto textureDesc = CD3D11_TEXTURE2D_DESC(eFormat, uWidth, uHeight, uArraySize, uMips,
		uBindFlags, eUsage, eUsage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0);

	if (pInitialData)
	{
		const auto bufferInitData = D3D11_SUBRESOURCE_DATA { pInitialData, uStride * uWidth, 0 };
		ThrowIfFailed(m_pDXDevice->CreateTexture2D(&textureDesc, &bufferInitData, &m_pTexture));
	}
	else ThrowIfFailed(m_pDXDevice->CreateTexture2D(&textureDesc, nullptr, &m_pTexture));

	// Create SRV
	if (uBindFlags & D3D11_BIND_SHADER_RESOURCE) CreateSRV(uArraySize, 1, uMips);

	// Create UAV
	if (uBindFlags & D3D11_BIND_UNORDERED_ACCESS) CreateUAV(uArraySize, uMips);
}

void Texture2D::Create(const uint32_t uWidth, const uint32_t uHeight, const DXGI_FORMAT eFormat,
	const uint8_t uBindFlags, const uint8_t uMips, const lpcvoid pInitialData,
	const uint8_t uStride, const D3D11_USAGE eUsage)
{
	Create(uWidth, uHeight, 1, eFormat, uBindFlags, uMips, pInitialData, uStride, eUsage);
}

void Texture2D::CreateSRV(const uint32_t uArraySize, const uint8_t uSamples, const uint8_t uMips)
{
	const auto pTexture = m_pTexture.Get();

	// Setup the description of the shader resource view.
	const auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pTexture, uArraySize > 1 ?
		(uSamples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY) :
		(uSamples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D));

	// Create the shader resource view.
	ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture, &srvDesc, &m_pSRV));

	if (uMips > 1)
	{
		auto uMip = 0ui8;
		VEC_ALLOC(m_vpSRVs, uMips);
		for (auto &pSRV : m_vpSRVs)
		{
			// Setup the description of the shader resource view.
			const auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pTexture, uArraySize > 1 ?
				(uSamples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY) :
				(uSamples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D),
				DXGI_FORMAT_UNKNOWN, uMip++, 1);

			// Create the shader resource view.
			ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV));
		}
	}
}

void Texture2D::CreateUAV(const uint32_t uArraySize, const uint8_t uMips)
{
	const auto pTexture = m_pTexture.Get();

	auto uMip = 0ui8;
	VEC_ALLOC(m_vpUAVs, max(uMips, 1));
	for (auto &pUAV : m_vpUAVs)
	{
		// Setup the description of the unordered access view.
		const auto uavDesc = CD3D11_UNORDERED_ACCESS_VIEW_DESC(pTexture, uArraySize > 1 ?
			D3D11_UAV_DIMENSION_TEXTURE2DARRAY : D3D11_UAV_DIMENSION_TEXTURE2D,
			DXGI_FORMAT_UNKNOWN, uMip++);

		// Create the unordered access view.
		ThrowIfFailed(m_pDXDevice->CreateUnorderedAccessView(pTexture, &uavDesc, &pUAV));
	}
}

void Texture2D::CreateSubSRVs()
{
	const auto pTexture = m_pTexture.Get();
	auto texDesc = D3D11_TEXTURE2D_DESC();
	pTexture->GetDesc(&texDesc);

	auto uMip = 1ui8;
	VEC_ALLOC(m_vpSubSRVs, max(texDesc.MipLevels, 1) - 1);
	for (auto &pSRV : m_vpSubSRVs)
	{
		// Setup the description of the shader resource view.
		const auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pTexture, texDesc.ArraySize > 1 ?
			(texDesc.SampleDesc.Count > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY) :
			(texDesc.SampleDesc.Count > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D),
			DXGI_FORMAT_UNKNOWN, uMip++);

		// Create the shader resource view.
		ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV));
	}
}

const CPDXTexture2D &Texture2D::GetTexture() const
{
	return m_pTexture;
}

const CPDXUnorderedAccessView &Texture2D::GetUAV(const uint8_t i) const
{
	return m_vpUAVs[i];
}

const CPDXShaderResourceView &Texture2D::GetSRVLevel(const uint8_t i) const
{
	return m_vpSRVs[i];
}

const CPDXShaderResourceView &Texture2D::GetSubSRV(const uint8_t i) const
{
	return i ? m_vpSubSRVs[i - 1] : m_pSRV;
}

//--------------------------------------------------------------------------------------
// Render target
//--------------------------------------------------------------------------------------

RenderTarget::RenderTarget(const CPDXDevice &pDXDevice) :
	Texture2D(pDXDevice),
	m_vvpRTVs(0)
{
}

RenderTarget::~RenderTarget()
{
}

void RenderTarget::Create(const uint32_t uWidth, const uint32_t uHeight, const uint32_t uArraySize,
	const DXGI_FORMAT eFormat, const uint8_t uSamples, const uint8_t uMips, const uint8_t uBindFlags)
{
	create(uWidth, uHeight, uArraySize, eFormat, uSamples, uMips, uBindFlags);
	const auto pTexture = m_pTexture.Get();

	VEC_ALLOC(m_vvpRTVs, uArraySize);
	for (auto i = 0ui8; i < uArraySize; ++i)
	{
		auto uMip = 0ui8;
		VEC_ALLOC(m_vvpRTVs[i], max(uMips, 1));
		for (auto &pRTV : m_vvpRTVs[i])
		{
			// Setup the description of the render target view.
			const auto rtvDesc = CD3D11_RENDER_TARGET_VIEW_DESC(pTexture, uArraySize > 1 ?
				(uSamples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY) :
				(uSamples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D),
				DXGI_FORMAT_UNKNOWN, uMip++, i, 1);

			// Create the render target view.
			ThrowIfFailed(m_pDXDevice->CreateRenderTargetView(pTexture, &rtvDesc, &pRTV));
		}
	}
}

void RenderTarget::Create(const uint32_t uWidth, const uint32_t uHeight, const DXGI_FORMAT eFormat,
	const uint8_t uSamples, const uint8_t uMips, const uint8_t uBindFlags)
{
	Create(uWidth, uHeight, 1, eFormat, uSamples, uMips, uBindFlags);
}

void RenderTarget::CreateArray(const uint32_t uWidth, const uint32_t uHeight, const uint32_t uArraySize,
	const DXGI_FORMAT eFormat, const uint8_t uSamples, const uint8_t uMips, const uint8_t uBindFlags)
{
	create(uWidth, uHeight, uArraySize, eFormat, uSamples, uMips, uBindFlags);
	const auto pTexture = m_pTexture.Get();

	VEC_ALLOC(m_vvpRTVs, 1);
	VEC_ALLOC(m_vvpRTVs[0], max(uMips, 1));

	auto uMip = 0ui8;
	for (auto &pRTV : m_vvpRTVs[0])
	{
		// Setup the description of the render target view.
		const auto rtvDesc = CD3D11_RENDER_TARGET_VIEW_DESC(pTexture, uArraySize > 1 ?
			(uSamples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY) :
			(uSamples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D),
			DXGI_FORMAT_UNKNOWN, uMip++);

		// Create the render target view.
		ThrowIfFailed(m_pDXDevice->CreateRenderTargetView(pTexture, &rtvDesc, &pRTV));
	}
}

void RenderTarget::Populate(const CPDXShaderResourceView &pSRVSrc, const spShader &pShader,
	const uint8_t uSRVSlot, const uint8_t uSlice, const uint8_t uMip)
{
	auto pDXContext = CPDXContext();
	m_pDXDevice->GetImmediateContext(&pDXContext);

	// Change the render target and clear the frame-buffer
	auto pRTVBack = CPDXRenderTargetView();
	auto pDSVBack = CPDXDepthStencilView();
	pDXContext->OMGetRenderTargets(1, &pRTVBack, &pDSVBack);
	pDXContext->OMSetRenderTargets(1, m_vvpRTVs[uSlice][uMip].GetAddressOf(), nullptr);
	pDXContext->ClearRenderTargetView(m_vvpRTVs[uSlice][uMip].Get(), Colors::Transparent);

	// Change the viewport
	auto uNumViewports = 1u;
	auto VpBack = D3D11_VIEWPORT();
	const auto VpDepth = CD3D11_VIEWPORT(m_pTexture.Get(), m_vvpRTVs[uSlice][uMip].Get());
	pDXContext->RSGetViewports(&uNumViewports, &VpBack);
	pDXContext->RSSetViewports(uNumViewports, &VpDepth);

	pDXContext->PSSetShaderResources(uSRVSlot, 1, pSRVSrc.GetAddressOf());
	pDXContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pDXContext->VSSetShader(pShader->GetVertexShader(g_uVSScreenQuad).Get(), nullptr, 0);
	pDXContext->GSSetShader(nullptr, nullptr, 0);
	pDXContext->PSSetShader(pShader->GetPixelShader(g_uPSResample).Get(), nullptr, 0);
	//pDXContext->PSSetSamplers(uSmpSlot, 1, pState->AnisotropicWrap().GetAddressOf());

	pDXContext->Draw(3, 0);

	const auto pNullSRV = LPDXShaderResourceView(nullptr);
	pDXContext->PSSetShaderResources(uSRVSlot, 1, &pNullSRV);
	pDXContext->VSSetShader(nullptr, nullptr, 0);
	pDXContext->PSSetShader(nullptr, nullptr, 0);

	pDXContext->RSSetViewports(uNumViewports, &VpBack);
	pDXContext->OMSetRenderTargets(1, pRTVBack.GetAddressOf(), pDSVBack.Get());
}

const CPDXRenderTargetView &RenderTarget::GetRTV(const uint8_t uSlice, const uint8_t uMip) const
{
	assert(m_vvpRTVs.size() > uSlice);
	assert(m_vvpRTVs[uSlice].size() > uMip);
	return m_vvpRTVs[uSlice][uMip];
}

const uint8_t RenderTarget::GetArraySize() const
{
	return static_cast<uint8_t>(m_vvpRTVs.size());
}

const uint8_t RenderTarget::GetNumMips(const uint8_t uSlice) const
{
	return static_cast<uint8_t>(m_vvpRTVs[uSlice].size());
}

void RenderTarget::create(const uint32_t uWidth, const uint32_t uHeight, const uint32_t uArraySize,
	const DXGI_FORMAT eFormat, const uint8_t uSamples, const uint8_t uMips, const uint8_t uBindFlags)
{
	// Setup the render target texture description.
	const auto textureDesc = CD3D11_TEXTURE2D_DESC(eFormat, uWidth, uHeight, uArraySize, uMips,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | uBindFlags,
		D3D11_USAGE_DEFAULT, 0, uSamples, 0, uMips == 1 ? 0 : D3D11_RESOURCE_MISC_GENERATE_MIPS);

	// Create the render target texture.
	ThrowIfFailed(m_pDXDevice->CreateTexture2D(&textureDesc, nullptr, &m_pTexture));

	// Create SRV
	CreateSRV(uArraySize, uSamples);

	// Create UAV
	if (uBindFlags & D3D11_BIND_UNORDERED_ACCESS) CreateUAV(uArraySize, uMips);
}

//--------------------------------------------------------------------------------------
// Depth-stencil
//--------------------------------------------------------------------------------------

DepthStencil::DepthStencil(const CPDXDevice &pDXDevice) :
	Texture2D(pDXDevice),
	m_vpDSVs(0),
	m_vpDSVROs(0)
{
}

DepthStencil::~DepthStencil()
{
}

void DepthStencil::Create(const uint32_t uWidth, const uint32_t uHeight, const uint32_t uArraySize,
	const DXGI_FORMAT eFormat, const uint8_t uBindFlags, const uint8_t uSamples, const uint8_t uMips)
{
	const auto bSRV = static_cast<bool>(uBindFlags & D3D11_BIND_SHADER_RESOURCE);

	// Map formats
	auto fmtTexture = DXGI_FORMAT_R32_TYPELESS;
	auto fmtSRV = DXGI_FORMAT_R32_FLOAT;
	
	switch (eFormat)
	{
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		fmtTexture = DXGI_FORMAT_R24G8_TYPELESS;
		fmtSRV = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	case DXGI_FORMAT_D16_UNORM:
		fmtTexture = DXGI_FORMAT_R16_TYPELESS;
		fmtSRV = DXGI_FORMAT_R16_UNORM;
		break;
	}

	// Setup the render depth stencil description.
	{
		const auto textureDesc = CD3D11_TEXTURE2D_DESC(
			fmtTexture, uWidth, uHeight, uArraySize, uMips, D3D11_BIND_DEPTH_STENCIL | uBindFlags,
			D3D11_USAGE_DEFAULT, 0, uSamples);

		// Create the depth stencil texture.
		ThrowIfFailed(m_pDXDevice->CreateTexture2D(&textureDesc, nullptr, &m_pTexture));
	}

	const auto pTexture = m_pTexture.Get();
	if (bSRV)
	{
		// Setup the description of the shader resource view.
		const auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(uArraySize > 1 ?
			(uSamples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY) :
			(uSamples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D), fmtSRV);

		// Create the shader resource view.
		ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture, &srvDesc, &m_pSRV));
	}

	const auto uNumMips = max(uMips, 1);
	VEC_ALLOC(m_vpDSVs, uNumMips);
	VEC_ALLOC(m_vpDSVROs, uNumMips);

	for (auto i = 0ui8; i < uNumMips; ++i)
	{
		// Setup the description of the depth stencil view.
		auto dsvDesc = CD3D11_DEPTH_STENCIL_VIEW_DESC(uArraySize > 1 ?
			(uSamples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY) :
			(uSamples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D), eFormat, i);

		// Create the depth stencil view.
		ThrowIfFailed(m_pDXDevice->CreateDepthStencilView(pTexture, &dsvDesc, &m_vpDSVs[i]));

		if (bSRV)
		{
			dsvDesc.Flags = eFormat == DXGI_FORMAT_D24_UNORM_S8_UINT ?
				D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL :
				D3D11_DSV_READ_ONLY_DEPTH;

			// Create the depth stencil view.
			ThrowIfFailed(m_pDXDevice->CreateDepthStencilView(pTexture, &dsvDesc, &m_vpDSVROs[i]));
		}
		else m_vpDSVROs[i] = m_vpDSVs[i];
	}
}

void DepthStencil::Create(const uint32_t uWidth, const uint32_t uHeight, const DXGI_FORMAT eFormat,
	const uint8_t uBindFlags, const uint8_t uSamples, const uint8_t uMips)
{
	Create(uWidth, uHeight, 1, eFormat, uBindFlags, uSamples, uMips);
}

const CPDXDepthStencilView &DepthStencil::GetDSV(const uint8_t uMip) const
{
	assert(m_vpDSVs.size() > uMip);
	return m_vpDSVs[uMip];
}

const CPDXDepthStencilView &DepthStencil::GetDSVRO(const uint8_t uMip) const
{
	assert(m_vpDSVROs.size() > uMip);
	return m_vpDSVROs[uMip];
}

const uint8_t DepthStencil::GetNumMips() const
{
	return static_cast<uint8_t>(m_vpDSVs.size());
}

//--------------------------------------------------------------------------------------
// 3D Texture
//--------------------------------------------------------------------------------------

Texture3D::Texture3D(const CPDXDevice &pDXDevice) :
	Resource(pDXDevice),
	m_pTexture(nullptr),
	m_vpUAVs(0),
	m_vpSRVs(0),
	m_vpSubSRVs(0)
{
}

Texture3D::~Texture3D()
{
}

void Texture3D::Create(const uint32_t uWidth, const uint32_t uHeight, const uint32_t uDepth,
	const DXGI_FORMAT eFormat, const uint8_t uBindFlags, const uint8_t uMips,
	const lpcvoid pInitialData, const uint8_t uStride, const D3D11_USAGE eUsage)
{
	// Setup the texture description.
	const auto textureDesc = CD3D11_TEXTURE3D_DESC(eFormat, uWidth, uHeight, uDepth, uMips,
		uBindFlags, eUsage, eUsage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0);

	if (pInitialData)
	{
		const auto bufferInitData = D3D11_SUBRESOURCE_DATA
		{ pInitialData, uStride * uWidth, uStride * uWidth * uHeight };
		ThrowIfFailed(m_pDXDevice->CreateTexture3D(&textureDesc, &bufferInitData, &m_pTexture));
	}
	else ThrowIfFailed(m_pDXDevice->CreateTexture3D(&textureDesc, nullptr, &m_pTexture));
	
	// Create SRV
	const auto pTexture = m_pTexture.Get();
	if (uBindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		// Setup the description of the shader resource view.
		const auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pTexture);

		// Create the shader resource view.
		ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture, &srvDesc, &m_pSRV));

		if (uMips > 1)
		{
			auto uMip = 0ui8;
			VEC_ALLOC(m_vpSRVs, uMips);
			for (auto &pSRV : m_vpSRVs)
			{
				// Setup the description of the shader resource view.
				const auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pTexture, DXGI_FORMAT_UNKNOWN, uMip++, 1);

				// Create the shader resource view.
				ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV));
			}
		}
	}

	// Create UAV
	if (uBindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		auto uMip = 0ui8;
		VEC_ALLOC(m_vpUAVs, max(uMips, 1));
		for (auto &pUAV : m_vpUAVs)
		{
			// Setup the description of the unordered access view.
			const auto uWSize = uDepth >> uMip;
			const auto uavDesc = CD3D11_UNORDERED_ACCESS_VIEW_DESC(pTexture, DXGI_FORMAT_UNKNOWN, uMip++, 0, uWSize);

			// Create the unordered access view.
			ThrowIfFailed(m_pDXDevice->CreateUnorderedAccessView(pTexture, &uavDesc, &pUAV));
		}
	}
}

void Texture3D::CreateSubSRVs()
{
	const auto pTexture = m_pTexture.Get();
	auto texDesc = D3D11_TEXTURE3D_DESC();
	pTexture->GetDesc(&texDesc);

	auto uMip = 1ui8;
	VEC_ALLOC(m_vpSubSRVs, max(texDesc.MipLevels, 1) - 1);
	for (auto &pSRV : m_vpSubSRVs)
	{
		// Setup the description of the shader resource view.
		const auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pTexture, DXGI_FORMAT_UNKNOWN, uMip++);

		// Create the shader resource view.
		ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV));
	}
}

const CPDXTexture3D &Texture3D::GetTexture() const
{
	return m_pTexture;
}

const CPDXUnorderedAccessView &Texture3D::GetUAV(const uint8_t i) const
{
	return m_vpUAVs[i];
}

const CPDXShaderResourceView &Texture3D::GetSRVLevel(const uint8_t i) const
{
	return m_vpSRVs[i];
}

const CPDXShaderResourceView &Texture3D::GetSubSRV(const uint8_t i) const
{
	return i ? m_vpSubSRVs[i - 1] : m_pSRV;
}

//--------------------------------------------------------------------------------------
// Raw buffer
//--------------------------------------------------------------------------------------

RawBuffer::RawBuffer(const CPDXDevice &pDXDevice) :
	Resource(pDXDevice),
	m_pBuffer(nullptr),
	m_pUAV(nullptr)
{
}

RawBuffer::~RawBuffer()
{
}

void RawBuffer::Create(const uint32_t uByteWidth, const uint8_t uBindFlags,
	const lpcvoid pInitialData, const uint8_t uUAVFlags, const D3D11_USAGE eUsage)
{
	const auto bSRV = static_cast<bool>(uBindFlags & D3D11_BIND_SHADER_RESOURCE);
	const auto bUAV = static_cast<bool>(uBindFlags & D3D11_BIND_UNORDERED_ACCESS);

	// Create RB
	auto bufferDesc = CD3D11_BUFFER_DESC(uByteWidth, uBindFlags, eUsage,
		eUsage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0);
	bufferDesc.MiscFlags = 
		bSRV || bUAV ? D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS : bufferDesc.MiscFlags;

	if (pInitialData)
	{
		const auto bufferInitData = D3D11_SUBRESOURCE_DATA{ pInitialData };
		ThrowIfFailed(m_pDXDevice->CreateBuffer(&bufferDesc, &bufferInitData, &m_pBuffer));
	}
	else ThrowIfFailed(m_pDXDevice->CreateBuffer(&bufferDesc, nullptr, &m_pBuffer));

	// Create SRV
	if (bSRV) CreateSRV(uByteWidth);

	// Create UAV
	if (bUAV)
	{
		const auto pBuffer = m_pBuffer.Get();

		// Setup the description of the unordered access view.
		const auto uavDesc = CD3D11_UNORDERED_ACCESS_VIEW_DESC(pBuffer,
			DXGI_FORMAT_R32_TYPELESS, 0, uByteWidth / 4, uUAVFlags);

		// Create the unordered access view.
		ThrowIfFailed(m_pDXDevice->CreateUnorderedAccessView(pBuffer, &uavDesc, &m_pUAV));
	}
}

void RawBuffer::CreateSRV(const uint32_t uByteWidth)
{
	// Create SRV
	const auto pBuffer = m_pBuffer.Get();

	// Setup the description of the shader resource view.
	const auto desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pBuffer, DXGI_FORMAT_R32_TYPELESS,
		0, uByteWidth / 4, D3D11_BUFFEREX_SRV_FLAG_RAW);

	// Create the shader resource view.
	ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pBuffer, &desc, &m_pSRV));
}

const CPDXBuffer &RawBuffer::GetBuffer() const
{
	return m_pBuffer;
}

const CPDXUnorderedAccessView &RawBuffer::GetUAV() const
{
	return m_pUAV;
}

//--------------------------------------------------------------------------------------
// Typed buffer
//--------------------------------------------------------------------------------------

TypedBuffer::TypedBuffer(const CPDXDevice &pDXDevice) :
	RawBuffer(pDXDevice)
{
}

TypedBuffer::~TypedBuffer()
{
}

void TypedBuffer::Create(const uint32_t uNumElements, const uint32_t uStride,
	const DXGI_FORMAT eFormat, const uint8_t uBindFlags, const lpcvoid pInitialData,
	const uint8_t uUAVFlags, const D3D11_USAGE eUsage)
{
	// Create TB
	const auto bufferDesc = CD3D11_BUFFER_DESC(uNumElements * uStride, uBindFlags,
		eUsage, eUsage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0);

	if (pInitialData)
	{
		const auto bufferInitData = D3D11_SUBRESOURCE_DATA{ pInitialData };
		ThrowIfFailed(m_pDXDevice->CreateBuffer(&bufferDesc, &bufferInitData, &m_pBuffer));
	}
	else ThrowIfFailed(m_pDXDevice->CreateBuffer(&bufferDesc, nullptr, &m_pBuffer));

	// Create SRV
	if (uBindFlags & D3D11_BIND_SHADER_RESOURCE) CreateSRV(uNumElements, eFormat);

	// Create UAV
	if (uBindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		const auto pBuffer = m_pBuffer.Get();

		// Setup the description of the unordered access view.
		const auto uavDesc = CD3D11_UNORDERED_ACCESS_VIEW_DESC(pBuffer,
			eFormat, 0, uNumElements, uUAVFlags);

		// Create the unordered access view.
		ThrowIfFailed(m_pDXDevice->CreateUnorderedAccessView(pBuffer, &uavDesc, &m_pUAV));
	}
}

void TypedBuffer::CreateSRV(const uint32_t uNumElements, const DXGI_FORMAT eFormat)
{
	// Create SRV
	const auto pBuffer = m_pBuffer.Get();

	// Setup the description of the shader resource view.
	const auto desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pBuffer, eFormat, 0, uNumElements);

	// Create the shader resource view.
	ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pBuffer, &desc, &m_pSRV));
}

//--------------------------------------------------------------------------------------
// Structured buffer
//--------------------------------------------------------------------------------------

StructuredBuffer::StructuredBuffer(const CPDXDevice &pDXDevice) :
	RawBuffer(pDXDevice)
{
}

StructuredBuffer::~StructuredBuffer()
{
}

void StructuredBuffer::Create(const uint32_t uNumElements, const uint32_t uStride,
	const uint8_t uBindFlags, const lpcvoid pInitialData, const uint8_t uUAVFlags,
	const D3D11_USAGE eUsage)
{
	// Create SB
	const auto bufferDesc = CD3D11_BUFFER_DESC(uNumElements * uStride, uBindFlags,
		eUsage, eUsage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0,
		D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, uStride);

	if (pInitialData)
	{
		const auto bufferInitData = D3D11_SUBRESOURCE_DATA{ pInitialData };
		ThrowIfFailed(m_pDXDevice->CreateBuffer(&bufferDesc, &bufferInitData, &m_pBuffer));
	}
	else ThrowIfFailed(m_pDXDevice->CreateBuffer(&bufferDesc, nullptr, &m_pBuffer));

	// Create SRV
	if (uBindFlags & D3D11_BIND_SHADER_RESOURCE) CreateSRV(uNumElements);

	// Create UAV
	if (uBindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		const auto pBuffer = m_pBuffer.Get();

		// Setup the description of the unordered access view.
		const auto uavDesc = CD3D11_UNORDERED_ACCESS_VIEW_DESC(pBuffer,
			DXGI_FORMAT_UNKNOWN, 0, uNumElements, uUAVFlags);

		// Create the unordered access view.
		ThrowIfFailed(m_pDXDevice->CreateUnorderedAccessView(pBuffer, &uavDesc, &m_pUAV));
	}
}

void StructuredBuffer::CreateSRV(const uint32_t uNumElements)
{
	// Create SRV
	const auto pBuffer = m_pBuffer.Get();

	// Setup the description of the shader resource view.
	const auto desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pBuffer, DXGI_FORMAT_UNKNOWN, 0, uNumElements);

	// Create the shader resource view.
	ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pBuffer, &desc, &m_pSRV));
}
