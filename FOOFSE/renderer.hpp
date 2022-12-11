#pragma once

#include <d3d11.h>
#include "d3d.hpp"
#include "camera.hpp"
#include "lines.hpp"
#include "cbuffer.hpp"

class Renderer
{
	public:

		// The fragment linked lists draw fragments from a
		// memory pool. The size of the pool is determined by
		// an expected average overdraw rate.
		static const int EXPECTED_OVERDRAW_IN_LINKED_LISTS = 8;

		struct FragmentData	{
			unsigned int Color;		// Pixel color
			unsigned int Depth;		// Depth
			unsigned int Coverage;	// Coverage
			static int GetSizeInBytes() { return sizeof(unsigned int) * 3; }
		};

		struct FragmentDataLowRes {
			unsigned int Depth;		// Depth
			float AlphaWeight;					// Alpha weight
			float Importance;
			static int GetSizeInBytes() { return sizeof(unsigned int) * 1 + sizeof(float) * 2; }
		};

		struct FragmentLink {
			FragmentData FragmentData;	// Fragment data
			unsigned int Next;			// Link to next fragment
			static int GetSizeInBytes() { return sizeof(unsigned int) * 4; }
		};

		struct FragmentLinkLowRes {
			FragmentDataLowRes FragmentData;	// Fragment data
			unsigned int Next;					// Link to next fragment

			static int GetSizeInBytes() { return sizeof(unsigned int) * 4; }
		};

		struct CbFadeToAlpha
		{
			CbFadeToAlpha() : FadeToAlpha(0.1f), LaplaceWeight(0.1f) {}
			float FadeToAlpha;
			float LaplaceWeight;
		};

		struct CbRenderer
		{
			CbRenderer() : Q(0), R(0), Lambda(1), TotalNumberOfControlPoints(1),
				LineColor(1, 163.0f / 255.0f, 0, 1),
				HaloColor(0, 0, 0, 1),
				StripWidth(0.00015f),
				HaloPortion(0.7f),
				ScreenWidth(100), ScreenHeight(100) {}
			float Q;
			float R;
			float Lambda;
			int TotalNumberOfControlPoints;
			XMFLOAT4 LineColor;
			XMFLOAT4 HaloColor;
			float StripWidth;
			float HaloPortion;
			int ScreenWidth;
			int ScreenHeight;
		};

		struct FourierCoef
		{
			XMFLOAT4 fFourierA;
			XMFLOAT4 fFourierB;
			static int GetSizeInBytes() { return sizeof(XMFLOAT4) * 2; };
		};

		Renderer(float q, float r, float lambda, float stripWidth, int smoothingIterations) : 
			_StartOffsetBuffer(NULL),
			_FragmentLinkBuffer(NULL),
			_VbViewportQuad(NULL),
			_UavStartOffsetBuffer(NULL),
			_UavFragmentLinkBuffer(NULL),
			_StartOffsetBufferLowRes(NULL),
			_FragmentLinkBufferLowRes(NULL),
			_FourierCoefBuffer(NULL),
			_UavStartOffsetBufferLowRes(NULL),
			_UavFragmentLinkBufferLowRes(NULL),
			_UavFourierCoef(NULL),
			_VsLineShader_HQ(NULL),
			_VsLineShader_LowRes(NULL),
			_VsSortFragments(NULL),
			_VsSortFragments_LowRes(NULL),
			_VsMinGather_LowRes(NULL),
			_VsRenderFragments(NULL),
			_GsLineShader_HQ(NULL),
			_GsLineShader_LowRes(NULL),
			_PsLineShader_HQ(NULL),
			_PsLineShader_LowRes(NULL),
			_PsSortFragments(NULL),
			_PsSortFragments_LowRes(NULL),
			_PsMinGather_LowRes(NULL),
			_PsRenderFragments(NULL),
			_InputLayout_Line_HQ(NULL),
			_InputLayout_Line_LowRes(NULL),
			_InputLayout_ViewportQuad(NULL),
			_CsFadeAlpha(NULL),
			_CsSmoothAlpha(NULL),
			_VsLineShaderFOM(NULL),
			_PsLineShaderFOM(NULL),
			_GsLineShaderFOM(NULL),
			_VsMinGatherFOM(NULL),
			_PsMinGatherFOM(NULL),
			_ResolutionDownScale(1),
			_SmoothingIterations(smoothingIterations)
		{
			_CbRenderer.Data.Q = q;
			_CbRenderer.Data.R = r;
			_CbRenderer.Data.Lambda = lambda;
			_CbRenderer.Data.StripWidth = stripWidth;
		}
		~Renderer()
		{
			D3DReleaseDevice();
			D3DReleaseSwapChain();
		}

