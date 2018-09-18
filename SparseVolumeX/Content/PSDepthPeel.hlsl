//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#include "SharedConst.h"

//--------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------
struct PSIn
{
	float4	Pos		: SV_POSITION;
	float3	WSPos	: POSWORLD;
	float3	Nrm		: NORMAL;
};

//--------------------------------------------------------------------------------------
// Unordered access textures
//--------------------------------------------------------------------------------------
RWTexture2DArray<uint>	g_rwKBufDepth;

//--------------------------------------------------------------------------------------
// Depth peeling
//--------------------------------------------------------------------------------------
[earlydepthstencil]
void main(PSIn input)
{
	const uint2 vLoc = input.Pos.xy;
	uint uDepth = asuint(input.Pos.z);
	uint uDepthPrev;

	for (uint i = 0; i < NUM_K_LAYERS; ++i)
	{
		const uint3 vTex = { vLoc, i };
		InterlockedMin(g_rwKBufDepth[vTex], uDepth, uDepthPrev);
		uDepth = max(uDepth, uDepthPrev);
	}
}
