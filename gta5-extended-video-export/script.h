#pragma once

#include <dxgi.h>
#include <d3d11.h>
#include <wrl.h>
using namespace Microsoft::WRL;

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

template <class B, class A> B ForceCast(A a) {
	union {
		A a;
		B b;
	} x;
	x.a = a;
	return x.b;
}
void initialize();
void ScriptMain();
void onPresent(IDXGISwapChain *swapChain);
void finalize();
static void prepareDeferredContext(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext);
static ComPtr<ID3D11Texture2D> divideBuffer(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext, uint32_t k);
static void drawAdditive(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext, ComPtr<ID3D11Texture2D> pSource);