		bool D3DCreateDevice(ID3D11Device* Device)
		{
			char* blobLineShader_HQ, *blobLineShader_LowRes, *blobMinGather_LowRes, *blobRenderFragments, *blobSortFragments, *blobSortFragmentsLowRes, *blobLineShaderFOM, *blobMinGatherFOM;
			UINT sizeLineShader_HQ, sizeLineShader_LowRes, sizeMinGather_LowRes, sizeRenderFragments, sizeSortFragments, sizeSortFragmentsLowRes, sizeLineShaderFOM, sizeMinGatherFOM;
			if (!D3D::LoadVertexShaderFromFile("shader_CreateLists_HQ.vso", Device, &_VsLineShader_HQ, &blobLineShader_HQ, &sizeLineShader_HQ)) return false;
			if (!D3D::LoadVertexShaderFromFile("shader_CreateLists_LowRes.vso", Device, &_VsLineShader_LowRes, &blobLineShader_LowRes, &sizeLineShader_LowRes)) return false;
			if (!D3D::LoadVertexShaderFromFile("shader_MinGather_LowRes.vso", Device, &_VsMinGather_LowRes, &blobMinGather_LowRes, &sizeMinGather_LowRes)) return false;
			if (!D3D::LoadVertexShaderFromFile("shader_RenderFragments.vso", Device, &_VsRenderFragments, &blobRenderFragments, &sizeRenderFragments)) return false;
			if (!D3D::LoadVertexShaderFromFile("shader_SortFragments.vso", Device, &_VsSortFragments, &blobSortFragments, &sizeSortFragments)) return false;
			if (!D3D::LoadVertexShaderFromFile("shader_SortFragments_LowRes.vso", Device, &_VsSortFragments_LowRes, &blobSortFragmentsLowRes, &sizeSortFragmentsLowRes)) return false;

			if (!D3D::LoadGeometryShaderFromFile("shader_CreateLists_HQ.gso", Device, &_GsLineShader_HQ)) return false;
			if (!D3D::LoadGeometryShaderFromFile("shader_CreateLists_LowRes.gso", Device, &_GsLineShader_LowRes)) return false;

			if (!D3D::LoadPixelShaderFromFile("shader_CreateLists_HQ.pso", Device, &_PsLineShader_HQ)) return false;
			if (!D3D::LoadPixelShaderFromFile("shader_CreateLists_LowRes.pso", Device, &_PsLineShader_LowRes)) return false;
			if (!D3D::LoadPixelShaderFromFile("shader_MinGather_LowRes.pso", Device, &_PsMinGather_LowRes)) return false;
			if (!D3D::LoadPixelShaderFromFile("shader_RenderFragments.pso", Device, &_PsRenderFragments)) return false;
			if (!D3D::LoadPixelShaderFromFile("shader_SortFragments.pso", Device, &_PsSortFragments)) return false;
			if (!D3D::LoadPixelShaderFromFile("shader_SortFragments_LowRes.pso", Device, &_PsSortFragments_LowRes)) return false;

			if (!D3D::LoadComputeShaderFromFile("shader_FadeToAlphaPerVertex.cso", Device, &_CsFadeAlpha)) return false;
			if (!D3D::LoadComputeShaderFromFile("shader_SmoothAlpha.cso", Device, &_CsSmoothAlpha)) return false;
			
			// FOM shader
			if (!D3D::LoadVertexShaderFromFile("shader_CreateLists_LowRes_FOM.vso", Device, &_VsLineShaderFOM, &blobLineShaderFOM, &sizeLineShaderFOM)) return false;
			if (!D3D::LoadPixelShaderFromFile("shader_CreateLists_LowRes_FOM.pso", Device, &_PsLineShaderFOM)) return false;
			if (!D3D::LoadGeometryShaderFromFile("shader_CreateLists_LowRes_FOM.gso", Device, &_GsLineShaderFOM)) return false;

			if (!D3D::LoadVertexShaderFromFile("shader_MinGather_FOM.vso", Device, &_VsMinGatherFOM, &blobMinGatherFOM, &sizeMinGatherFOM)) return false;
			if (!D3D::LoadPixelShaderFromFile("shader_MinGather_FOM.pso", Device, &_PsMinGatherFOM)) return false;


			// Create input layout
			{
				const D3D11_INPUT_ELEMENT_DESC layout[] =
				{
					{ "POSITIONA",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "POSITIONB",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(XMFLOAT3) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IDA",  0, DXGI_FORMAT_R32_SINT, 1, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IDB",  0, DXGI_FORMAT_R32_SINT, 1, sizeof(int) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "ALPHAA",  0, DXGI_FORMAT_R32_FLOAT, 2, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "ALPHAB",  0, DXGI_FORMAT_R32_FLOAT, 2, sizeof(float) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
				};
				HRESULT hr = Device->CreateInputLayout(layout, 6, blobLineShader_HQ, sizeLineShader_HQ, &_InputLayout_Line_HQ);
				if (FAILED(hr))	return false;
			}

			{
				const D3D11_INPUT_ELEMENT_DESC layout[] =
				{
					{ "POSITIONA",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "POSITIONB",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(XMFLOAT3) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IDA",  0, DXGI_FORMAT_R32_SINT, 1, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IDB",  0, DXGI_FORMAT_R32_SINT, 1, sizeof(int) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IMPORTANCEA",  0, DXGI_FORMAT_R32_FLOAT, 2, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IMPORTANCEB",  0, DXGI_FORMAT_R32_FLOAT, 2, sizeof(float) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "ALPHAWEIGHTA",  0, DXGI_FORMAT_R32_FLOAT, 3, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "ALPHAWEIGHTB",  0, DXGI_FORMAT_R32_FLOAT, 3, sizeof(float) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
				};
				HRESULT hr = Device->CreateInputLayout(layout, 8, blobLineShader_LowRes, sizeLineShader_LowRes, &_InputLayout_Line_LowRes);
				if (FAILED(hr))	return false;
			}

			{
				const D3D11_INPUT_ELEMENT_DESC layout[] =
				{
					{ "POSITIONA",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "POSITIONB",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(XMFLOAT3) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IDA",  0, DXGI_FORMAT_R32_SINT, 1, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IDB",  0, DXGI_FORMAT_R32_SINT, 1, sizeof(int) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IMPORTANCEA",  0, DXGI_FORMAT_R32_FLOAT, 2, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "IMPORTANCEB",  0, DXGI_FORMAT_R32_FLOAT, 2, sizeof(float) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "ALPHAWEIGHTA",  0, DXGI_FORMAT_R32_FLOAT, 3, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "ALPHAWEIGHTB",  0, DXGI_FORMAT_R32_FLOAT, 3, sizeof(float) * 2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
				};
				HRESULT hr = Device->CreateInputLayout(layout, 8, blobLineShaderFOM, sizeLineShaderFOM, &_InputLayout_Line_LowRes);
				if (FAILED(hr))	return false;
			}

			{
				const D3D11_INPUT_ELEMENT_DESC layout[] =
				{
					{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
				};
				HRESULT hr = Device->CreateInputLayout(layout, 1, blobSortFragments, sizeSortFragments, &_InputLayout_ViewportQuad);
				if (FAILED(hr))	return false;
			}

			{
				D3D11_BUFFER_DESC bufDesc;
				ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
				bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				bufDesc.ByteWidth = 6 * sizeof(XMFLOAT3);
				bufDesc.CPUAccessFlags = 0;
				bufDesc.MiscFlags = 0;
				bufDesc.StructureByteStride = sizeof(XMFLOAT3);
				bufDesc.Usage = D3D11_USAGE_DEFAULT;

				XMFLOAT3 positions[] = {
					XMFLOAT3(-1, -1, 0),
					XMFLOAT3(-1,  1, 0),
					XMFLOAT3(1, -1, 0),
					XMFLOAT3(-1,  1, 0),
					XMFLOAT3(1,  1, 0),
					XMFLOAT3(1, -1, 0),
				};
				D3D11_SUBRESOURCE_DATA initData;
				ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
				initData.pSysMem = positions;

				if (FAILED(Device->CreateBuffer(&bufDesc, &initData, &_VbViewportQuad))) return false;
			}

			if (!_CbFadeToAlpha.Create(Device)) return false;
			if (!_CbRenderer.Create(Device)) return false;

			delete[] blobLineShader_HQ;
			delete[] blobLineShader_LowRes;
			delete[] blobMinGather_LowRes;
			delete[] blobRenderFragments;
			delete[] blobSortFragments;
			delete[] blobSortFragmentsLowRes;
			delete[] blobLineShaderFOM;
			delete[] blobMinGatherFOM;

			return true;
		}

		bool D3DCreateSwapChain(ID3D11Device* Device, const DXGI_SURFACE_DESC* BackBufferSurfaceDesc)
		{
			// --- create start offset
			{
				unsigned int NUM_ELEMENTS = BackBufferSurfaceDesc->Width * BackBufferSurfaceDesc->Height;
				D3D11_BUFFER_DESC bufDesc;
				ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
				bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
				bufDesc.ByteWidth = NUM_ELEMENTS * sizeof(unsigned int);
				bufDesc.CPUAccessFlags = 0;
				bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
				bufDesc.StructureByteStride = sizeof(unsigned int);
				bufDesc.Usage = D3D11_USAGE_DEFAULT;
				if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &_StartOffsetBuffer))) return false;

				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				ZeroMemory(&uavDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
				uavDesc.Buffer.FirstElement = 0;
				uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
				uavDesc.Buffer.NumElements = NUM_ELEMENTS;
				uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
				if (FAILED(Device->CreateUnorderedAccessView(_StartOffsetBuffer, &uavDesc, &_UavStartOffsetBuffer))) return false;
			}

			{
				unsigned int NUM_ELEMENTS = BackBufferSurfaceDesc->Width * BackBufferSurfaceDesc->Height / (_ResolutionDownScale*_ResolutionDownScale);
				D3D11_BUFFER_DESC bufDesc;
				ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
				bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
				bufDesc.ByteWidth = NUM_ELEMENTS * sizeof(unsigned int);
				bufDesc.CPUAccessFlags = 0;
				bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
				bufDesc.StructureByteStride = sizeof(unsigned int);
				bufDesc.Usage = D3D11_USAGE_DEFAULT;
				if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &_StartOffsetBufferLowRes))) return false;

				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				ZeroMemory(&uavDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
				uavDesc.Buffer.FirstElement = 0;
				uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
				uavDesc.Buffer.NumElements = NUM_ELEMENTS;
				uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
				if (FAILED(Device->CreateUnorderedAccessView(_StartOffsetBufferLowRes, &uavDesc, &_UavStartOffsetBufferLowRes))) return false;
			}

			// --- create fragment link list 
			{
				unsigned int NUM_ELEMENTS = BackBufferSurfaceDesc->Width * BackBufferSurfaceDesc->Height;

				NUM_ELEMENTS *= EXPECTED_OVERDRAW_IN_LINKED_LISTS;	// approximately EXPECTED_OVERDRAW_IN_LINKED_LISTS entries per pixel on average

				D3D11_BUFFER_DESC bufDesc;
				ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
				bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
				bufDesc.ByteWidth = NUM_ELEMENTS * FragmentLink::GetSizeInBytes();
				bufDesc.CPUAccessFlags = 0;
				bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
				bufDesc.StructureByteStride = FragmentLink::GetSizeInBytes();
				bufDesc.Usage = D3D11_USAGE_DEFAULT;
				if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &_FragmentLinkBuffer))) return false;

				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				ZeroMemory(&uavDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
				uavDesc.Buffer.FirstElement = 0;
				uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
				uavDesc.Buffer.NumElements = NUM_ELEMENTS;
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
				if (FAILED(Device->CreateUnorderedAccessView(_FragmentLinkBuffer, &uavDesc, &_UavFragmentLinkBuffer))) return false;
			}

			{
				unsigned int NUM_ELEMENTS = BackBufferSurfaceDesc->Width * BackBufferSurfaceDesc->Height / (_ResolutionDownScale*_ResolutionDownScale);

				NUM_ELEMENTS *= EXPECTED_OVERDRAW_IN_LINKED_LISTS;	// approximately EXPECTED_OVERDRAW_IN_LINKED_LISTS entries per pixel on average

				D3D11_BUFFER_DESC bufDesc;
				ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
				bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
				bufDesc.ByteWidth = NUM_ELEMENTS * FragmentLinkLowRes::GetSizeInBytes();
				bufDesc.CPUAccessFlags = 0;
				bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
				bufDesc.StructureByteStride = FragmentLinkLowRes::GetSizeInBytes();
				bufDesc.Usage = D3D11_USAGE_DEFAULT;
				if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &_FragmentLinkBufferLowRes))) return false;

				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				ZeroMemory(&uavDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
				uavDesc.Buffer.FirstElement = 0;
				uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
				uavDesc.Buffer.NumElements = NUM_ELEMENTS;
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
				if (FAILED(Device->CreateUnorderedAccessView(_FragmentLinkBufferLowRes, &uavDesc, &_UavFragmentLinkBufferLowRes))) return false;
			}

			// --- create Fourier Coefs
			{
				unsigned int NUM_ELEMENTS = BackBufferSurfaceDesc->Width * BackBufferSurfaceDesc->Height;
				D3D11_BUFFER_DESC bufDesc;
				ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
				bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
				bufDesc.ByteWidth = NUM_ELEMENTS * FourierCoef::GetSizeInBytes();
				bufDesc.CPUAccessFlags = 0;
				bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
				bufDesc.StructureByteStride = FourierCoef::GetSizeInBytes();
				bufDesc.Usage = D3D11_USAGE_DEFAULT;
				if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &_FourierCoefBuffer))) return false;

				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				ZeroMemory(&uavDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
				uavDesc.Buffer.FirstElement = 0;
				uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
				uavDesc.Buffer.NumElements = NUM_ELEMENTS;
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
				if (FAILED(Device->CreateUnorderedAccessView(_FourierCoefBuffer, &uavDesc, &_UavFourierCoef))) return false;
			}

			return true;
		}

		void D3DReleaseDevice()
		{
			if (_InputLayout_Line_HQ)		_InputLayout_Line_HQ->Release();		_InputLayout_Line_HQ = NULL;
			if (_InputLayout_Line_LowRes)	_InputLayout_Line_LowRes->Release();	_InputLayout_Line_LowRes = NULL;
			if (_InputLayout_ViewportQuad)	_InputLayout_ViewportQuad->Release();	_InputLayout_ViewportQuad = NULL;
			if (_VsLineShader_HQ)			_VsLineShader_HQ->Release();			_VsLineShader_HQ = NULL;
			if (_VsLineShader_LowRes)		_VsLineShader_LowRes->Release();		_VsLineShader_LowRes = NULL;
			if (_VsSortFragments)			_VsSortFragments->Release();			_VsSortFragments = NULL;
			if (_VsSortFragments_LowRes)	_VsSortFragments_LowRes->Release();		_VsSortFragments_LowRes = NULL;
			if (_VsRenderFragments)			_VsRenderFragments->Release();			_VsRenderFragments = NULL;
			if (_VsMinGather_LowRes)		_VsMinGather_LowRes->Release();			_VsMinGather_LowRes = NULL;
			if (_GsLineShader_HQ)			_GsLineShader_HQ->Release();			_GsLineShader_HQ = NULL;
			if (_GsLineShader_LowRes)		_GsLineShader_LowRes->Release();		_GsLineShader_LowRes = NULL;
			if (_PsLineShader_HQ)			_PsLineShader_HQ->Release();			_PsLineShader_HQ = NULL;
			if (_PsLineShader_LowRes)		_PsLineShader_LowRes->Release();		_PsLineShader_LowRes = NULL;
			if (_PsSortFragments)			_PsSortFragments->Release();			_PsSortFragments = NULL;
			if (_PsSortFragments_LowRes)	_PsSortFragments_LowRes->Release();		_PsSortFragments_LowRes = NULL;
			if (_PsRenderFragments)			_PsRenderFragments->Release();			_PsRenderFragments = NULL;
			if (_PsMinGather_LowRes)		_PsMinGather_LowRes->Release();			_PsMinGather_LowRes = NULL;
			if (_CsFadeAlpha)				_CsFadeAlpha->Release();				_CsFadeAlpha = NULL;
			if (_CsSmoothAlpha)				_CsSmoothAlpha->Release();				_CsSmoothAlpha = NULL;
			if (_VbViewportQuad)			_VbViewportQuad->Release();				_VbViewportQuad = NULL;
			
			// FOM
			if (_VsLineShaderFOM)			_VsLineShaderFOM->Release();			_VsLineShaderFOM = NULL;
			if (_PsLineShaderFOM)			_PsLineShaderFOM->Release();			_PsLineShaderFOM = NULL;
			if (_GsLineShaderFOM)			_GsLineShaderFOM->Release();			_GsLineShaderFOM = NULL;

			if (_VsMinGatherFOM)			_VsMinGatherFOM->Release();				_VsMinGatherFOM = NULL;
			if (_PsMinGatherFOM)			_PsMinGatherFOM->Release();				_PsMinGatherFOM = NULL;

			_CbFadeToAlpha.Release();
			_CbRenderer.Release();
		}

		void D3DReleaseSwapChain()
		{
			if (_StartOffsetBuffer)				_StartOffsetBuffer->Release();				_StartOffsetBuffer = NULL;
			if (_UavStartOffsetBuffer)			_UavStartOffsetBuffer->Release();			_UavStartOffsetBuffer = NULL;
			if (_FragmentLinkBuffer)			_FragmentLinkBuffer->Release();				_FragmentLinkBuffer = NULL;
			if (_UavFragmentLinkBuffer)			_UavFragmentLinkBuffer->Release();			_UavFragmentLinkBuffer = NULL;
			if (_StartOffsetBufferLowRes)		_StartOffsetBufferLowRes->Release();		_StartOffsetBufferLowRes = NULL;
			if (_UavStartOffsetBufferLowRes)	_UavStartOffsetBufferLowRes->Release();		_UavStartOffsetBufferLowRes = NULL;
			if (_FragmentLinkBufferLowRes)		_FragmentLinkBufferLowRes->Release();		_FragmentLinkBufferLowRes = NULL;
			if (_UavFragmentLinkBufferLowRes)	_UavFragmentLinkBufferLowRes->Release();	_UavFragmentLinkBufferLowRes = NULL;
			if (_FourierCoefBuffer)				_FourierCoefBuffer->Release();				_FourierCoefBuffer = NULL;
			if (_UavFourierCoef)				_UavFourierCoef->Release();					_UavFourierCoef = NULL;
		}

		void Draw(ID3D11DeviceContext* ImmediateContext, D3D* D3D, Lines* Geometry, Camera* Camera)
		{
			static int ping = 0;

			_CbFadeToAlpha.UpdateBuffer(ImmediateContext);

			Camera->GetParams().UpdateBuffer(ImmediateContext);

			_CbRenderer.Data.TotalNumberOfControlPoints = Geometry->GetTotalNumberOfControlPoints();
			_CbRenderer.Data.ScreenWidth = (int)D3D->GetBackBufferSurfaceDesc().Width;
			_CbRenderer.Data.ScreenHeight = (int)D3D->GetBackBufferSurfaceDesc().Height;
			_CbRenderer.UpdateBuffer(ImmediateContext);

			ID3D11Buffer* cbs[] = { Camera->GetParams().GetBuffer(), _CbRenderer.GetBuffer() };
			ImmediateContext->VSSetConstantBuffers(0, 2, cbs);
			ImmediateContext->GSSetConstantBuffers(0, 2, cbs);
			ImmediateContext->PSSetConstantBuffers(0, 2, cbs);

			ID3D11RenderTargetView* rtvs[] = { D3D->GetRtvBackbuffer() };
			float blendFactor[4] = { 1,1,1,1 };

			const D3D11_VIEWPORT& fullViewport = D3D->GetFullViewport();

			// switch to smaller viewport resolution
			D3D11_VIEWPORT smallViewport = fullViewport;
			smallViewport.Width /= _ResolutionDownScale;
			smallViewport.Height /= _ResolutionDownScale;
			ImmediateContext->RSSetViewports(1, &smallViewport);

			// -------------------------------------------
#pragma region Create fragment linked lists - low res
			{
				// Clear the start offset buffer by magic value.
				unsigned int clearStartOffset[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
				ImmediateContext->ClearUnorderedAccessViewUint(_UavStartOffsetBufferLowRes, clearStartOffset);

				// Bind states
				ImmediateContext->IASetInputLayout(_InputLayout_Line_LowRes);
				ID3D11RenderTargetView* rtvsNo[] = { NULL };
				ImmediateContext->OMSetRenderTargets(0, rtvsNo, NULL);
				float blendFactor[4] = { 1,1,1,1 };
				ImmediateContext->OMSetBlendState(D3D->GetBsDefault(), blendFactor, 0xffffffff);
				ImmediateContext->OMSetDepthStencilState(D3D->GetDsTestWriteOff(), 0);
				ImmediateContext->RSSetState(D3D->GetRsCullNone());

				//ImmediateContext->VSSetShader(_VsLineShader_LowRes, NULL, 0);
				//ImmediateContext->GSSetShader(_GsLineShader_LowRes, NULL, 0);
				//ImmediateContext->PSSetShader(_PsLineShader_LowRes, NULL, 0);

				ImmediateContext->VSSetShader(_VsLineShaderFOM, NULL, 0);
				ImmediateContext->GSSetShader(_GsLineShaderFOM, NULL, 0);
				ImmediateContext->PSSetShader(_PsLineShaderFOM, NULL, 0);

				ID3D11ShaderResourceView* srvs[] = { D3D->GetSrvDepthbuffer() };
				ImmediateContext->PSSetShaderResources(0, 1, srvs);

				ID3D11UnorderedAccessView* uavs[] = { _UavFragmentLinkBufferLowRes, _UavStartOffsetBufferLowRes, _UavFourierCoef };
				UINT initialCount[] = { 0,0,0,0,0 };
				// render target is only bound so that the rasterizer knows, how many samples we want. We won't render into it, yet.
				ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvsNo, NULL, 1, 3, uavs, initialCount);
				
				// Render
				Geometry->DrawLowRes(ImmediateContext);

				ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL };
				ImmediateContext->GSSetShaderResources(0, 1, noSrvs);
				ImmediateContext->PSSetShaderResources(0, 3, noSrvs);
			}
