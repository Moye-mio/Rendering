#include "shader_Common.hlsli"

struct FragmentData
{
	uint nColor;	// Pixel color
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
	float4 pos                : SV_POSITION; 
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
// We're in trouble if the fragment linked list is larger than this...
#define TEMPORARY_BUFFER_MAX        256

void PS( QuadPS_Input input )
{   
	// index to current pixel.
	uint nIndex = (uint)input.pos.y * ScreenWidth + (uint)input.pos.x;

	FragmentData aData[ TEMPORARY_BUFFER_MAX ];            // temporary buffer
	uint nNumFragment = 0;                                // number of fragments in current pixel's linked list.
	uint nNext = StartOffsetSRV.Load(nIndex * 4);                // get first fragment from the start offset buffer.
	uint nFirst = nNext;

	// early exit if no fragments in the linked list.
	if( nNext == 0xFFFFFFFF ) {
		return;
	}

	// Read and store linked list data to the temporary buffer.    
	[allow_uav_condition]
	for (int ii = 0; ii < TEMPORARY_BUFFER_MAX; ii++)
	{
		if ( nNext == 0xFFFFFFFF )
			break;
		FragmentLink element = FragmentLinkSRV[nNext];
		aData[ nNumFragment ] = element.fragmentData;
		
		nNumFragment++;
		nNext = element.nNext;
	}
	
	// insertion sort
	[allow_uav_condition]
	for (uint jj = 1; jj < nNumFragment; jj++)
	{
		FragmentData valueToInsert = aData[jj];
		uint holePos;
		
		[allow_uav_condition]
		for (holePos=jj; holePos>0; holePos--)
		{
			if (valueToInsert.nDepth > aData[holePos - 1].nDepth) break;
			aData[holePos] = aData[holePos - 1];
		}
		aData[holePos] = valueToInsert;
	}

	// Store the sorted list. This is not efficient! Its better to update the Next-Links instead.
	nNext = nFirst;
	[allow_uav_condition]
	for( uint x = 0; x < nNumFragment; ++x )
	{
		FragmentLinkSRV[nNext].fragmentData = aData[ x ];		// front to back
		nNext = FragmentLinkSRV[nNext].nNext;
	}
}
