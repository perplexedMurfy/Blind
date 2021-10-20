cbuffer Constants : register(b0) {
	float4x4 WorldToScreen;
};

struct QuadSampleV {
	uint VertexID : SV_VERTEXID;
	float3 Center : CENTER;
	float2 Dimentions : DIMENTIONS;
	float1 ArrayIndex : INDEX;
	
	float2 TextureCoord0 : TEXTCOORD0;
	float2 TextureCoord1 : TEXTCOORD1;
	float2 TextureCoord2 : TEXTCOORD2;
	float2 TextureCoord3 : TEXTCOORD3;
};

Texture2D TextureDebugCol : register(t0);
Texture2D TextureBackground : register(t1);
Texture2D TextureForeground : register(t2);
Texture2D TextureAtlas : register(t3);

SamplerState SamplerConfig : register(s0);

struct QuadSampleP {
	float4 Position : SV_POSITION;
	float2 TextureCoord : TEXTCOORD0;
	float1 ArrayIndex : INDEX;
};

// NOTE using Triangle strip primivative
QuadSampleP QuadSampleVertex(QuadSampleV Input) {
	QuadSampleP Result;
	float4 Vertex = float4(0, 0, 0, 0);
	
	if (Input.VertexID == 0) {
		Vertex = float4(-1, 1, 0, 1);
		Result.TextureCoord = Input.TextureCoord0;
	}
	else if (Input.VertexID == 1) {
		Vertex = float4(1, 1, 0, 1);
		Result.TextureCoord = Input.TextureCoord1;
	}
	else if (Input.VertexID == 2) {
		Vertex = float4(-1, -1, 0, 1);
		Result.TextureCoord = Input.TextureCoord2;
	}
	else if (Input.VertexID == 3) {
		Vertex = float4(1, -1, 0, 1);
		Result.TextureCoord = Input.TextureCoord3;
	}
	
	Vertex.x = Vertex.x * Input.Dimentions.x/2;
	Vertex.y = Vertex.y * Input.Dimentions.y/2;

	Vertex.x = Vertex.x + Input.Center.x;
	Vertex.y = Vertex.y + Input.Center.y;
	
	Vertex.z = Input.Center.z;
	
	Result.Position = mul(WorldToScreen, Vertex);
	Result.ArrayIndex = Input.ArrayIndex;
	
	return Result;
}

float4 QuadSamplePixel(QuadSampleP Input) : SV_TARGET {
	uint Index = uint(Input.ArrayIndex);
	Texture2D Temp;
	
	if (Index == 0) {
		return TextureDebugCol.Sample(SamplerConfig, Input.TextureCoord);
	}
	if (Index == 1) {
		return TextureBackground.Sample(SamplerConfig, Input.TextureCoord);
	}
	if (Index == 2) {
		return TextureForeground.Sample(SamplerConfig, Input.TextureCoord);
	}
	if (Index == 3) {
		return TextureAtlas.Sample(SamplerConfig, Input.TextureCoord);
	}

	return float4(1, 0, 1, 1);
}