#pragma endregion
			// -------------------------------------------

			// -------------------------------------------
#pragma region Sort the fragments - low res
			{
				ImmediateContext->IASetInputLayout(_InputLayout_ViewportQuad);
				ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				ID3D11Buffer* vbs[] = { _VbViewportQuad };
				UINT strides[] = { sizeof(XMFLOAT3) };
				UINT offsets[] = { 0 };
				ImmediateContext->IASetVertexBuffers(0, 1, vbs, strides, offsets);

				ImmediateContext->VSSetShader(_VsSortFragments_LowRes, NULL, 0);
				ImmediateContext->GSSetShader(NULL, NULL, 0);
				ImmediateContext->PSSetShader(_PsSortFragments_LowRes, NULL, 0);

				ID3D11UnorderedAccessView* uavs[] = { _UavStartOffsetBufferLowRes, _UavFragmentLinkBufferLowRes };
				UINT initialCount[] = { 0,0 };
				ID3D11RenderTargetView* rtvsNo[] = { NULL };
				ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvsNo, NULL, 1, 2, uavs, initialCount);

				ImmediateContext->Draw(6, 0);
			}
#pragma endregion
			// -------------------------------------------

			// -------------------------------------------
#pragma region Min gather of alpha values
			// -------------------------------------------
			{
				ImmediateContext->OMSetBlendState(D3D->GetBsBlendBackToFront(), blendFactor, 0xffffffff);

				UINT maxValues[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
				ImmediateContext->ClearUnorderedAccessViewUint(Geometry->GetUavAlpha()[ping], maxValues);

				/*ImmediateContext->VSSetShader(_VsMinGather_LowRes, NULL, 0);
				ImmediateContext->GSSetShader(NULL, NULL, 0);
				ImmediateContext->PSSetShader(_PsMinGather_LowRes, NULL, 0);*/

				ImmediateContext->VSSetShader(_VsMinGatherFOM, NULL, 0);
				ImmediateContext->GSSetShader(NULL, NULL, 0);
				ImmediateContext->PSSetShader(_PsMinGatherFOM, NULL, 0);

				{
					ID3D11UnorderedAccessView* uavs[] = { _UavStartOffsetBufferLowRes, _UavFragmentLinkBufferLowRes, _UavFourierCoef, Geometry->GetUavAlpha()[ping] };
					UINT initialCount[] = { 0,0,0,0 };
					ID3D11RenderTargetView* rtvsNo[] = { NULL };
					ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvsNo, NULL, 1, 4, uavs, initialCount);
				}

				ImmediateContext->Draw(6, 0);

				{
					ID3D11UnorderedAccessView* uavs[] = { NULL, NULL, NULL, NULL };
					UINT initialCount[] = { 0,0,0,0 };
					ID3D11RenderTargetView* rtvsNo[] = { NULL };
					ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvsNo, NULL, 1, 4, uavs, initialCount);
				}
			}
