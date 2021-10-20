
struct quad_sample_instance_data {
	hmm_v3 Center;
	hmm_v2 Dimentions;
	u32 TextureId;
};

struct quad_sample_vertex_data {
	hmm_v2 TextureCoords[4];
};

struct quad_fill_instance_data {
	hmm_v3 Center;
	hmm_v2 Dimentions;
	hmm_v3 Color;
};

void Render_EnqueueQuadSample(quad_sample_vertex_data, quad_sample_instance_data);
void Render_EnqueueQuadFill(quad_fill_instance_data Data);
void Render_UpdateTextureArray(u32, u8*, u32);
void Render_ClearScreen(hmm_v4 Color);
void Render_Draw();
