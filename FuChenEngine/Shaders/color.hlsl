//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbPerObject : register(b0)
{
	//float4x4 gWorldViewProj; 
	float4x4 gRotation;
	float4x4 gWorld;
	float4x4 gView;
	float4x4 gProj;
	float time;
};

struct VertexIn
{
	float3 PosL  : POSITION;
    float4 Color : COLOR;
	float4 Normal : NORMAL;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float4 Color : COLOR;
	float4 Normal : NORMAL;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	// Transform to homogeneous clip space.
	//vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	//vout.PosH = mul(float4(vin.PosL, abs(sin(time))*0.5+0.5), gWorldViewProj);
	float4x4 gWorldViewProj = mul(mul(gProj, gView), gWorld);
	vout.PosH = mul(float4(vin.PosL, abs(sin(time)) * 0.5 + 0.5), gWorldViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;

	vout.Normal = mul(vin.Normal, gRotation);
	//vout.Normal = vin.Normal;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	//Gamma Correction
	float4 RetColor = pow(pin.Normal * 0.5f + 0.5f,1/2.2f);
	return RetColor;
	//return pin.Color;
}