#pragma endregion
			// -------------------------------------------

			// -------------------------------------------
#pragma region Smoothing
			{
				ImmediateContext->CSSetShader(_CsSmoothAlpha, NULL, 0);

				ID3D11Buffer* cbs[] = { _CbFadeToAlpha.GetBuffer() };
				ImmediateContext->CSSetConstantBuffers(0, 1, cbs);

				for (int s = 0; s< _SmoothingIterations; ++s)
				{
					float laplaceWeight = _CbFadeToAlpha.Data.LaplaceWeight;

					for (int ta = 0; ta < 2; ++ta)
					{
						if (ta == 0)
							_CbFadeToAlpha.Data.LaplaceWeight = laplaceWeight;
						else
							_CbFadeToAlpha.Data.LaplaceWeight = -laplaceWeight*1.01f;
						_CbFadeToAlpha.UpdateBuffer(ImmediateContext);

						if (ta == 1) continue;	// skip the shrinking..

						ID3D11ShaderResourceView* srvs[] = { Geometry->GetSrvAlpha()[ping], Geometry->GetSrvLineID() };
						ImmediateContext->CSSetShaderResources(0, 2, srvs);

						ID3D11UnorderedAccessView* uavs[] = { Geometry->GetUavAlpha()[1 - ping] };
						UINT initialCounts[] = { 0, 0, 0, 0 };
						ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

						UINT groupsX = Geometry->GetTotalNumberOfControlPoints();
						if (groupsX % (512) == 0)
							groupsX = groupsX / (512);
						else groupsX = groupsX / (512) + 1;
						ImmediateContext->Dispatch(groupsX, 1, 1);

						// clean up
						ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
						ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

						ID3D11UnorderedAccessView* noUavs[] = { NULL };
						ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);

						ping = 1 - ping;	// ping pong!
					}
					_CbFadeToAlpha.Data.LaplaceWeight = laplaceWeight;
				}

				ID3D11Buffer* noCbs[] = { NULL };
				ImmediateContext->CSSetConstantBuffers(0, 1, noCbs);
			}
