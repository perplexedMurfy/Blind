#include <Dxgi.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include "Generated/ShaderByteCode_QuadSampleVertex.h"
#include "Generated/ShaderByteCode_QuadSamplePixel.h"

#include "Generated/ShaderByteCode_QuadFillVertex.h"
#include "Generated/ShaderByteCode_QuadFillPixel.h"

// @TODO(Michael) I need to figure out how to pass per vertex texture mapping data in quad sample.
// @TODO(Michael) I think there is also something wrong with my shader code for selecting textures.

struct shader_program {
	// @TODO(Michael) As of now (2021-10-20) we only use one buffer per shader program. We could remove the 2nd buffer but I've decided to leave it here for the time being. If this is still unused in the future feel free to remove it.
	// [0] = Index Buffer
	// [1] = Vertex Buffer
	ID3D11Buffer *Buffers[2];

	ID3D11InputLayout *Layout;
	
	ID3D11VertexShader *VertexShader;
	ID3D11PixelShader *PixelShader;
};

global_var shader_program QuadSample;
global_var shader_program QuadFill;

global_var ID3D11Device *D3D_Device = 0;
global_var ID3D11DeviceContext *D3D_DeviceCtx = 0;
global_var IDXGIFactory *DXGI_Factory = 0;

global_var IDXGISwapChain *SwapChain = 0;
global_var ID3D11RenderTargetView *RenderTargetView = 0;

// [0] = Debug Colision
// [1] = Background
// [2] = Foreground
// [3] = TextureAtlas
global_var ID3D11Texture2D *Textures[4] = {};
global_var ID3D11ShaderResourceView *TexturesSRV[4] = {};

global_var quad_sample_instance_data QuadSampleInstanceData[100] = {};
global_var u32 QuadSampleInstanceDataCount = 0;

global_var quad_sample_vertex_data QuadSampleVertexData[100] = {};
global_var u32 QuadSampleVertexDataCount = 0;

global_var quad_fill_instance_data QuadFillInstanceData[100] = {};
global_var u32 QuadFillInstanceDataCount = 0;

#define D3D_CompileBakedVertexShader(Name, Dest) {	  \
		HRESULT MACRO_Result = D3D_Device->CreateVertexShader(ShaderByteCode_##Name, ArraySize(ShaderByteCode_##Name), 0, &Dest); \
		Assert(SUCCEEDED(MACRO_Result)); \
	}

#define D3D_CompileBakedPixelShader(Name, Dest) {	  \
		HRESULT MACRO_Result = D3D_Device->CreatePixelShader(ShaderByteCode_##Name, ArraySize(ShaderByteCode_##Name), 0, &Dest); \
		Assert(SUCCEEDED(MACRO_Result)); \
	}

inline 
void D3D_UpdateBuffer(ID3D11Resource *Resource, void *UpdateData, u32 StartByte, u32 EndByte) {
	// NOTE: StartByte and EndByte refer to locations within the Resource, not the UpdateData.
	D3D11_BOX Box = {StartByte, 0, 0, EndByte, 1, 1};
	D3D_DeviceCtx->UpdateSubresource(Resource, 0, &Box, UpdateData, 0, 0);
}

inline 
void D3D_UpdateConstantBuffer(ID3D11Resource *Resource, void *UpdateData) {
	// NOTE: Must update the entire resource.
	D3D_DeviceCtx->UpdateSubresource(Resource, 0, 0, UpdateData, 0, 0);
}

