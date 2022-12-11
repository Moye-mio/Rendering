#pragma once

#include "math.hpp"
#include <d3d11.h>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

class Lines
{
public:

	Lines(const std::string& path, int totalNumCPs) :
		_NumLines(0),
		_VbPosition(NULL),
		_VbID(NULL),
		_VbImportance(NULL),
		_VbAlphaWeights(NULL),
		_VbCurrentAlpha(NULL),
		_SrvAlphaWeights(NULL),
		_SrvCurrentAlpha(NULL),
		_UavCurrentAlpha(NULL),
		_LineID(NULL),
		_SrvLineID(NULL),
		_TotalNumberOfControlPoints(totalNumCPs)
	{
		_AlphaBuffer[0] = _AlphaBuffer[1] = NULL;
		_SrvAlphaBuffer[0] = _SrvAlphaBuffer[1] = NULL;
		_UavAlphaBuffer[0] = _UavAlphaBuffer[1] = NULL;

		LoadLineSet(path);
	}

	~Lines() {
		Release();
	}

	bool Create(ID3D11Device* Device)
	{
		// create the vertex buffers
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.ByteWidth = (UINT)_Positions.size() * sizeof(XMFLOAT3);
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
		initData.pSysMem = _Positions.data();
		if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbPosition))) return false;

		bufferDesc.ByteWidth = (UINT)_ID.size() * sizeof(int);
		initData.pSysMem = _ID.data();
		if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbID))) return false;

		initData.pSysMem = _Importance.data();
		if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbImportance))) return false;

		// create buffer for the alpha weights
		{
			bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
			bufferDesc.ByteWidth = (UINT)_Positions.size() * sizeof(float);
			initData.pSysMem = &_AlphaWeights[0];
			if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbAlphaWeights))) return false;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv;
			ZeroMemory(&srv, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			srv.BufferEx.NumElements = (UINT)_Positions.size();
			srv.Format = DXGI_FORMAT_R32_TYPELESS;
			if (FAILED(Device->CreateShaderResourceView(_VbAlphaWeights, &srv, &_SrvAlphaWeights))) return false;
		}

		// create current alpha resources
		{
			unsigned int NUM_ELEMENTS = (unsigned int)_Positions.size();
			D3D11_BUFFER_DESC bufDesc;
			ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
			bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			bufDesc.ByteWidth = NUM_ELEMENTS * sizeof(float);
			bufDesc.CPUAccessFlags = 0;
			bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			bufDesc.StructureByteStride = sizeof(unsigned int);
			bufDesc.Usage = D3D11_USAGE_DEFAULT;
			if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &_VbCurrentAlpha))) return false;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv;
			ZeroMemory(&srv, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			srv.BufferEx.NumElements = NUM_ELEMENTS;
			srv.Format = DXGI_FORMAT_R32_TYPELESS;
			if (FAILED(Device->CreateShaderResourceView(_VbCurrentAlpha, &srv, &_SrvCurrentAlpha))) return false;

			D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
			ZeroMemory(&uav, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			uav.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uav.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
			uav.Buffer.NumElements = NUM_ELEMENTS;
			uav.Format = DXGI_FORMAT_R32_TYPELESS;
			if (FAILED(Device->CreateUnorderedAccessView(_VbCurrentAlpha, &uav, &_UavCurrentAlpha))) return false;
		}

		for (int p = 0; p<2; ++p)
		{
			unsigned int NUM_ELEMENTS = _TotalNumberOfControlPoints;
			D3D11_BUFFER_DESC bufDesc;
			ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
			bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			bufDesc.ByteWidth = NUM_ELEMENTS * sizeof(unsigned int);
			bufDesc.CPUAccessFlags = 0;
			bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			bufDesc.StructureByteStride = sizeof(unsigned int);
			bufDesc.Usage = D3D11_USAGE_DEFAULT;
			if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &_AlphaBuffer[p]))) return false;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv;
			ZeroMemory(&srv, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			srv.BufferEx.NumElements = NUM_ELEMENTS;
			srv.Format = DXGI_FORMAT_R32_TYPELESS;
			if (FAILED(Device->CreateShaderResourceView(_AlphaBuffer[p], &srv, &_SrvAlphaBuffer[p]))) return false;

			D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
			ZeroMemory(&uav, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			uav.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uav.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
			uav.Buffer.NumElements = NUM_ELEMENTS;
			uav.Format = DXGI_FORMAT_R32_TYPELESS;
			if (FAILED(Device->CreateUnorderedAccessView(_AlphaBuffer[p], &uav, &_UavAlphaBuffer[p]))) return false;
		}

		{
			unsigned int NUM_ELEMENTS = _TotalNumberOfControlPoints;
			D3D11_BUFFER_DESC bufDesc;
			ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
			bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			bufDesc.ByteWidth = NUM_ELEMENTS * sizeof(unsigned int);
			bufDesc.CPUAccessFlags = 0;
			bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			bufDesc.StructureByteStride = sizeof(unsigned int);
			bufDesc.Usage = D3D11_USAGE_DEFAULT;

			D3D11_SUBRESOURCE_DATA initData;
			ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
			initData.pSysMem = _ControlPointLineIndices.data();
			if (FAILED(Device->CreateBuffer(&bufDesc, &initData, &_LineID))) return false;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv;
			ZeroMemory(&srv, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			srv.BufferEx.NumElements = NUM_ELEMENTS;
			srv.Format = DXGI_FORMAT_R32_TYPELESS;
			if (FAILED(Device->CreateShaderResourceView(_LineID, &srv, &_SrvLineID))) return false;
		}

		return true;
	}

	void Release()
	{
		if (_VbPosition)		_VbPosition->Release();			_VbPosition = NULL;
		if (_VbID)				_VbID->Release();				_VbID = NULL;
		if (_VbImportance)		_VbImportance->Release();		_VbImportance = NULL;
		if (_VbAlphaWeights)	_VbAlphaWeights->Release();		_VbAlphaWeights = NULL;
		if (_SrvAlphaWeights)	_SrvAlphaWeights->Release();	_SrvAlphaWeights = NULL;
		if (_VbCurrentAlpha)	_VbCurrentAlpha->Release();		_VbCurrentAlpha = NULL;
		if (_SrvCurrentAlpha)	_SrvCurrentAlpha->Release();	_SrvCurrentAlpha = NULL;
		if (_UavCurrentAlpha)	_UavCurrentAlpha->Release();	_UavCurrentAlpha = NULL;
		for (int p = 0; p<2; ++p) {
			if (_AlphaBuffer[p])		_AlphaBuffer[p]->Release();		_AlphaBuffer[p] = NULL;
			if (_SrvAlphaBuffer[p])		_SrvAlphaBuffer[p]->Release();	_SrvAlphaBuffer[p] = NULL;
			if (_UavAlphaBuffer[p])		_UavAlphaBuffer[p]->Release();	_UavAlphaBuffer[p] = NULL;
		}
		if (_LineID)			_LineID->Release();				_LineID = NULL;
		if (_SrvLineID)			_SrvLineID->Release();			_SrvLineID = NULL;
	}

	void DrawHQ(ID3D11DeviceContext* ImmediateContext)
	{
		ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

		ID3D11Buffer* vbs[] = { _VbPosition, _VbID, _VbCurrentAlpha };
		UINT strides[] = { sizeof(float) * 3, sizeof(int), sizeof(float) };
		UINT offsets[] = { 0, 0, 0 };
		ImmediateContext->IASetVertexBuffers(0, 3, vbs, strides, offsets);
		ImmediateContext->Draw(GetTotalNumberOfVertices() - 2, 0);
	}

	void DrawLowRes(ID3D11DeviceContext* ImmediateContext)
	{
		ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

		ID3D11Buffer* vbs[] = { _VbPosition, _VbID, _VbImportance, _VbAlphaWeights };
		UINT strides[] = { sizeof(float) * 3, sizeof(int), sizeof(float), sizeof(float) };
		UINT offsets[] = { 0, 0, 0, 0 };
		ImmediateContext->IASetVertexBuffers(0, 4, vbs, strides, offsets);
		ImmediateContext->Draw(GetTotalNumberOfVertices() - 2, 0);
	}

	ID3D11ShaderResourceView* GetSrvCurrentAlpha() { return _SrvCurrentAlpha; }
	ID3D11UnorderedAccessView* GetUavCurrentAlpha() { return _UavCurrentAlpha; }
	ID3D11ShaderResourceView** GetSrvAlpha() { return _SrvAlphaBuffer; }
	ID3D11UnorderedAccessView** GetUavAlpha() { return _UavAlphaBuffer; }
	ID3D11ShaderResourceView* GetSrvAlphaWeights() { return _SrvAlphaWeights; }
	ID3D11ShaderResourceView* GetSrvLineID() { return _SrvLineID; }

	int GetTotalNumberOfControlPoints() const { return _TotalNumberOfControlPoints; }
	int GetTotalNumberOfVertices() const { return (int)_Positions.size(); }

private:

	void LoadLineSet(const std::string& path)
	{
		typedef std::vector<XMFLOAT3> Line;
		static const int OBJ_ZERO_BASED_SHIFT = -1;

		std::vector<Line> lines;
		std::vector<float> objImportance;

		XMFLOAT3 last(0, 0, 0);
		_NumLines = 0;
		std::vector<XMFLOAT3> vertices;
		std::ifstream myfile(path);
		if (myfile.is_open())
		{
			std::string line;
			while (myfile.good())
			{
				std::getline(myfile, line);
				if (line.size() < 2) continue;
				if (line[0] == 'v' && line[1] == ' ')	// read in vertex
				{
					XMFLOAT3 vertex;
					if (sscanf_s(line.c_str(), "v %f %f %f", &vertex.x, &vertex.y, &vertex.z) == 3)
						vertices.push_back(vertex);
				}
				else if (line[0] == 'v' && line[1] == 't')	// read in tex coord (aka importance)
				{
					float imp;
					if (sscanf_s(line.c_str(), "vt %f", &imp) == 1)
					{
						objImportance.push_back(imp);
					}
				}
				else if (line[0] == 'l') // read in line indices
				{
					std::stringstream t(line);
					std::string lead;
					t >> lead;
					int index;
					Line vline;
					float noise = 0; // 0.01f + rand() / (float)(RAND_MAX)* 0.2f;
					while (t >> index)
					{
						XMVECTOR vNew = XMLoadFloat3(&vertices[index + OBJ_ZERO_BASED_SHIFT]);
						XMVECTOR vLast = XMLoadFloat3(&last);
						float dist;
						XMVECTOR vDist = XMVector3LengthSq(vNew - vLast);
						XMStoreFloat(&dist, vDist);
						if (dist > 0.0001f)
						{
							last = vertices[index + OBJ_ZERO_BASED_SHIFT];
							vline.push_back(vertices[index + OBJ_ZERO_BASED_SHIFT]);


							if (index + OBJ_ZERO_BASED_SHIFT < objImportance.size())
							{
								float imp = objImportance[index + OBJ_ZERO_BASED_SHIFT];
								_Importance.push_back(imp + noise);
							}
						}
					}
					lines.push_back(vline);
					_NumLines++;
				}
			}
			myfile.close();
		}

		// compute the lengths of the lines
		float lineLengthMin = FLT_MAX;
		float lineLengthMax = -FLT_MAX;
		{
			int lineId = 0;
			{
				_LineLengths.clear();
				_LineLengths.resize(lines.size(), 0);
				float totalLengths = 0;
				for (auto itLine = lines.begin(); itLine != lines.end(); ++itLine, lineId++)
				{
					float length = 0;
					for (int id = 0; id < (int)itLine->size() - 1; ++id)
					{
						XMVECTOR a = XMLoadFloat3(&itLine->at(id));
						XMVECTOR b = XMLoadFloat3(&itLine->at(id + 1));
						float l = 0;
						XMStoreFloat(&l, XMVector3Length(a - b));

						if (l < 999999999)
							length += l;
					}
					_LineLengths[lineId] = length;

					totalLengths += length;
					lineLengthMin = std::min(lineLengthMin, length);
					lineLengthMax = std::max(lineLengthMax, length);
				}
			}
		}

		float accumLineLength = 0;
		int totalNumPoints = 0;
		{
			int lineId = 0;
			// get total number of vertices
			for (auto itLine = lines.begin(); itLine != lines.end(); ++itLine, lineId++)
			{
				totalNumPoints += (int)itLine->size();
				accumLineLength += _LineLengths[lineId];
			}
		}

		// store position data and IDs in linear memory
		_Positions.resize(totalNumPoints);
		_ID.resize(totalNumPoints);

		int offset = 0;
		int lineID = 0;
		for (auto itLine = lines.begin(); itLine != lines.end(); ++itLine)
		{
			memcpy_s(&(_Positions)[offset], (totalNumPoints - offset) * sizeof(XMFLOAT3), &((*itLine)[0]), itLine->size() * sizeof(XMFLOAT3));
			for (unsigned int i = 0; i<itLine->size(); ++i)
				_ID[i + offset] = lineID;
			offset += (int)itLine->size();
			lineID++;
		}

		// ==============================================================
		// Distribute polyline segments (here sometimes called control points) among the lines so that they are roughly equally-sized.
		// ==============================================================
		assert(_TotalNumberOfControlPoints * 2 > (int)_NumLines);

		_NumberOfControlPointsOfLine.resize(_NumLines);
		{
			// calculate the average length of a polyline segment
			float lengthPerPatch = accumLineLength / _TotalNumberOfControlPoints;

			// we first make sure that lines shorter than this length receive two polyline segments
			int remPatches = _TotalNumberOfControlPoints;
			float remLength = accumLineLength;
			for (int lineId = 0; lineId < lines.size(); ++lineId)
			{
				float length = _LineLengths[lineId];
				if (length <= lengthPerPatch)
				{
					_NumberOfControlPointsOfLine[lineId] = 2;
					remPatches -= 2;
					remLength -= length;
				}
			}

			// the remaining polyline segments are distributed among the other lines
			std::map<float, int> remainder;
			int assignedPatches = 0;
			for (int lineId = 0; lineId < lines.size(); ++lineId)
			{
				float length = _LineLengths[lineId];
				if (length > lengthPerPatch)
				{
					float numPatches = length / remLength * remPatches;
					_NumberOfControlPointsOfLine[lineId] = (int)numPatches;
					assignedPatches += (int)numPatches;
					remainder.insert(std::pair<float, int>(numPatches - (int)numPatches, lineId));
				}
			}

			// due to rounding a few segments are not yet assigned. do so now.
			assert(assignedPatches <= remPatches);
			while (assignedPatches < remPatches)
			{
				// get the patch with the largest remainder
				auto itBiggest = --remainder.end();
				int biggestRemLineId = itBiggest->second;
				remainder.erase(itBiggest);
				_NumberOfControlPointsOfLine[biggestRemLineId] += 1;
				assignedPatches += 1;
			}
		}

		// ==============================================================
		// Compute the blending weight parameterization
		// ==============================================================

		// compute the (alpha) control weights
		_AlphaWeights.resize(totalNumPoints);
		{
			int offset = 0;
			int lineId = 0;
			int cpOffset = 0;
			for (auto itLine = lines.begin(); itLine != lines.end(); ++itLine, lineId++)
			{
				float currLength = 0;
				float lineLength = _LineLengths[lineId];
				_AlphaWeights[offset] = (float)cpOffset; //_NumControlPointsPerLine * lineId;
				offset++;

				int numCp = _NumberOfControlPointsOfLine[lineId];
				for (unsigned int id = 0; id < itLine->size() - 1; ++id)
				{
					XMVECTOR a = XMLoadFloat3(&itLine->at(id));
					XMVECTOR b = XMLoadFloat3(&itLine->at(id + 1));
					float l = 0;
					XMStoreFloat(&l, XMVector3Length(a - b));
					currLength += l;

					_AlphaWeights[offset] = std::min(currLength / lineLength * (numCp - 1), numCp - 1 - 0.0001f);
					_AlphaWeights[offset] += cpOffset;

					offset++;
				}
				cpOffset += numCp;
			}
			if (offset != totalNumPoints)
				throw new std::exception("Offset mismatch.");
		}

		{
			_ControlPointLineIndices.resize(_TotalNumberOfControlPoints);
			int lineID = 0;
			int cpID = 0;
			for (int numCP : _NumberOfControlPointsOfLine)
			{
				for (int i = 0; i < numCP; ++i)
					_ControlPointLineIndices[cpID++] = lineID;

				lineID++;
			}
		}
	}
	
	int _TotalNumberOfControlPoints;
	
	int _NumLines;
	
	ID3D11Buffer* _VbPosition;
	ID3D11Buffer* _VbID;
	ID3D11Buffer* _VbImportance;
	ID3D11Buffer* _VbAlphaWeights;	// blending weights (basically the position between control points)
	ID3D11Buffer* _VbCurrentAlpha;	// alpha stored with the vertex buffer

	ID3D11ShaderResourceView* _SrvAlphaWeights;
	ID3D11ShaderResourceView* _SrvCurrentAlpha;
	ID3D11UnorderedAccessView* _UavCurrentAlpha;

	ID3D11Buffer* _AlphaBuffer[2];	// alpha at the control points -> desired alpha value -> we smoothly fade to it.
	ID3D11UnorderedAccessView* _UavAlphaBuffer[2];
	ID3D11ShaderResourceView* _SrvAlphaBuffer[2];

	ID3D11Buffer* _LineID;		// stores for every control point the lineID (used for smoothing)
	ID3D11ShaderResourceView* _SrvLineID;

	std::vector<Vec3f> _Positions;
	std::vector<int> _ID;
	std::vector<float> _Importance;
	std::vector<float> _AlphaWeights;	// Blending weight parameterization

	std::vector<float> _LineLengths;
	std::vector<int> _NumberOfControlPointsOfLine;
	std::vector<unsigned int> _ControlPointLineIndices;

};