#pragma endregion
			// -------------------------------------------

			// -------------------------------------------
#pragma region Fade the current alpha solution per vertex
			{
				ImmediateContext->CSSetShader(_CsFadeAlpha, NULL, 0);

				ID3D11ShaderResourceView* srvs[] = { Geometry->GetSrvAlpha()[ping], Geometry->GetSrvAlphaWeights() };
				ImmediateContext->CSSetShaderResources(0, 2, srvs);

				ID3D11UnorderedAccessView* uavs[] = { Geometry->GetUavCurrentAlpha() };
				UINT initialCounts[] = { 0,0,0,0 };
				ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

				ID3D11Buffer* cbs[] = { _CbFadeToAlpha.GetBuffer() };
				ImmediateContext->CSSetConstantBuffers(0, 1, cbs);

				UINT groupsX = Geometry->GetTotalNumberOfVertices();
				if (groupsX % (512) == 0)
					groupsX = groupsX / (512);
				else groupsX = groupsX / (512) + 1;
				ImmediateContext->Dispatch(groupsX, 1, 1);

				// clean up
				ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
				ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

				ID3D11UnorderedAccessView* noUavs[] = { NULL };
				ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);

				ID3D11Buffer* noCbs[] = { NULL };
				ImmediateContext->CSSetConstantBuffers(0, 1, noCbs);
			}
