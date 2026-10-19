// The sprite shaders D3D11Device draws with: the vertex color times the
// texel, where a coverage texel (one red channel) is white with the coverage
// as alpha. Compiled ahead of time into source/D3D11Shaders.h by
// compile_d3d11.py, so the library needs no shader compiler.

cbuffer SpriteConstants : register(b0) {
	// Sprite coordinates to clip space, column-major as litefont's Matrix4.
	float4x4 transform;
	// x is 1 when the texture holds coverage in its red channel.
	float4 options;
};

struct VertexInput {
	float2 position : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 color : COLOR0;
};

struct PixelInput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	float4 color : COLOR0;
};

Texture2D atlas : register(t0);
SamplerState atlasSampler : register(s0);

PixelInput vertexMain(VertexInput input) {
	PixelInput output;
	output.position = mul(transform, float4(input.position, 0.0, 1.0));
	output.texCoord = input.texCoord;
	output.color = input.color;
	return output;
}

float4 pixelMain(PixelInput input) : SV_TARGET {
	float4 texel = atlas.Sample(atlasSampler, input.texCoord);
	if (options.x > 0.5) {
		texel = float4(1.0, 1.0, 1.0, texel.r);
	}
	return input.color * texel;
}
