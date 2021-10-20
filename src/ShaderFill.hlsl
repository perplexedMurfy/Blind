cbuffer Constants : register(b0) {
	float4x4 WorldToScreen;
};

struct QuadFillV {
	uint VertexID : SV_VERTEXID;
	float3 Center : CENTER;
	float2 Dimentions : DIMENTIONS;
	float3 Color : COLOR;
};

struct QuadFillP {
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

QuadFillP QuadFillVertex(QuadFillV Input) {
	QuadFillP Result;
	float4 Vertex;
	
	if (Input.VertexID == 0) {
		Vertex = float4(-1, 1, 0, 1);
	}
	else if (Input.VertexID == 1) {
		Vertex = float4(1, 1, 0, 1);
	}
	else if (Input.VertexID == 2) {
		Vertex = float4(-1, -1, 0, 1);
	}
	else if (Input.VertexID == 3) {
		Vertex = float4(1, -1, 0, 1);
	}

	Vertex.x = Vertex.x * Input.Dimentions.x/2;
	Vertex.y = Vertex.y * Input.Dimentions.y/2;

	Vertex.x = Vertex.x + Input.Center.x;
	Vertex.y = Vertex.y + Input.Center.y;
	
	Vertex.z = Input.Center.z;
	
	Result.Position = mul(WorldToScreen, Vertex);
	Result.Color = float4(Input.Color.r, Input.Color.g, Input.Color.b, 1);
	
	return Result;
}

float4 QuadFillPixel(QuadFillP Input) : SV_TARGET {
	return Input.Color;
}