#pragma endregion
			// -------------------------------------------

			// switch to full viewport
			ImmediateContext->RSSetViewports(1, &fullViewport);

			// -------------------------------------------
#pragma region Create fragment linked list and render
			// -------------------------------------------
			{
				// Clear the start offset buffer by magic value.
				unsigned int clearStartOffset[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
				ImmediateContext->ClearUnorderedAccessViewUint(_UavStartOffsetBuffer, clearStartOffset);

				// Bind states
				ID3D11RenderTargetView* rtvsNo[] = { NULL };
				ImmediateContext->OMSetRenderTargets(0, rtvsNo, NULL);
				ImmediateContext->OMSetBlendState(D3D->GetBsDefault(), blendFactor, 0xffffffff);
				ImmediateContext->OMSetDepthStencilState(D3D->GetDsTestWriteOff(), 0);
				ImmediateContext->RSSetState(D3D->GetRsCullNone());

				ImmediateContext->IASetInputLayout(_InputLayout_Line_HQ);

				ImmediateContext->VSSetShader(_VsLineShader_HQ, NULL, 0);
				ImmediateContext->GSSetShader(_GsLineShader_HQ, NULL, 0);
				ImmediateContext->PSSetShader(_PsLineShader_HQ, NULL, 0);

				ID3D11ShaderResourceView* srvs[] = { D3D->GetSrvDepthbuffer() };
				ImmediateContext->PSSetShaderResources(0, 1, srvs);

				{
					ID3D11UnorderedAccessView* uavs[] = { _UavFragmentLinkBuffer, _UavStartOffsetBuffer };
					UINT initialCount[] = { 0,0 };
					// render target is only bound so that the rasterizer knows, how many samples we want. We won't render into it, yet.
					ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, NULL, 1, 2, uavs, initialCount);
				}

				// Render
				Geometry->DrawHQ(ImmediateContext);

				ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL, NULL };
				ImmediateContext->GSSetShaderResources(0, 4, noSrvs);
				ImmediateContext->PSSetShaderResources(0, 4, noSrvs);
			}
