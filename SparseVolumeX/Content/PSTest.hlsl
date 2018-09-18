struct PSIn
{
	float4	Pos		: SV_POSITION;
	float3	WSPos	: POSWORLD;
	float3	Nrm		: NORMAL;
};

float4 main(PSIn input) : SV_TARGET
{
	float3 vNorm = normalize(input.Nrm);
	float3 vLightDir = normalize(float3(10.0, 45.0, 75.0));
	float fLightAmt = saturate(dot(vNorm, vLightDir));

	return float4(fLightAmt.xxx + 0.3, 1.0);
}
