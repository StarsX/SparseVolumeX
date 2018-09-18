//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#include "XSDXSharedConst.h"
#include "SharedConst.h"

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbMatrices
{
	matrix	g_mViewProjLS;		// Light space
	matrix	g_mScreenToWorld;	// View-screen space
};

static const min16float g_fDensity = 1.0;
static const float g_fAbsorption = 1.0;

static const min16float3 g_vCornflowerBlue = { 0.392156899, 0.584313750, 0.929411829 };
static const min16float3 g_vClear = g_vCornflowerBlue * g_vCornflowerBlue;

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2DArray<uint>		g_txKBufDepth;		// View-screen space
Texture2DArray<uint>		g_txKBufDepthLS;	// Light space

//--------------------------------------------------------------------------------------
// Unordered access textures
//--------------------------------------------------------------------------------------
RWTexture2D<min16float4>	g_rwPresent;

//--------------------------------------------------------------------------------------
// Screen space to loacal space
//--------------------------------------------------------------------------------------
float3 ScreenToWorld(const float3 vLoc)
{
	float4 vPos = mul(float4(vLoc, 1.0), g_mScreenToWorld);
	
	return vPos.xyz / vPos.w;
}

//--------------------------------------------------------------------------------------
// Perspective clip space to view space
//--------------------------------------------------------------------------------------
float PrespectiveToViewZ(const float fz)
{
	return g_fZNear * g_fZFar / (g_fZFar - fz * (g_fZFar - g_fZNear));
}

//--------------------------------------------------------------------------------------
// Orthographic clip space to view space
//--------------------------------------------------------------------------------------
float OrthoToViewZ(const float fz)
{
	return fz * (g_fZFarLS - g_fZNearLS) + g_fZNearLS;
}

//--------------------------------------------------------------------------------------
// Compute light-path thickness
//--------------------------------------------------------------------------------------
float LightPathThickness(float3 vPos)
{
	vPos = mul(float4(vPos, 1.0), g_mViewProjLS).xyz;
	vPos.xy = vPos.xy * float2(0.5, -0.5) + 0.5;

	const uint2 vLoc = vPos.xy * SHADOW_MAP_SIZE;
	
	float fThickness = 0.0;
	for (uint i = 0; i < NUM_K_LAYERS >> 1; ++i)
	{
		// Get light-space depths
		const float fDepthFront = asfloat(g_txKBufDepthLS[uint3(vLoc, i * 2)]);
		float fDepthBack = asfloat(g_txKBufDepthLS[uint3(vLoc, i * 2 + 1)]);

		// Clip to the current point
		if (fDepthFront > vPos.z || fDepthBack >= 1.0) break;
		fDepthBack = min(fDepthBack, vPos.z);

		// Transform to view space
		const float fZFront = OrthoToViewZ(fDepthFront);
		const float fZBack = OrthoToViewZ(fDepthBack);

		fThickness += fZBack - fZFront;
	}

	return fThickness;
}

//--------------------------------------------------------------------------------------
// Simpson rule for integral approximation
//--------------------------------------------------------------------------------------
min16float Simpson(const min16float4 vf, const float a, const float b)
{
	return min16float(b - a) / 8.0 * (vf.x + 3.0 * (vf.y + vf.z) + vf.w);
}

//--------------------------------------------------------------------------------------
// Rendering from sparse volume representation
//--------------------------------------------------------------------------------------
[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	const uint2 vLoc = DTid.xy;
	const float2 vPos = vLoc;

	float fThickness = 0.0;
	min16float fScatter = 0.0;
	for (uint i = 0; i < NUM_K_LAYERS >> 1; ++i)
	{
		// Get screen-space depths
		const float fDepthFront = asfloat(g_txKBufDepth[uint3(vLoc, i * 2)]);
		const float fDepthBack = asfloat(g_txKBufDepth[uint3(vLoc, i * 2 + 1)]);

		if (fDepthFront >= 1.0 || fDepthBack >= 1.0) break;

		// Transform to world space
		const float3 vPosFront = ScreenToWorld(float3(vPos, fDepthFront));
		const float3 vPosBack = ScreenToWorld(float3(vPos, fDepthBack));
		const float3 vPosFMid = lerp(vPosFront, vPosBack, 1.0 / 3.0);
		const float3 vPosBMid = lerp(vPosFront, vPosBack, 2.0 / 3.0);

		// Transform to view space
		const float fZFront = PrespectiveToViewZ(fDepthFront);
		const float fZBack = PrespectiveToViewZ(fDepthBack);

		// Tickness of the current interval (segment)
		const float fThicknessSeg = fZBack - fZFront;
		//const float fThicknessSeg = distance(vPosFront, vPosBack);

		float4 vThickness;	// Front, 1/3, 2/3, and back thicknesses
		vThickness.x = LightPathThickness(vPosFront) + fThickness;
		vThickness.y = LightPathThickness(vPosFMid) + fThicknessSeg / 3.0 + fThickness;
		vThickness.z = LightPathThickness(vPosBMid) + fThicknessSeg * (2.0 / 3.0) + fThickness;

		// Update the total thickness
		fThickness += fThicknessSeg;
		vThickness.w = LightPathThickness(vPosBack) + fThickness;

		// Compute transmission
		const min16float4 vTransmission = min16float4(exp(-vThickness * g_fAbsorption * g_fDensity));
		
		// Integral
		fScatter += g_fDensity * Simpson(vTransmission, 0.0, fThicknessSeg);
	}

	const min16float fTransmission = min16float(exp(-fThickness * g_fAbsorption * g_fDensity));

	min16float3 vResult = fScatter * 1.0 + 0.3;
	vResult = lerp(vResult, g_vClear, fTransmission);

	g_rwPresent[DTid.xy] = min16float4(sqrt(vResult), 1.0);
}