#pragma endregion
			// -------------------------------------------

			// -------------------------------------------
#pragma region Sort the fragments
			// -------------------------------------------
			{
				ImmediateContext->IASetInputLayout(_InputLayout_ViewportQuad);
				ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				ID3D11Buffer* vbs[] = { _VbViewportQuad, NULL, NULL, NULL };
				UINT strides[] = { sizeof(XMFLOAT3), 0, 0, 0 };
				UINT offsets[] = { 0, 0, 0, 0 };
				ImmediateContext->IASetVertexBuffers(0, 4, vbs, strides, offsets);

				ImmediateContext->VSSetShader(_VsSortFragments, NULL, 0);
				ImmediateContext->GSSetShader(NULL, NULL, 0);
				ImmediateContext->PSSetShader(_PsSortFragments, NULL, 0);

				{
					ID3D11UnorderedAccessView* uavs[] = { _UavStartOffsetBuffer, _UavFragmentLinkBuffer };
					UINT initialCount[] = { 0,0 };
					ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, D3D->GetDsvBackbuffer(), 1, 2, uavs, initialCount);
				}

				ImmediateContext->Draw(6, 0);
			}
#pragma endregion
			// -------------------------------------------

			// -------------------------------------------
#pragma region Render the fragments
			// -------------------------------------------
			{
				ImmediateContext->OMSetBlendState(D3D->GetBsBlendBackToFront(), blendFactor, 0xffffffff);

				ImmediateContext->VSSetShader(_VsRenderFragments, NULL, 0);
				ImmediateContext->GSSetShader(NULL, NULL, 0);
				ImmediateContext->PSSetShader(_PsRenderFragments, NULL, 0);

				{
					ID3D11UnorderedAccessView* uavs[] = { _UavStartOffsetBuffer, _UavFragmentLinkBuffer };
					UINT initialCount[] = { 0,0 };
					ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, D3D->GetDsvBackbuffer(), 1, 2, uavs, initialCount);
				}

				ImmediateContext->Draw(6, 0);

				ImmediateContext->OMSetRenderTargets(1, rtvs, D3D->GetDsvBackbuffer());
			}
