#define NUM_THREADS 512

ByteAddressBuffer AlphaPing : register( t0 );  // alphas per control point (to fade to)
ByteAddressBuffer LineID : register( t1 );		// line indices per control point
RWByteAddressBuffer AlphaPong : register( u0 ); // alphas per vertex (current state)

cbuffer FadeToAlphaBuffer : register(b0)
{
    float FadeToAlpha;
	float LaplaceWeight;
}

[numthreads(NUM_THREADS, 1, 1)]
void CS( uint DTid : SV_DispatchThreadID )
{
	uint addr = DTid * 4;

	float3 values = 1 - asfloat(AlphaPing.Load3(addr - 4));	// loads three values (left, current, right)
	uint3 indices = LineID.Load3(addr - 4);					// loads the indices

	float weight = 0;
	float target = 0;
	if (indices.x == indices.y && values.x == values.x) { 
		target += values.x;
		weight += 1;
	}
	if (indices.z == indices.y && values.z == values.z) {
		target += values.z;
		weight += 1;
	}

	if (weight > 0) target /= weight;

	float newAlpha = 0;
	if (weight > 0)
		newAlpha = values.y + (target - values.y) * LaplaceWeight; // fade to the new alpha

	AlphaPong.Store(addr, asuint(1 - newAlpha));
}
