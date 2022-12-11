#include "shader_Common.hlsli"

//-------------------------------------------------------------------------
// Parameter
//-------------------------------------------------------------------------

cbuffer CameraParameters : register(b0)
{
	row_major matrix View = matrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	row_major matrix Projection = matrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
}

cbuffer RendererParameters : register(b1)
{
	float Q;
	float R;
	float Lambda;
	int TotalNumberOfControlPoints;
	float4 LineColor;
	float4 HaloColor;
	float StripWidth;
	float HaloPortion;
	int ScreenWidth;
	int ScreenHeight;
}

struct FragmentData
{
    unsigned int uDepth;		// Depth as uint
	float fAlphaWeight;			// Alpha weight
	float fImportance;			// Importance
};

struct FragmentLink
{
    FragmentData fragmentData;	// Fragment data
    uint nNext;			// Link to next fragment
};

// Fragment And Link Buffer
RWStructuredBuffer< FragmentLink >  FLBuffer        : register( u1 );
// Start Offset Buffer
RWByteAddressBuffer StartOffsetBuffer                : register( u2 );

#ifdef MSAA_SAMPLES
Texture2DMS<float> DepthBuffer : register( t0 );
#else
Texture2D<float> DepthBuffer : register( t0 );
#endif

//-------------------------------------------------------------------------
// Layouts

//-------------------------------------------------------------------------

struct VS_INPUT10
{
    float3 PositionA : POSITIONA;
	float3 PositionB : POSITIONB;
	int IDA : IDA;
	int IDB : IDB;
	float ImportanceA : IMPORTANCEA;
	float ImportanceB : IMPORTANCEB;
	float AlphaWeightA : ALPHAWEIGHTA;
	float AlphaWeightB : ALPHAWEIGHTB;
};

struct GS_INPUT10
{
	float4 PositionA : POSITIONA;
	float4 PositionB : POSITIONB;
	int IDA : IDA;
	int IDB : IDB;
	float ImportanceA : IMPORTANCEA;
	float ImportanceB : IMPORTANCEB;
	float AlphaWeightA : ALPHAWEIGHTA;
	float AlphaWeightB : ALPHAWEIGHTB;
};

struct GS_OUTPUT10
{
    float4 Position : SV_POSITION;
	float3 VRCPosition : TEXCOORD0;
	float AlphaWeight : ALPHAWEIGHT;
	float3 VRCDirection : TEXCOORD1;
	float Importance : IMPORTANCE;
    float2 TexCoord : TEXCOORD2;
};

