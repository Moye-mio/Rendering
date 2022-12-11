#define NUM_THREADS 512

ByteAddressBuffer AlphaBuffer : register( t0 );  // alphas per control point (to fade to)
ByteAddressBuffer AlphaWeight : register( t1 );  // blending weights used for fetching from the control point alphas
RWByteAddressBuffer CurrentBuffer : register( u0 ); // alphas per vertex (current state)

cbuffer FadeToAlphaBuffer : register(b0)
{
    float FadeToAlpha;
}

float GetCpBlendedValue(ByteAddressBuffer buffer, float weight)
{
	int firstField = 0;
	float tt = modf(weight, firstField);

	float a0 = asfloat(buffer.Load(firstField * 4));		if (a0 != a0) a0 = 0;
	float a1 = asfloat(buffer.Load((firstField+1) * 4));	if (a1 != a1) a1 = 0;
	return lerp(a0, a1, smoothstep(0,1,tt));
}

[numthreads(NUM_THREADS, 1, 1)]
void CS( uint DTid : SV_DispatchThreadID )
{
	uint addr = DTid * 4;

	float current = asfloat(CurrentBuffer.Load(addr));	// loads the current value
	float alphaWeight = asfloat(AlphaWeight.Load(addr)); // get alpha weight -> tells us from control points to interpolate

	float target = GetCpBlendedValue(AlphaBuffer, alphaWeight); // interpolate the alpha value at this position
	
	float newAlpha = current + (target - current) * FadeToAlpha; // fade to the new alpha
	CurrentBuffer.Store(addr, asuint(newAlpha)); // store it
}