#pragma endregion
			// -------------------------------------------
		}

	private:

		ID3D11Buffer* _StartOffsetBuffer;
		ID3D11Buffer* _FragmentLinkBuffer;
		ID3D11Buffer* _VbViewportQuad;
		ID3D11UnorderedAccessView* _UavStartOffsetBuffer;
		ID3D11UnorderedAccessView* _UavFragmentLinkBuffer;

		ID3D11Buffer* _StartOffsetBufferLowRes;
		ID3D11Buffer* _FragmentLinkBufferLowRes;
		ID3D11Buffer* _FourierCoefBuffer;
		ID3D11UnorderedAccessView* _UavStartOffsetBufferLowRes;
		ID3D11UnorderedAccessView* _UavFragmentLinkBufferLowRes;
		ID3D11UnorderedAccessView* _UavFourierCoef;

		ID3D11VertexShader* _VsLineShader_HQ;
		ID3D11VertexShader* _VsLineShader_LowRes;
		ID3D11VertexShader* _VsSortFragments;
		ID3D11VertexShader* _VsSortFragments_LowRes;
		ID3D11VertexShader* _VsMinGather_LowRes;
		ID3D11VertexShader* _VsRenderFragments;

		ID3D11GeometryShader* _GsLineShader_HQ;
		ID3D11GeometryShader* _GsLineShader_LowRes;
		
		ID3D11PixelShader* _PsLineShader_HQ;
		ID3D11PixelShader* _PsLineShader_LowRes;
		ID3D11PixelShader* _PsSortFragments;
		ID3D11PixelShader* _PsSortFragments_LowRes;
		ID3D11PixelShader* _PsMinGather_LowRes;
		ID3D11PixelShader* _PsRenderFragments;

		ID3D11InputLayout* _InputLayout_Line_HQ;
		ID3D11InputLayout* _InputLayout_Line_LowRes;
		ID3D11InputLayout* _InputLayout_ViewportQuad;

		ID3D11ComputeShader* _CsFadeAlpha;
		ID3D11ComputeShader* _CsSmoothAlpha;
		ConstantBuffer<CbFadeToAlpha> _CbFadeToAlpha;
		ConstantBuffer<CbRenderer> _CbRenderer;

		// FOM shader
		ID3D11VertexShader* _VsLineShaderFOM;
		ID3D11PixelShader* _PsLineShaderFOM;
		ID3D11GeometryShader* _GsLineShaderFOM;

		ID3D11VertexShader* _VsMinGatherFOM;
		ID3D11PixelShader* _PsMinGatherFOM;

		int _ResolutionDownScale;
		int _SmoothingIterations;
};