struct PS_INPUT10
{
    float4 Position : SV_POSITION;
	float3 VRCPosition : TEXCOORD0;
	float AlphaWeight : ALPHAWEIGHT;
	float3 VRCDirection : TEXCOORD1;
	float Importance : IMPORTANCE;
	float2 TexCoord : TEXCOORD2;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
GS_INPUT10 VS(VS_INPUT10 input)
{
    GS_INPUT10 output;
	output.PositionA = mul(float4(input.PositionA, 1), View);    
	output.PositionB = mul(float4(input.PositionB, 1), View);
	output.IDA = input.IDA;
	output.IDB = input.IDB;
	output.ImportanceA = input.ImportanceA;
	output.ImportanceB = input.ImportanceB;
	output.AlphaWeightA = input.AlphaWeightA;
	output.AlphaWeightB = input.AlphaWeightB;
    return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader
//--------------------------------------------------------------------------------------
[maxvertexcount(4)]
void GS(line GS_INPUT10 input[2], inout TriangleStream<GS_OUTPUT10> output)
{
	if (input[0].IDA == input[0].IDB && input[0].IDB == input[1].IDA && input[1].IDA == input[1].IDB)
	{
		GS_OUTPUT10 vertex[4];
	
		float4 p1 = input[0].PositionB.xyzw / input[0].PositionB.w;
		float4 p0 = input[1].PositionA.xyzw / input[1].PositionA.w;

		float4 a0 = input[0].PositionA.xyzw / input[0].PositionA.w;
		float4 a1 = input[1].PositionB.xyzw / input[1].PositionB.w;

		float t0 = 0;
		float t1 = 0;

		float3 dir3_0 = normalize(p1.xyz - a0.xyz);
		float3 dir3_1 = normalize(a1.xyz - p0.xyz);

		float2 dir0 = normalize(p1.xy - a0.xy) * StripWidth;
		float2 dir1 = normalize(a1.xy - p0.xy) * StripWidth;
		float4 off0 = float4(-dir0.y, dir0.x, 0, 0);
		float4 off1 = float4(-dir1.y, dir1.x, 0, 0);	

		vertex[0].Position = p0 + off0;
		vertex[0].TexCoord = float2(0, t0);
		vertex[0].VRCDirection = dir3_0;
		vertex[0].Importance = input[1].ImportanceA;
		vertex[0].AlphaWeight = input[1].AlphaWeightA;
	
		vertex[1].Position = p0 - off0;
		vertex[1].TexCoord = float2(1, t0);
		vertex[1].VRCDirection = dir3_0;
		vertex[1].Importance = input[1].ImportanceA;
		vertex[1].AlphaWeight = input[1].AlphaWeightA;

		vertex[2].Position = p1 + off1;
		vertex[2].TexCoord = float2(0, t1);
		vertex[2].VRCDirection = dir3_1;
		vertex[2].Importance = input[0].ImportanceB;
		vertex[2].AlphaWeight = input[0].AlphaWeightB;
	
		vertex[3].Position = p1 - off1;
		vertex[3].TexCoord = float2(1, t1);
		vertex[3].VRCDirection = dir3_1;
		vertex[3].Importance = input[0].ImportanceB;
		vertex[3].AlphaWeight = input[0].AlphaWeightB;

		for (int i = 0; i < 4; i++)
		{
			vertex[i].VRCPosition = vertex[i].Position.xyz;
			vertex[i].Position = mul(vertex[i].Position, Projection);
			output.Append(vertex[i]);
		}
		output.RestartStrip();
	}
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

void PS( PS_INPUT10 input)
{
	float depth = 0;

	int isHalo;
	float halfDistCenter = abs(input.TexCoord.x - 0.5);
	if (halfDistCenter * 2 > HaloPortion)
	{
		float offset = (halfDistCenter) * 2 * StripWidth;
		float4 offsetPosNPC = mul(float4(input.VRCPosition, 1) + float4(0,0,offset, 0), Projection);
		depth = offsetPosNPC.z / offsetPosNPC.w;
		isHalo = 1;
	}
	else
	{
		float4 offsetPosNPC = mul(float4(input.VRCPosition, 1), Projection);
		depth = offsetPosNPC.z / offsetPosNPC.w;		
		isHalo = 0;
	}

	uint x = input.Position.x;
	uint y = input.Position.y;

	#ifdef MSAA_SAMPLES
	// only check one sample. We know the other bits the object can write to. (don't care for MSAA here. would be correcter to loop all samples, though.)
	float refDepth = DepthBuffer.Load( uint2(input.Position.xy), 1 );
	if (depth < refDepth)
	#else
	float refDepth = DepthBuffer.Load( uint3(input.Position.xy, 0) );
	if (depth < refDepth)
	#endif
	{
		// Create fragment data.
		FragmentLink element;
		element.fragmentData.uDepth = asuint(depth); // ((uint)(depth * (16777215.0f)) << 8) | uImportance;
		element.fragmentData.fAlphaWeight = input.AlphaWeight;
		element.fragmentData.fImportance = saturate(input.Importance);

		// Increment and get current pixel count.
		uint nPixelCount= FLBuffer.IncrementCounter();

		// Read and update Start Offset Buffer.
		uint nIndex = y * ScreenWidth + x;
		uint nStartOffsetAddress = 4 * nIndex;
		uint nOldStartOffset;
    
		StartOffsetBuffer.InterlockedExchange(nStartOffsetAddress, nPixelCount, nOldStartOffset );

		// Store fragment link.
		element.nNext = nOldStartOffset;
		FLBuffer[ nPixelCount ] = element;
	}
}
