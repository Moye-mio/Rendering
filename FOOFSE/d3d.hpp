#ifndef _D3D_INCLUDE_ONCE
#define _D3D_INCLUDE_ONCE

#include <dxgi.h>
#include <d3d11_1.h>
#include <fstream>
#include "shader_Common.hlsli"

class D3D
{
public:

	// Constructor. Creates a Direct3D device.
	D3D(HWND WindowHandle) : _Device(NULL), _ImmediateContext(NULL),
	_BsDefault(NULL), _BsBlendBackToFront(NULL), _DsTestWriteOff(NULL), _RsCullNone(NULL)
	{
		IDXGIFactory* factory;
		if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory))) return;
		
		HRESULT hr = S_OK;

		UINT createDeviceFlags = 0;
#ifdef _DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_DRIVER_TYPE driverTypes[] =
		{
			D3D_DRIVER_TYPE_UNKNOWN,
		};
		UINT numDriverTypes = sizeof(driverTypes) / sizeof(driverTypes[0]);

		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
		unsigned int numFeatureLevels = 1;
		D3D_FEATURE_LEVEL usedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

		// iterate the display adapters and look for a DirectX 11 capable device.
		for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
		{
			D3D_DRIVER_TYPE driverType = driverTypes[driverTypeIndex];

			IDXGIAdapter* adapter;
			for (UINT i = 0;
				factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND;
				++i)
			{
				DXGI_ADAPTER_DESC desc;
				adapter->GetDesc(&desc);

				std::wstring adapterDesc(desc.Description);
				if (adapterDesc[0] == 'I') continue;
				hr = D3D11CreateDevice(adapter, driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &_Device, &usedFeatureLevel, &_ImmediateContext);
				if (SUCCEEDED(hr))
				{
					wprintf(L"D3D is using: %s\n", adapterDesc.c_str());
					break;
				}
			}

			if (!_Device) {
				printf("Couldn't create DirectX device.\nMaybe DirectX 11 is not supported on your machine?\n");
				exit(-1);
			}

			if (adapter) adapter->Release();

			if (SUCCEEDED(hr))
				break;
		}

		// silcence the vertex stride message
		ID3D11Debug *d3dDebug = nullptr;
		if (SUCCEEDED(_Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug)))
		{
			ID3D11InfoQueue *d3dInfoQueue = nullptr;
			if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
			{
				D3D11_MESSAGE_ID hide[] =
				{
					D3D11_MESSAGE_ID_DEVICE_DRAW_VERTEX_BUFFER_STRIDE_TOO_SMALL,
					// Add more message IDs here as needed
				};
				D3D11_INFO_QUEUE_FILTER filter;
				memset(&filter, 0, sizeof(filter));
				filter.DenyList.NumIDs = _countof(hide);
				filter.DenyList.pIDList = hide;
				d3dInfoQueue->AddStorageFilterEntries(&filter);
				d3dInfoQueue->Release();
			}
			d3dDebug->Release();
		}

		_BackBufferSurfaceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		RECT windowRect;
		GetClientRect(WindowHandle, &windowRect);

		_BackBufferSurfaceDesc.Width = windowRect.right - windowRect.left;
		_BackBufferSurfaceDesc.Height = windowRect.bottom - windowRect.top;

		// 4xMSAA: 4,4  |  8xCSAA:4,8  |  16xCSAA:4,16
#ifdef MSAA_SAMPLES
		_BackBufferSurfaceDesc.SampleDesc.Count = MSAA_SAMPLES;
		_BackBufferSurfaceDesc.SampleDesc.Quality = 0;
#else
		_BackBufferSurfaceDesc.SampleDesc.Count = 1;
		_BackBufferSurfaceDesc.SampleDesc.Quality = 0;
