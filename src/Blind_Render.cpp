
struct quad_sample_instance_data {
	hmm_v3 Center;
	hmm_v2 Dimentions;
	f32  TextureId;

	// NOTE(Michael): I don't feel like learning how to use per instance data and per vertex data at the same time. I'm going to just put 4 "per instance values" that corrispond to the 4 verts that we process in the QuadSample shader.
	hmm_v2 TextureCoord0;
	hmm_v2 TextureCoord1;
	hmm_v2 TextureCoord2;
	hmm_v2 TextureCoord3;
};

struct quad_fill_instance_data {
	hmm_v3 Center;
	hmm_v2 Dimentions;
	hmm_v3 Color;
};

void Render_EnqueueQuadSample(quad_sample_instance_data);
void Render_EnqueueQuadFill(quad_fill_instance_data Data);
void Render_UpdateTextureArray(u32, u8*, u32);
void Render_ClearScreen(hmm_v4 Color);
void Render_Draw();
