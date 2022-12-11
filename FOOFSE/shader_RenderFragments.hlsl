#include "shader_Common.hlsli"

struct FragmentData
{
    uint nColor;			// Pixel color
	uint nDepth;	// Depth 
	uint nCoverage; // Coverage
};

struct FragmentLink
{
    FragmentData fragmentData;	// Fragment data
    uint nNext;			// Link to next fragment
};

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

RWByteAddressBuffer StartOffsetSRV					: register( u1 );
RWStructuredBuffer< FragmentLink >  FragmentLinkSRV	: register( u2 );

struct QuadVSinput
{
    float4 pos : POSITION;
};

struct QuadVS_Output
{
    float4 pos : SV_POSITION; 
};

QuadVS_Output VS( QuadVSinput Input )
{
    QuadVS_Output Output;
    Output.pos = Input.pos;
    return Output;
}

struct QuadPS_Input 
{
    float4 pos                : SV_POSITION; 
};

// Max hardcoded.
#define TEMPORARY_BUFFER_MAX        256

float4 GetColor(uint nColor)
{
    float4 color;        
    color.r = ( (nColor >> 0) & 0xFF ) / 255.0f;
    color.g = ( (nColor >> 8) & 0xFF ) / 255.0f;
    color.b = ( (nColor >> 16) & 0xFF ) / 255.0f;
    color.a = ( (nColor >> 24) & 0xFF ) / 255.0f;
	return color;
}

float4 PS( QuadPS_Input input ) : SV_Target0
{    
    // index to current pixel.
	uint nIndex = (uint)input.pos.y * ScreenWidth + (uint)input.pos.x;
                           // number of fragments in current pixel's linked list.
    uint nNext = StartOffsetSRV.Load(nIndex * 4);                // get first fragment from the start offset buffer.
    
    // early exit if no fragments in the linked list.
    if( nNext == 0xFFFFFFFF ) {
		return float4( 0,0,0,0 );
    }

#ifdef MSAA_SAMPLES
	float4 result[MSAA_SAMPLES];
	float allAlpha[MSAA_SAMPLES];
	bool hasValue[MSAA_SAMPLES];
	for (int i=0; i<MSAA_SAMPLES; ++i)
	{
		 result[i] = float4(0,0,0,1);
		 allAlpha[i] = 1;
		 hasValue[i] = false;
	}
#else
	float4 result = float4(0,0,0,1);
	float allAlpha = 1;
#endif

    // Iterate the list and blend 
	[allow_uav_condition]
	for (int ii = 0; ii < TEMPORARY_BUFFER_MAX; ii++)
	{
		if ( nNext == 0xFFFFFFFF )
			break;
        FragmentLink element = FragmentLinkSRV[nNext];
        
#ifdef MSAA_SAMPLES
		for (int s=0; s<MSAA_SAMPLES; ++s)
		{
			uint cmp = 1 << s;
			if ((element.fragmentData.nCoverage & cmp) == cmp)
			{
#endif
				uint nColor = element.fragmentData.nColor;
				float4 color = GetColor(nColor);

#ifdef MSAA_SAMPLES			
				result[s].rgb += color.rgb*(1-color.a)*allAlpha[s];
				allAlpha[s] *= (color.a);
				hasValue[s] = true;
			}
		}
#else
				result.rgb += color.rgb*(1-color.a)*allAlpha;
				allAlpha *= (color.a);
#endif

        nNext = element.nNext;
    }

#ifdef MSAA_SAMPLES
	float4 sum = float4(0,0,0,0);
	for (int k=0; k<MSAA_SAMPLES; ++k)
	{
		if (!hasValue[k])
		{
			result[k] = float4(1,1,1,0);	// clear color (background...)
			sum += result[k];
		}
		else
		{
			result[k].a = 1-allAlpha[k];
			// final composition will do alpha blending like so:
			// result.rgb * (result.a) + background * (1-result.a)
			// since we allready weighted with the correct alpha, we divide once away, so that the blend state will cancel it out.
			// would be a little nicer to change the blend state
			result[k].rgb /= result[k].a;
			sum += result[k];
		}
	}
	return sum / MSAA_SAMPLES;
#else
	result.a = 1-allAlpha;
	// final composition will do alpha blending like so:
	// result.rgb * (result.a) + background * (1-result.a)
	// since we allready weighted with the correct alpha, we divide once away, so that the blend state will cancel it out.
	// would be a little nicer to change the blend state
	result.rgb /= result.a;
	return result;
#endif
}
