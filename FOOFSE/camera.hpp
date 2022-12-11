#pragma once

#include "math.hpp"
#include "cbuffer.hpp"

class Camera
{
	
public:

	struct CbParam
	{
		CbParam() : mView(), mProj() {}
		XMFLOAT4X4 mView;
		XMFLOAT4X4 mProj;
	};

	explicit Camera(const Vec3f& position, const Vec3f& lookAt, float aspectRatio, HWND hWnd) :
		mPosition(position.x, position.y, position.z),
		mLookAt(lookAt.x, lookAt.y, lookAt.z),
		mUp(0, 1, 0),
		mAspectRatio(aspectRatio),
		mMoveStep(20.0f),
		mRotStep(0.001f),
		mLastX(0),
		mLastY(0),
		mDamping(1.0f),
		mNear(0.01f),
		mFar(100),
		mPhi(0),
		mTheta(0),
		hWnd(hWnd)
	{
		mCurrentLookAt = mLookAt;
		mCurrentPosition = mPosition;
		UpdateViewMatrix();
		UpdateProjMatrix();
	}

	~Camera()
	{

	}
	
	bool Create(ID3D11Device* Device)
	{
		if (!mParam.Create(Device)) return false;
		return true;
	}

	void Release()
	{
		mParam.Release();
	}

	void Update(double elapsedTime)
	{
		bool window_has_focus = GetActiveWindow() == hWnd;
		if (window_has_focus)
		{
			float y = (float)((GetAsyncKeyState('W') < 0) - (GetAsyncKeyState('S') < 0)) * mMoveStep * (float)elapsedTime;
			if (y != 0)
			{
				XMVECTOR lookAt = XMLoadFloat3(&mLookAt);
				XMVECTOR eye = XMLoadFloat3(&mPosition);
				XMVECTOR dir = XMVector3Normalize(lookAt - eye);
				eye = eye + dir * y;
				XMStoreFloat3(&mPosition, eye);
			}

			float RotationCoef = 0.1;
			float x = (float)((GetAsyncKeyState('A') < 0) - (GetAsyncKeyState('D') < 0)) * mMoveStep * (float)elapsedTime * RotationCoef;
			if (x != 0)
			{
				XMVECTOR lookAt = XMLoadFloat3(&mLookAt);
				XMVECTOR eye = XMLoadFloat3(&mPosition);
				XMVECTOR up = XMLoadFloat3(&mUp);

				// rotate left - right
				XMVECTOR dir = lookAt - eye;
				float distance;
				XMStoreFloat(&distance, XMVector3Length(dir));
				XMVECTOR rot = XMQuaternionRotationAxis(up, x);
				dir = XMVector3Rotate(dir, rot);

				eye = (lookAt - XMVector3Normalize(dir) * distance);
				//XMStoreFloat3(&mLookAt, lookAt);
				XMStoreFloat3(&mPosition, eye);
				//UpdateViewMatrix();
			}

		}
		POINT arg;
		if (GetCursorPos(&arg))
		{
			if (window_has_focus && (GetAsyncKeyState(VK_LBUTTON) < 0))
			{
				float rx = (arg.x - mLastX) * mRotStep;
				float ry = -(arg.y - mLastY) * mRotStep;
				if (rx != 0 || ry != 0)
				{
					XMVECTOR lookAt = XMLoadFloat3(&mLookAt);
					XMVECTOR eye = XMLoadFloat3(&mPosition);
					XMVECTOR up = XMLoadFloat3(&mUp);

					// rotate left - right
					XMVECTOR dir = lookAt - eye;
					float distance;
					XMStoreFloat(&distance, XMVector3Length(dir));
					XMVECTOR rot = XMQuaternionRotationAxis(up, rx);
					dir = XMVector3Rotate(dir, rot);

					// rotate up - down            
					XMVECTOR lastDirection = dir;
					XMVECTOR right = XMVector3Normalize(XMVector3Cross(dir, up));
					rot = XMQuaternionRotationAxis(right, ry);
					dir = XMVector3Rotate(dir, rot);

					// prevent flipping around the up-vector
					if (abs(XMVectorGetX(XMVector3Dot(XMVector3Normalize(dir), up))) > 0.99f)
						dir = lastDirection;

					eye = (lookAt - XMVector3Normalize(dir) * distance);
					XMStoreFloat3(&mLookAt, lookAt);
					XMStoreFloat3(&mPosition, eye);
					UpdateViewMatrix();
					SetCursorPos(mLastX, mLastY);
				}
			}
			else
			{
				mLastX = arg.x;
				mLastY = arg.y;
			}
		}
		UpdateViewMatrix();
		UpdateProjMatrix();
	}
	
	ConstantBuffer<CbParam>& GetParams() { return mParam; }

private:

	void UpdateViewMatrix()
	{
		mCurrentPosition.x += (mPosition.x - mCurrentPosition.x) * mDamping;
		mCurrentPosition.y += (mPosition.y - mCurrentPosition.y) * mDamping;
		mCurrentPosition.z += (mPosition.z - mCurrentPosition.z) * mDamping;

		mCurrentLookAt.x += (mLookAt.x - mCurrentLookAt.x) * mDamping;
		mCurrentLookAt.y += (mLookAt.y - mCurrentLookAt.y) * mDamping;
		mCurrentLookAt.z += (mLookAt.z - mCurrentLookAt.z) * mDamping;

		XMMATRIX view = XMMatrixLookAtLH(
			XMLoadFloat3(&mCurrentPosition),
			XMLoadFloat3(&mCurrentLookAt),
			XMLoadFloat3(&mUp));
		XMStoreFloat4x4(&mParam.Data.mView, view);
	}

	void UpdateProjMatrix()
	{
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, mAspectRatio, mNear, mFar);
		XMStoreFloat4x4(&mParam.Data.mProj, proj);
	}

	HWND hWnd;

	XMFLOAT3 mPosition;
	XMFLOAT3 mLookAt;
	XMFLOAT3 mUp;

	ConstantBuffer<CbParam> mParam;

	XMFLOAT3 mCurrentLookAt;
	XMFLOAT3 mCurrentPosition;

	float mMoveStep;
	float mRotStep;
	int mLastX;
	int mLastY;
	float mAspectRatio;
	float mPhi;
	float mTheta;

	float mDamping;

	float mNear;
	float mFar;
};