#endif


		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = _BackBufferSurfaceDesc.Width;
		sd.BufferDesc.Height = _BackBufferSurfaceDesc.Height;
		sd.BufferDesc.Format = _BackBufferSurfaceDesc.Format;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = WindowHandle;
		sd.SampleDesc.Count = _BackBufferSurfaceDesc.SampleDesc.Count;
		sd.SampleDesc.Quality = _BackBufferSurfaceDesc.SampleDesc.Quality;
		sd.Windowed = TRUE;

		hr = factory->CreateSwapChain(_Device, &sd, &_SwapChain);
		hr = _SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&_TexBackbuffer);
		hr = _Device->CreateRenderTargetView(_TexBackbuffer, NULL, &_RtvBackbuffer);

		// Create depth stencil texture	
		D3D11_TEXTURE2D_DESC descDepth;
		ZeroMemory(&descDepth, sizeof(D3D11_TEXTURE2D_DESC));
		descDepth.Width = _BackBufferSurfaceDesc.Width;
		descDepth.Height = _BackBufferSurfaceDesc.Height;
		descDepth.MipLevels = 1;
		descDepth.ArraySize = 1;
		descDepth.Format = DXGI_FORMAT_R32_TYPELESS;
		descDepth.SampleDesc.Count = _BackBufferSurfaceDesc.SampleDesc.Count;
		descDepth.SampleDesc.Quality = _BackBufferSurfaceDesc.SampleDesc.Quality;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;
		ID3D11Texture2D* pDSTexture;
		hr = _Device->CreateTexture2D(&descDepth, NULL, &pDSTexture);

		// Create the depth stencil view
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
		descDSV.Format = DXGI_FORMAT_D32_FLOAT;
		if (_BackBufferSurfaceDesc.SampleDesc.Count == 1 && _BackBufferSurfaceDesc.SampleDesc.Quality == 0)
			descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		else descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		descDSV.Texture2D.MipSlice = 0;
		hr = _Device->CreateDepthStencilView(pDSTexture,
			&descDSV,
			&_DsvBackbuffer);

		// Create a shader resource view on the depth buffer
		D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
		ZeroMemory(&resDesc, sizeof(resDesc));
		resDesc.Format = DXGI_FORMAT_R32_FLOAT;
		if (_BackBufferSurfaceDesc.SampleDesc.Count == 1 && _BackBufferSurfaceDesc.SampleDesc.Quality == 0)
			resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		else resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
		resDesc.Texture2D.MostDetailedMip = 0;
		resDesc.Texture2D.MipLevels = 1;
		hr = _Device->CreateShaderResourceView(pDSTexture, &resDesc, &_SrvDepthbuffer);
		
		if (pDSTexture) { pDSTexture->Release(); pDSTexture = NULL; }
		
		_ImmediateContext->OMSetRenderTargets(1, &_RtvBackbuffer, _DsvBackbuffer);

		// Setup the viewport    
		_FullViewport.Width = (FLOAT)_BackBufferSurfaceDesc.Width;
		_FullViewport.Height = (FLOAT)_BackBufferSurfaceDesc.Height;
		_FullViewport.MinDepth = 0.0f;
		_FullViewport.MaxDepth = 1.0f;
		_FullViewport.TopLeftX = 0;
		_FullViewport.TopLeftY = 0;
		_ImmediateContext->RSSetViewports(1, &_FullViewport);

		factory->Release();

		// -----------------------------------------
		D3D11_BLEND_DESC bsDefault;
		ZeroMemory(&bsDefault, sizeof(D3D11_BLEND_DESC));
		bsDefault.AlphaToCoverageEnable = false;
		bsDefault.IndependentBlendEnable = false;
		for (int i = 0; i < 8; ++i)
		{
			bsDefault.RenderTarget[i].BlendEnable = false;
			bsDefault.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
			bsDefault.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bsDefault.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
			bsDefault.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
			bsDefault.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			bsDefault.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
			bsDefault.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
		}
		_Device->CreateBlendState(&bsDefault, &_BsDefault);

		// -----------------------------------------
		D3D11_BLEND_DESC bsBlend;
		ZeroMemory(&bsBlend, sizeof(D3D11_BLEND_DESC));
		bsBlend.AlphaToCoverageEnable = false;
		bsBlend.IndependentBlendEnable = false;
		for (int i = 0; i < 8; ++i)
		{
			bsBlend.RenderTarget[i].BlendEnable = true;
			bsBlend.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
			bsBlend.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bsBlend.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			bsBlend.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			bsBlend.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			bsBlend.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			bsBlend.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		}
		_Device->CreateBlendState(&bsBlend, &_BsBlendBackToFront);

		// -----------------------------------------
		D3D11_DEPTH_STENCIL_DESC dsTestWriteOff;
		ZeroMemory(&dsTestWriteOff, sizeof(D3D11_DEPTH_STENCIL_DESC));
		dsTestWriteOff.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		dsTestWriteOff.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsTestWriteOff.DepthEnable = false;
		dsTestWriteOff.StencilEnable = false;
		_Device->CreateDepthStencilState(&dsTestWriteOff, &_DsTestWriteOff);

		// -----------------------------------------
		D3D11_RASTERIZER_DESC rsDesc;
		ZeroMemory(&rsDesc, sizeof(D3D11_RASTERIZER_DESC));
		rsDesc.CullMode = D3D11_CULL_NONE;
		rsDesc.DepthBias = 0;
		rsDesc.DepthBiasClamp = 0;
		rsDesc.FillMode = D3D11_FILL_SOLID;
		rsDesc.AntialiasedLineEnable = false;
		rsDesc.DepthClipEnable = true;
		rsDesc.FrontCounterClockwise = true;
