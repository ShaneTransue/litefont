// The smallest litefont program with Direct3D 11: "Hello world" in a Win32 window.
//
//     litefont_hello_d3d11 [font.ttf]
//
// Without a font it loads fonts/NotoSans-Regular.ttf next to the program.

#include <litefont/litefont.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>

#include <filesystem>

// (1) Declared once: a font, the device for Direct3D 11, and a painter.
static litefont::Font font;
static litefont::D3D11Device device;
static litefont::TextPainter painter(device);

// The program's own Direct3D objects.
static ID3D11Device* d3d = nullptr;
static ID3D11DeviceContext* context = nullptr;
static IDXGISwapChain* swapChain = nullptr;
static ID3D11RenderTargetView* target = nullptr;
static UINT width = 640;
static UINT height = 360;

static void resize(UINT newWidth, UINT newHeight) {
	width = newWidth;
	height = newHeight;
	if (target != nullptr) {
		target->Release();
	}
	swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	ID3D11Texture2D* backBuffer = nullptr;
	swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
	d3d->CreateRenderTargetView(backBuffer, nullptr, &target);
	backBuffer->Release();
}

// (2) Made once: Direct3D for the window, handed to litefont, and the font.
static bool init(HWND window, int argumentCount, char** arguments) {
	DXGI_SWAP_CHAIN_DESC description = {};
	description.BufferCount = 2;
	description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.OutputWindow = window;
	description.SampleDesc.Count = 1;
	description.Windowed = TRUE;
	description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &description,
			&swapChain, &d3d, nullptr, &context))) {
		return false;
	}
	resize(width, height);
	device.attach(d3d, context);
	const std::filesystem::path path = argumentCount > 1 ? litefont::File::toPath(arguments[1])
		: litefont::File::toPath(arguments[0]).parent_path() / "fonts" / "NotoSans-Regular.ttf";
	return font.load(path);
}

// (3) Drawn wherever the program draws.
static void display() {
	const FLOAT background[4] = {0.1f, 0.1f, 0.1f, 1.0f};
	const D3D11_VIEWPORT viewport = {0.0f, 0.0f, static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0.0f, 1.0f};
	context->OMSetRenderTargets(1, &target, nullptr);
	context->RSSetViewports(1, &viewport);
	context->ClearRenderTargetView(target, background);
	painter.drawText(font, 48.0f, "Hello world", 40.0f, 40.0f);
	painter.drawText(font, 18.0f, "drawn with litefont through Direct3D 11", 42.0f, 110.0f, litefont::Color4f{0.6f, 0.8f, 1.0f, 1.0f});
	swapChain->Present(1, 0);
}

static LRESULT CALLBACK handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_SIZE && swapChain != nullptr && wParam != SIZE_MINIMIZED) {
		resize(LOWORD(lParam), HIWORD(lParam));
		return 0;
	}
	if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

int main(int argumentCount, char** arguments) {
	WNDCLASSW windowClass = {};
	windowClass.lpfnWndProc = handleMessage;
	windowClass.hInstance = GetModuleHandleW(nullptr);
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = L"hello_litefont";
	RegisterClassW(&windowClass);
	RECT frame = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
	const HWND window = CreateWindowW(windowClass.lpszClassName, L"Hello litefont", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, windowClass.hInstance, nullptr);
	if (!init(window, argumentCount, arguments)) {
		MessageBoxW(window, L"Cannot start Direct3D 11 or load the font.", L"Hello litefont", MB_OK);
		return 1;
	}
	MSG message = {};
	while (message.message != WM_QUIT) {
		if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		else {
			display();
		}
	}
	painter.release();
	target->Release();
	swapChain->Release();
	context->Release();
	d3d->Release();
	return 0;
}