void Render_UpdateTextureArray(u32 ArrayIndex, u8 *TextureData, u32 TextureDataPitch) {
	D3D11_MAPPED_SUBRESOURCE Mapped;
	HRESULT HResult = D3D_DeviceCtx->Map(Textures[ArrayIndex], 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	Assert(SUCCEEDED(HResult));
	
	for (s32 Index = 0; Index < WindowHeight; Index++) {
		memcpy((u8*)Mapped.pData + Mapped.RowPitch * Index, TextureData + TextureDataPitch * Index, TextureDataPitch);
	}
	
	D3D_DeviceCtx->Unmap(Textures[ArrayIndex], 0);
}

void Render_Init(HWND Window) {
	// NOTE: Create base devices. Use these to create everything else in D3D11 and DXGI.
	{
		UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
		D3D_FEATURE_LEVEL FeatureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
		};
		HRESULT Result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, CreateFlags, FeatureLevels, ArraySize(FeatureLevels), D3D11_SDK_VERSION, &D3D_Device, 0, &D3D_DeviceCtx);
		Assert(SUCCEEDED(Result));
		Assert(D3D_Device);
		Assert(D3D_DeviceCtx);
	}

	{
		HRESULT Result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&DXGI_Factory);
		Assert(SUCCEEDED(Result));
		Assert(DXGI_Factory);
	}

	// NOTE: Create swap chain. We create a double buffer setup.
	{
		DXGI_SWAP_CHAIN_DESC SwapChainDesc;
		SwapChainDesc.BufferDesc.Width = WindowWidth;
		SwapChainDesc.BufferDesc.Height = WindowHeight;
		SwapChainDesc.BufferDesc.RefreshRate = {60, 1};
		SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		SwapChainDesc.SampleDesc = {1, 0};
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		SwapChainDesc.BufferCount = 2;
		SwapChainDesc.OutputWindow = Window;
		SwapChainDesc.Windowed = true;
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Flags = 0;
		HRESULT Result = DXGI_Factory->CreateSwapChain(D3D_Device, &SwapChainDesc, &SwapChain);
		Assert(SUCCEEDED(Result));
		Assert(SwapChain);
	}

	// enables break on API error 
	{
		ID3D11InfoQueue *Info;
		HRESULT Result = D3D_Device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&Info);
		Assert(Info);
		Assert(SUCCEEDED(Result));
		Result = Info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
		Assert(SUCCEEDED(Result));
		Result = Info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
		Assert(SUCCEEDED(Result));
	}

	// NOTE: Creates a RenderTargetView. We need to bookkeep this so that we can set with OMSetRenderTargets().
	// @TODO(Michael) can we set and forget with OMSetRenderTargets()?
	{
		ID3D11Texture2D *FrameBuffer = 0;
		HRESULT Result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);
		Assert(SUCCEEDED(Result));
		Assert(FrameBuffer);
		
		Result = D3D_Device->CreateRenderTargetView(FrameBuffer, 0, &RenderTargetView);
		Assert(SUCCEEDED(Result));
		Assert(RenderTargetView);
	}

	//~ Initing Shaders
	
	D3D_CompileBakedVertexShader(QuadSampleVertex, QuadSample.VertexShader);
	D3D_CompileBakedPixelShader(QuadSamplePixel, QuadSample.PixelShader);
	{
		const D3D11_INPUT_ELEMENT_DESC IED[] = {
			{ "CENTER", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{ "DIMENTIONS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{ "INDEX", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{ "TEXTCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		HRESULT HResult = D3D_Device->CreateInputLayout(IED, ArraySize(IED), ShaderByteCode_QuadSampleVertex, ArraySize(ShaderByteCode_QuadSampleVertex), &QuadSample.Layout);
		Assert(SUCCEEDED(HResult));
		Assert(QuadSample.Layout);
	}

	{
		D3D11_BUFFER_DESC BufferDescription = {};
		BufferDescription.Usage = D3D11_USAGE_DEFAULT;
		BufferDescription.ByteWidth = sizeof(QuadSampleInstanceData);
		BufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDescription.CPUAccessFlags = 0;
		BufferDescription.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA InitialData = {};
		InitialData.pSysMem = QuadSampleInstanceData;
		HRESULT Result = D3D_Device->CreateBuffer(&BufferDescription, &InitialData, &QuadSample.Buffers[0]);
		Assert(SUCCEEDED(Result));
		Assert(QuadSample.Buffers[0]);
	}

	{
		D3D11_BUFFER_DESC BufferDescription = {};
		BufferDescription.Usage = D3D11_USAGE_DEFAULT;
		BufferDescription.ByteWidth = sizeof(QuadSampleVertexData);
		BufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDescription.CPUAccessFlags = 0;
		BufferDescription.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA InitialData = {};
		InitialData.pSysMem = QuadSampleInstanceData;
		HRESULT Result = D3D_Device->CreateBuffer(&BufferDescription, &InitialData, &QuadSample.Buffers[1]);
		Assert(SUCCEEDED(Result));
		Assert(QuadSample.Buffers[1]);
	}

	// Texture Array
	{
		D3D11_TEXTURE2D_DESC TextureDescription = {};
		TextureDescription.Width = WindowWidth;
		TextureDescription.Height = WindowHeight;
		TextureDescription.MipLevels = 1;
		TextureDescription.ArraySize = 1;
		TextureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		TextureDescription.SampleDesc = {1, 0};
		TextureDescription.Usage = D3D11_USAGE_DYNAMIC;
		TextureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		TextureDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		TextureDescription.MiscFlags = 0;

		HRESULT Result = D3D_Device->CreateTexture2D(&TextureDescription, 0, &Textures[0]);
		Assert(SUCCEEDED(Result));

		Result = D3D_Device->CreateShaderResourceView(Textures[0], 0, &TexturesSRV[0]);
		Assert(SUCCEEDED(Result));
#if 0
		Result = D3D_Device->CreateTexture2D(&TextureDescription, 0, &Textures[1]);
		Assert(SUCCEEDED(Result));

		Result = D3D_Device->CreateShaderResourceView(Textures[1], 0, &TexturesSRV[1]);
		Assert(SUCCEEDED(Result));

		Result = D3D_Device->CreateTexture2D(&TextureDescription, 0, &Textures[2]);
		Assert(SUCCEEDED(Result));

		Result = D3D_Device->CreateShaderResourceView(Textures[2], 0, &TexturesSRV[2]);
		Assert(SUCCEEDED(Result));

		Result = D3D_Device->CreateTexture2D(&TextureDescription, 0, &Textures[3]);
		Assert(SUCCEEDED(Result));

		Result = D3D_Device->CreateShaderResourceView(Textures[3], 0, &TexturesSRV[3]);
		Assert(SUCCEEDED(Result));
#endif
	}

	D3D_CompileBakedVertexShader(QuadFillVertex, QuadFill.VertexShader);
	D3D_CompileBakedPixelShader(QuadFillPixel, QuadFill.PixelShader);
	{
		const D3D11_INPUT_ELEMENT_DESC IED[] = {
			{ "CENTER", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{ "DIMENTIONS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		};

		HRESULT HResult = D3D_Device->CreateInputLayout(IED, ArraySize(IED), ShaderByteCode_QuadFillVertex, ArraySize(ShaderByteCode_QuadFillVertex), &QuadFill.Layout);
		Assert(SUCCEEDED(HResult));
		Assert(QuadFill.Layout);
	}

	{
		D3D11_BUFFER_DESC BufferDescription = {};
		BufferDescription.Usage = D3D11_USAGE_DEFAULT;
		BufferDescription.ByteWidth = sizeof(QuadFillInstanceData);
		BufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDescription.CPUAccessFlags = 0;
		BufferDescription.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA InitialData = {};
		InitialData.pSysMem = QuadFillInstanceData;
		HRESULT Result = D3D_Device->CreateBuffer(&BufferDescription, &InitialData, &QuadFill.Buffers[0]);
		Assert(SUCCEEDED(Result));
		Assert(QuadFill.Buffers[0]);
	}
}

// At a high level, we want to enqueue data and then draw it all in a batch.

void Render_EnqueueQuadSample(quad_sample_vertex_data VertexData, quad_sample_instance_data InstanceData) {
	QuadSampleInstanceData[QuadSampleInstanceDataCount] = InstanceData;
	QuadSampleInstanceDataCount++;

	QuadSampleVertexData[QuadSampleVertexDataCount] = VertexData;
	QuadSampleVertexDataCount++;
}

void Render_EnqueueQuadFill(quad_fill_instance_data Data) {
	QuadFillInstanceData[QuadFillInstanceDataCount] = Data;
	QuadFillInstanceDataCount++;
}

void Render_ClearScreen(hmm_v4 Color) {
	D3D_DeviceCtx->ClearRenderTargetView(RenderTargetView, Color.Elements);
}

void Render_Draw() {
	Render_ClearScreen(hmm_v4{0.7, 0.7, 0.7, 1.0});
	ID3D11BlendState *QuadBlendState = 0;
	
	// One time Initlization stuff
	local_persist b8 IsInitilized = false;
	if (!IsInitilized) {
		IsInitilized = true;

		ID3D11Buffer *ConstantBuffer = 0;
		{
			D3D11_BUFFER_DESC BufferDesc;
			BufferDesc.ByteWidth = sizeof(f32) * 4 * 4;
			BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			BufferDesc.MiscFlags = 0;

			HRESULT Result = D3D_Device->CreateBuffer(&BufferDesc, 0, &ConstantBuffer);
			Assert(SUCCEEDED(Result));
		}
		
		ID3D11SamplerState *SamplerState = 0;
		{
			D3D11_SAMPLER_DESC SamplerDesc = {};
			SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			SamplerDesc.MipLODBias = 0;
			SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			SamplerDesc.MinLOD = 5;
			SamplerDesc.MaxLOD = 0;
			
			D3D_Device->CreateSamplerState(&SamplerDesc, &SamplerState);
		}

		ID3D11BlendState *QuadBlendState = 0;
		{
			// NOTE(Michael) I don't toally understand it, but this should allow for alphablending.
			D3D11_BLEND_DESC BlendDesc = {};
			BlendDesc.RenderTarget[0].BlendEnable = true;

			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			
			BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

			BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			D3D_Device->CreateBlendState(&BlendDesc, &QuadBlendState);
		}

		// Loads data into the constant buffer
		{
			const f32 Width = (f32)WindowWidth;
			const f32 Height = (f32)WindowHeight;
			D3D11_MAPPED_SUBRESOURCE Mapped = {};
			hmm_mat4 WorldToScreen =
				HMM_Scale(hmm_v3{1/(Width/2), 1/(Height/2), 1}) *
				HMM_Translate(hmm_v3{-Width/2, -Height/2, 0})
				;

			D3D_DeviceCtx->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);

			memcpy(Mapped.pData, WorldToScreen.Elements, sizeof(WorldToScreen));

			D3D_DeviceCtx->Unmap(ConstantBuffer, 0);
		}
		// Sets this constant buffer as b0 in the shader
		D3D_DeviceCtx->VSSetShader(QuadSample.VertexShader, 0, 0);
		D3D_DeviceCtx->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		
		D3D_DeviceCtx->VSSetShader(QuadFill.VertexShader, 0, 0);
		D3D_DeviceCtx->VSSetConstantBuffers(0, 1, &ConstantBuffer);

		D3D_DeviceCtx->PSSetShader(QuadSample.PixelShader, 0, 0);
		D3D_DeviceCtx->PSSetSamplers(0, 1, &SamplerState);
	}

	// General State Setting
	D3D11_VIEWPORT Viewport = {0, 0, WindowWidth, WindowHeight, 0, 1};
	D3D_DeviceCtx->RSSetViewports(1, &Viewport);
	D3D_DeviceCtx->OMSetRenderTargets(1, &RenderTargetView, 0);
	D3D_DeviceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	
	// Quad Sample
	{
		D3D_DeviceCtx->OMSetBlendState(QuadBlendState, 0, 0xffffffff);
		D3D_DeviceCtx->IASetInputLayout(QuadSample.Layout);
		D3D_DeviceCtx->VSSetShader(QuadSample.VertexShader, 0, 0);
		D3D_DeviceCtx->PSSetShader(QuadSample.PixelShader, 0, 0);
#if 0 
		D3D_DeviceCtx->PSSetShaderResources(0, 4, TexturesSRV);
#else
		D3D_DeviceCtx->PSSetShaderResources(0, 1, TexturesSRV);
#endif

		u32 Stride[] = {sizeof(quad_sample_instance_data), sizeof(quad_sample_vertex_data)};
		u32 Offset[] = {0, 0};
		D3D_DeviceCtx->IASetVertexBuffers(0, 1, QuadSample.Buffers, Stride, Offset);
	
		D3D_UpdateBuffer(QuadSample.Buffers[0], QuadSampleInstanceData, 0, sizeof(QuadSampleInstanceData));
		D3D_UpdateBuffer(QuadSample.Buffers[1], QuadSampleVertexData, 0, sizeof(QuadSampleVertexData));

		D3D_DeviceCtx->OMSetRenderTargets(1, &RenderTargetView, 0);

		D3D_DeviceCtx->DrawInstanced(4, QuadSampleInstanceDataCount, 0, 0);
	}
	
	// Quad Fill
	{
		D3D_DeviceCtx->OMSetBlendState(0, 0, 0xffffffff);
		D3D_DeviceCtx->IASetInputLayout(QuadFill.Layout);
		D3D_DeviceCtx->VSSetShader(QuadFill.VertexShader, 0, 0);
		D3D_DeviceCtx->PSSetShader(QuadFill.PixelShader, 0, 0);

		u32 Stride = sizeof(quad_fill_instance_data);
		u32 Offset = 0;
		D3D_DeviceCtx->IASetVertexBuffers(0, 1, &QuadFill.Buffers[0], &Stride, &Offset);
	
		D3D_UpdateBuffer(QuadFill.Buffers[0], QuadFillInstanceData, 0, sizeof(QuadFillInstanceData));

		D3D_DeviceCtx->OMSetRenderTargets(1, &RenderTargetView, 0);

		D3D_DeviceCtx->DrawInstanced(4, QuadFillInstanceDataCount, 0, 0);
	}	
	
	SwapChain->Present(1, 0);

	QuadSampleInstanceDataCount = 0;
	QuadFillInstanceDataCount = 0;
}