#ifdef MSAA_SAMPLES
		rsDesc.MultisampleEnable = true;
#else
		rsDesc.MultisampleEnable = false;
#endif
		rsDesc.ScissorEnable = false;
		rsDesc.SlopeScaledDepthBias = 0;
		_Device->CreateRasterizerState(&rsDesc, &_RsCullNone);
	}

	// Destructor. Releases the resources.
	~D3D()
	{
		if (_Device) _Device->Release(); _Device = NULL;
		if (_ImmediateContext) _ImmediateContext->Release(); _ImmediateContext = NULL;
		if (_SwapChain) _SwapChain->Release(); _SwapChain = NULL;
		if (_RtvBackbuffer) _RtvBackbuffer->Release(); _RtvBackbuffer = NULL;
		if (_TexBackbuffer) _TexBackbuffer->Release(); _TexBackbuffer = NULL;
		if (_DsvBackbuffer) _DsvBackbuffer->Release(); _DsvBackbuffer = NULL;
		if (_SrvDepthbuffer) _SrvDepthbuffer->Release(); _SrvDepthbuffer = NULL;

		if (_BsDefault)				_BsDefault->Release();			_BsDefault = NULL;
		if (_BsBlendBackToFront)	_BsBlendBackToFront->Release();	_BsBlendBackToFront = NULL;
		if (_DsTestWriteOff)		_DsTestWriteOff->Release();		_DsTestWriteOff = NULL;
		if (_RsCullNone)			_RsCullNone->Release();			_RsCullNone = NULL;
	}

	// Utility function that loads a compute shader from file
	static bool LoadComputeShaderFromFile(const char* path, ID3D11Device* device, ID3D11ComputeShader** outShader)
	{
		unsigned int size;
		char* memblock = NULL;
		std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size = (unsigned int)file.tellg();
			memblock = new char[size];
			file.seekg(0, std::ios::beg);
			file.read(memblock, size);
			file.close();

			if (FAILED(device->CreateComputeShader(memblock, size, NULL, outShader))) {
				delete[] memblock;
				return false;
			}
			delete[] memblock;
		}
		return true;
	}

	// Utility function that loads a vertex shader from file. IMPORTANT: outBlob is a char array that has to be destructed by the calling function!
	static bool LoadVertexShaderFromFile(const char* path, ID3D11Device* device, ID3D11VertexShader** outShader, char** outBlob, UINT* outBlobSize)
	{
		*outBlob = NULL;
		*outBlobSize = 0;
		unsigned int size;
		char* memblock = NULL;
		std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size = (unsigned int)file.tellg();
			memblock = new char[size];
			file.seekg(0, std::ios::beg);
			file.read(memblock, size);
			file.close();

			if (FAILED(device->CreateVertexShader(memblock, size, NULL, outShader))) {
				return false;
			}
		}
		*outBlob = memblock;
		*outBlobSize = size;
		return true;
	}

	// Utility function that loads a geometry shader from file
	static bool LoadGeometryShaderFromFile(const char* path, ID3D11Device* device, ID3D11GeometryShader** outShader)
	{
		unsigned int size;
		char* memblock = NULL;
		std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size = (unsigned int)file.tellg();
			memblock = new char[size];
			file.seekg(0, std::ios::beg);
			file.read(memblock, size);
			file.close();

			if (FAILED(device->CreateGeometryShader(memblock, size, NULL, outShader))) {
				delete[] memblock;
				return false;
			}
			delete[] memblock;
		}
		return true;
	}

	// Utility function that loads a pixel shader from file
	static bool LoadPixelShaderFromFile(const char* path, ID3D11Device* device, ID3D11PixelShader** outShader)
	{
		unsigned int size;
		char* memblock = NULL;
		std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size = (unsigned int)file.tellg();
			memblock = new char[size];
			file.seekg(0, std::ios::beg);
			file.read(memblock, size);
			file.close();

			if (FAILED(device->CreatePixelShader(memblock, size, NULL, outShader))) {
				delete[] memblock;
				return false;
			}
			delete[] memblock;
		}
		return true;
	}

	// Gets the Direct3D device.
	ID3D11Device* GetDevice() { return _Device; }
	// Gets the immediate context.
	ID3D11DeviceContext* GetImmediateContext() { return _ImmediateContext; }

	ID3D11RenderTargetView* GetRtvBackbuffer() { return _RtvBackbuffer; }
	ID3D11DepthStencilView* GetDsvBackbuffer() { return _DsvBackbuffer; }
	ID3D11ShaderResourceView* GetSrvDepthbuffer() { return _SrvDepthbuffer; }
	const DXGI_SURFACE_DESC& GetBackBufferSurfaceDesc() const { return _BackBufferSurfaceDesc; }
	IDXGISwapChain* GetSwapChain() { return _SwapChain; }
	const D3D11_VIEWPORT& GetFullViewport() const { return _FullViewport; }


	// Gets the default blend state. (Blending disabled)
	ID3D11BlendState* GetBsDefault() { return _BsDefault; }
	// Gets a blend state that enables interpolation by source alpha.
	ID3D11BlendState* GetBsBlendBackToFront() { return _BsBlendBackToFront; }
	// Depth testing and writing disabled.
	ID3D11DepthStencilState* GetDsTestWriteOff() { return _DsTestWriteOff; }
	// Gets the state that disables face culling.
	ID3D11RasterizerState* GetRsCullNone() { return _RsCullNone; }

private:

	ID3D11Device* _Device;						// Direct3D device
	ID3D11DeviceContext* _ImmediateContext;		// Immediate context

	DXGI_SURFACE_DESC		_BackBufferSurfaceDesc;
	IDXGISwapChain*			_SwapChain;
	ID3D11RenderTargetView* _RtvBackbuffer;
	ID3D11Texture2D*		_TexBackbuffer;
	ID3D11DepthStencilView* _DsvBackbuffer;
	ID3D11ShaderResourceView* _SrvDepthbuffer;
	D3D11_VIEWPORT			_FullViewport;

	
	ID3D11BlendState* _BsDefault;				// No blending.
	ID3D11BlendState* _BsBlendBackToFront;		// Interpolation by source alpha.										
	ID3D11DepthStencilState* _DsTestWriteOff;	// Depth testing and writing disabled.
	ID3D11RasterizerState* _RsCullNone;			// No face culling.
};

#endif