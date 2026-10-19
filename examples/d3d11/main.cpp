// litefont in a Direct3D 11 program: declare, load once, draw.
//
//     litefont_d3d11 [font.ttf] [mono.ttf]        a Win32 window: a triangle drawn with the program's own shaders and
//                                                  text on top, with the code sample in the monospaced font when given;
//                                                  with no fonts named (started from Explorer, say), the fonts folder
//                                                  beside the program is used
//     litefont_d3d11 <font.ttf> --self-check      draws the labels through Direct3D 11 in every space the device has
//                                                  and on the CPU, prints the differences and exits (0 when they match)
//     litefont_d3d11 <font.ttf> --screenshot <file.ppm>   saves the fourth frame and exits
//
// The window, swap chain and triangle are the program's; litefont only draws
// text into whatever render target is bound, and puts the pipeline back.

#include "ExampleScene.h"

#include <litefont/litefont.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

/**
 * TriangleVertex - A corner of the program's own triangle: a position in
 * clip space and a color.
 */
struct TriangleVertex {
	float x = 0.0f;
	float y = 0.0f;
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
};

class Example {
public:
	static int run(int argumentCount, char** arguments);

private:
	static LRESULT CALLBACK handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	static bool createDevice(HWND window);
	static bool createTarget();
	static bool createTriangle();
	static void releaseAll();
	static void bindTarget();
	static void drawTriangle(float angle);
	static void drawFrame();
	static std::vector<litefont::Text> getLabels();
	static int checkAgainstCpu();
	static bool checkStateRestored(const std::vector<litefont::Text>& labels);

	template <typename T>
	static void release(T*& object);

	// (1) Declared once: the device for Direct3D 11 and a painter that draws
	// through it. The Direct3D device is attached once the program has made it.
	static litefont::Font font;
	static litefont::Font bold;
	static litefont::Font mono;
	static litefont::FontFamily family;
	static std::string fontName;
	static litefont::D3D11Device device;
	static litefont::TextPainter painter;
	static litefont::Text tag;

	static ID3D11Device* d3d;
	static ID3D11DeviceContext* context;
	static IDXGISwapChain* swapChain;
	static ID3D11RenderTargetView* target;
	static ID3D11VertexShader* triangleVertexShader;
	static ID3D11PixelShader* trianglePixelShader;
	static ID3D11InputLayout* triangleLayout;
	static ID3D11Buffer* triangleBuffer;
	static int width;
	static int height;
	static int frame;
	static bool selfCheck;
	static std::string screenshot;
	static std::chrono::steady_clock::time_point start;
};

litefont::Font Example::font;
litefont::Font Example::bold;
litefont::Font Example::mono;
litefont::FontFamily Example::family;
std::string Example::fontName;
litefont::D3D11Device Example::device;
litefont::TextPainter Example::painter(Example::device);
litefont::Text Example::tag;
ID3D11Device* Example::d3d = nullptr;
ID3D11DeviceContext* Example::context = nullptr;
IDXGISwapChain* Example::swapChain = nullptr;
ID3D11RenderTargetView* Example::target = nullptr;
ID3D11VertexShader* Example::triangleVertexShader = nullptr;
ID3D11PixelShader* Example::trianglePixelShader = nullptr;
ID3D11InputLayout* Example::triangleLayout = nullptr;
ID3D11Buffer* Example::triangleBuffer = nullptr;
int Example::width = ExampleScene::width;
int Example::height = ExampleScene::height;
int Example::frame = 0;
bool Example::selfCheck = false;
std::string Example::screenshot;
std::chrono::steady_clock::time_point Example::start;

template <typename T>
void Example::release(T*& object) {
	if (object != nullptr) {
		object->Release();
		object = nullptr;
	}
}

int Example::run(int argumentCount, char** arguments) {
	std::vector<std::string> fontPaths;
	for (int i = 1; i < argumentCount; i++) {
		const std::string argument = arguments[i];
		if (argument.rfind("--", 0) != 0) {
			fontPaths.push_back(argument);
		}
		else if (argument == "--self-check") {
			Example::selfCheck = true;
		}
		else if (argument == "--screenshot" && i + 1 < argumentCount) {
			Example::screenshot = arguments[++i];
		}
	}

	// (2) Created once: a font file. Started without one and without a fonts
	// folder, say so in a message box too, since there may be no console.
	if (!ExampleScene::loadFonts(fontPaths, arguments[0], Example::font, Example::bold, Example::mono, Example::fontName)) {
		std::printf("usage: litefont_d3d11 [font.ttf] [mono.ttf] [--self-check | --screenshot <file.ppm>]\n");
		MessageBoxW(nullptr, L"No fonts found. Pass a .ttf, .otf or .ttc file, or put fonts in a folder named fonts beside the program.",
			L"litefont + Direct3D 11", MB_OK | MB_ICONINFORMATION);
		return 1;
	}
	Example::family = litefont::FontFamily(Example::font);
	if (Example::bold.isLoaded()) {
		Example::family.addFace(Example::bold);
	}
	Example::tag = ExampleScene::getTag(Example::family);

	// The program's window, sized so its client area is the scene's size.
	SetProcessDPIAware();
	const HINSTANCE instance = GetModuleHandleW(nullptr);
	WNDCLASSEXW windowClass = {};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Example::handleMessage;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
	windowClass.lpszClassName = L"litefont_d3d11";
	RegisterClassExW(&windowClass);
	RECT frameRect = {0, 0, Example::width, Example::height};
	AdjustWindowRect(&frameRect, WS_OVERLAPPEDWINDOW, FALSE);
	const HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"litefont + Direct3D 11", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
		CW_USEDEFAULT, frameRect.right - frameRect.left, frameRect.bottom - frameRect.top, nullptr, nullptr, instance, nullptr);
	if (window == nullptr || !Example::createDevice(window) || !Example::createTarget() || !Example::createTriangle()) {
		std::printf("cannot create a Direct3D 11 window\n");
		Example::releaseAll();
		return 1;
	}
	Example::device.attach(Example::d3d, Example::context);

	if (Example::selfCheck) {
		const int result = Example::checkAgainstCpu();
		Example::releaseAll();
		return result;
	}
	if (Example::screenshot.empty()) {
		ShowWindow(window, SW_SHOWDEFAULT);
	}
	Example::start = std::chrono::steady_clock::now();
	MSG message = {};
	while (message.message != WM_QUIT) {
		if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageW(&message);
			continue;
		}
		Example::drawFrame();
	}
	Example::releaseAll();
	return 0;
}

LRESULT CALLBACK Example::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_SIZE && Example::swapChain != nullptr && wParam != SIZE_MINIMIZED) {
		Example::width = LOWORD(lParam);
		Example::height = HIWORD(lParam);
		Example::context->OMSetRenderTargets(0, nullptr, nullptr);
		Example::release(Example::target);
		Example::swapChain->ResizeBuffers(0, static_cast<UINT>(Example::width), static_cast<UINT>(Example::height), DXGI_FORMAT_UNKNOWN, 0);
		Example::createTarget();
		return 0;
	}
	if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

bool Example::createDevice(HWND window) {
	DXGI_SWAP_CHAIN_DESC description = {};
	description.BufferCount = 2;
	description.BufferDesc.Width = static_cast<UINT>(Example::width);
	description.BufferDesc.Height = static_cast<UINT>(Example::height);
	description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.OutputWindow = window;
	description.SampleDesc.Count = 1;
	description.Windowed = TRUE;
	description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
	D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
	return SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 3, D3D11_SDK_VERSION, &description,
		&Example::swapChain, &Example::d3d, &level, &Example::context));
}

bool Example::createTarget() {
	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(Example::swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)))) {
		return false;
	}
	const bool made = SUCCEEDED(Example::d3d->CreateRenderTargetView(backBuffer, nullptr, &Example::target));
	Example::release(backBuffer);
	return made;
}

bool Example::createTriangle() {
	// The program's own drawing: shaders compiled at run time, and a vertex
	// buffer it refills every frame.
	const char* source = R"(
struct VertexInput { float2 position : POSITION; float3 color : COLOR; };
struct PixelInput { float4 position : SV_POSITION; float3 color : COLOR; };
PixelInput vertexMain(VertexInput input) {
	PixelInput output;
	output.position = float4(input.position, 0.0, 1.0);
	output.color = input.color;
	return output;
}
float4 pixelMain(PixelInput input) : SV_TARGET { return float4(input.color, 1.0); }
)";
	ID3DBlob* vertexCode = nullptr;
	ID3DBlob* pixelCode = nullptr;
	bool made = SUCCEEDED(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "vertexMain", "vs_4_0", 0, 0, &vertexCode, nullptr))
		&& SUCCEEDED(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "pixelMain", "ps_4_0", 0, 0, &pixelCode, nullptr));
	if (made) {
		const D3D11_INPUT_ELEMENT_DESC elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof(TriangleVertex) * 3;
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		made = SUCCEEDED(Example::d3d->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr, &Example::triangleVertexShader))
			&& SUCCEEDED(Example::d3d->CreatePixelShader(pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(), nullptr, &Example::trianglePixelShader))
			&& SUCCEEDED(Example::d3d->CreateInputLayout(elements, 2, vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), &Example::triangleLayout))
			&& SUCCEEDED(Example::d3d->CreateBuffer(&bufferDesc, nullptr, &Example::triangleBuffer));
	}
	Example::release(vertexCode);
	Example::release(pixelCode);
	return made;
}

void Example::releaseAll() {
	// litefont's objects first, while the Direct3D device is still alive.
	Example::painter.release();
	Example::release(Example::triangleBuffer);
	Example::release(Example::triangleLayout);
	Example::release(Example::trianglePixelShader);
	Example::release(Example::triangleVertexShader);
	Example::release(Example::target);
	Example::release(Example::swapChain);
	Example::release(Example::context);
	Example::release(Example::d3d);
}

void Example::bindTarget() {
	D3D11_VIEWPORT viewport = {};
	viewport.Width = static_cast<FLOAT>(Example::width);
	viewport.Height = static_cast<FLOAT>(Example::height);
	viewport.MaxDepth = 1.0f;
	Example::context->OMSetRenderTargets(1, &Example::target, nullptr);
	Example::context->RSSetViewports(1, &viewport);
	const litefont::Color4f background = ExampleScene::getBackground();
	const FLOAT color[4] = {background.r, background.g, background.b, background.a};
	Example::context->ClearRenderTargetView(Example::target, color);
}

void Example::drawTriangle(float angle) {
	// Clockwise on screen, so Direct3D's default rasterizer state keeps it.
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(Example::context->Map(Example::triangleBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		return;
	}
	TriangleVertex* corners = static_cast<TriangleVertex*>(mapped.pData);
	const int order[] = {0, 2, 1};
	for (int i = 0; i < 3; i++) {
		const litefont::Vector2f position = ExampleScene::getCorner(angle, order[i]);
		const litefont::Color4f color = ExampleScene::getCornerColor(order[i]);
		corners[i] = TriangleVertex{position.x, position.y, color.r, color.g, color.b};
	}
	Example::context->Unmap(Example::triangleBuffer, 0);
	const UINT stride = sizeof(TriangleVertex);
	const UINT offset = 0;
	Example::context->IASetInputLayout(Example::triangleLayout);
	Example::context->IASetVertexBuffers(0, 1, &Example::triangleBuffer, &stride, &offset);
	Example::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Example::context->VSSetShader(Example::triangleVertexShader, nullptr, 0);
	Example::context->PSSetShader(Example::trianglePixelShader, nullptr, 0);
	Example::context->Draw(3, 0);
}

void Example::drawFrame() {
	Example::bindTarget();
	const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - Example::start).count();
	const float angle = ExampleScene::getAngle(seconds, !Example::screenshot.empty());
	Example::drawTriangle(angle);

	// (3) Drawn wherever the program draws: Texts with settings of their own,
	// and plain strings with drawText.
	for (const litefont::Text& label : Example::getLabels()) {
		Example::painter.draw(label);
	}
	const std::string counter = "frame " + std::to_string(Example::frame);
	Example::painter.drawText(Example::font, 12.0f, counter, 24.0f, static_cast<float>(Example::height) - 28.0f,
		litefont::Color4f{0.6f, 0.6f, 0.6f, 1.0f});

	// A label that follows the triangle's top corner: moving it costs no layout.
	const litefont::Vector2f point = ExampleScene::getTagPoint(angle, Example::width, Example::height);
	Example::painter.draw(Example::tag, point.x, point.y);

	// Frame 3: the triangle has been drawn after text three times, so a state
	// the device failed to restore would show in it.
	if (!Example::screenshot.empty() && Example::frame == 3) {
		const bool saved = Example::device.readPixels().save(litefont::File::toPath(Example::screenshot));
		std::printf("%s %s\n", saved ? "saved" : "cannot write", Example::screenshot.c_str());
		PostQuitMessage(saved ? 0 : 1);
	}
	Example::swapChain->Present(1, 0);
	Example::frame++;
}

std::vector<litefont::Text> Example::getLabels() {
	const ExampleDevice device{"Direct3D 11  ·  D3D11Device", "litefont::D3D11Device device(d3d, context);"};
	return ExampleScene::getLabels(Example::family, Example::mono, Example::fontName, device);
}

int Example::checkAgainstCpu() {
	// The labels and a placed tag through Direct3D 11 in every space the
	// device has, against the CPU, then the pipeline state around a draw.
	std::vector<litefont::Text> labels = Example::getLabels();
	litefont::Text placedTag = Example::tag;
	placedTag.setPosition(litefont::Vector2f{410.0f, 300.0f});
	labels.push_back(placedTag);

	const D3D_FEATURE_LEVEL level = Example::d3d->GetFeatureLevel();
	std::printf("Direct3D 11, feature level %d.%d, D3D11Device\n", (level >> 12) & 0xF, (level >> 8) & 0xF);
	Example::bindTarget();
	auto clear = []() {
		const litefont::Color4f background = ExampleScene::getBackground();
		const FLOAT color[4] = {background.r, background.g, background.b, background.a};
		Example::context->ClearRenderTargetView(Example::target, color);
	};
	auto useStack = []() {
		return false;
	};
	const bool good = ExampleScene::checkSpaces(Example::device, Example::painter, labels, litefont::Size2i{Example::width, Example::height}, clear, useStack);
	const bool restored = Example::checkStateRestored(labels);
	std::printf("%-20s %s\n", "Direct3D state", restored ? "restored after drawing" : "changed by drawing");
	if (Example::device.getError() != litefont::D3D11Error::None) {
		std::printf("device error %d\n", static_cast<int>(Example::device.getError()));
	}
	return good && restored ? 0 : 1;
}

bool Example::checkStateRestored(const std::vector<litefont::Text>& labels) {
	// A pipeline state unlike the one litefont draws with, which drawing must
	// leave as it found it: the program's own shaders, input layout, buffer
	// and topology, and blend, depth and rasterizer states of its own.
	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerDesc.CullMode = D3D11_CULL_FRONT;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.ScissorEnable = TRUE;
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthDesc.DepthFunc = D3D11_COMPARISON_GREATER;
	ID3D11RasterizerState* rasterizer = nullptr;
	ID3D11BlendState* blend = nullptr;
	ID3D11DepthStencilState* depth = nullptr;
	Example::d3d->CreateRasterizerState(&rasterizerDesc, &rasterizer);
	Example::d3d->CreateBlendState(&blendDesc, &blend);
	Example::d3d->CreateDepthStencilState(&depthDesc, &depth);
	const FLOAT factor[4] = {0.25f, 0.5f, 0.75f, 1.0f};
	const D3D11_RECT scissor = {1, 2, 3, 4};
	const UINT stride = sizeof(TriangleVertex);
	const UINT offset = 0;
	Example::context->RSSetState(rasterizer);
	Example::context->OMSetBlendState(blend, factor, 0x0F);
	Example::context->OMSetDepthStencilState(depth, 3);
	Example::context->RSSetScissorRects(1, &scissor);
	Example::context->IASetInputLayout(Example::triangleLayout);
	Example::context->IASetVertexBuffers(0, 1, &Example::triangleBuffer, &stride, &offset);
	Example::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Example::context->VSSetShader(Example::triangleVertexShader, nullptr, 0);
	Example::context->PSSetShader(Example::trianglePixelShader, nullptr, 0);

	auto read = []() {
		std::vector<std::uintptr_t> values;
		auto keep = [&values](IUnknown* object) {
			values.push_back(reinterpret_cast<std::uintptr_t>(object));
			if (object != nullptr) {
				object->Release();
			}
		};
		ID3D11RasterizerState* rasterizerNow = nullptr;
		Example::context->RSGetState(&rasterizerNow);
		keep(rasterizerNow);
		ID3D11BlendState* blendNow = nullptr;
		FLOAT factorNow[4] = {};
		UINT mask = 0;
		Example::context->OMGetBlendState(&blendNow, factorNow, &mask);
		keep(blendNow);
		for (const FLOAT component : factorNow) {
			std::uint32_t bits = 0;
			std::memcpy(&bits, &component, sizeof(bits));
			values.push_back(bits);
		}
		values.push_back(mask);
		ID3D11DepthStencilState* depthNow = nullptr;
		UINT reference = 0;
		Example::context->OMGetDepthStencilState(&depthNow, &reference);
		keep(depthNow);
		values.push_back(reference);
		UINT count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		D3D11_RECT rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
		Example::context->RSGetScissorRects(&count, rects);
		values.insert(values.end(), {count, static_cast<std::uintptr_t>(rects[0].left), static_cast<std::uintptr_t>(rects[0].top),
			static_cast<std::uintptr_t>(rects[0].right), static_cast<std::uintptr_t>(rects[0].bottom)});
		ID3D11InputLayout* layout = nullptr;
		Example::context->IAGetInputLayout(&layout);
		keep(layout);
		ID3D11Buffer* buffer = nullptr;
		UINT strideNow = 0;
		UINT offsetNow = 0;
		Example::context->IAGetVertexBuffers(0, 1, &buffer, &strideNow, &offsetNow);
		keep(buffer);
		values.push_back(strideNow);
		values.push_back(offsetNow);
		D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		Example::context->IAGetPrimitiveTopology(&topology);
		values.push_back(static_cast<std::uintptr_t>(topology));
		ID3D11VertexShader* vertexShader = nullptr;
		Example::context->VSGetShader(&vertexShader, nullptr, nullptr);
		keep(vertexShader);
		ID3D11PixelShader* pixelShader = nullptr;
		Example::context->PSGetShader(&pixelShader, nullptr, nullptr);
		keep(pixelShader);
		ID3D11ShaderResourceView* resource = nullptr;
		Example::context->PSGetShaderResources(0, 1, &resource);
		keep(resource);
		ID3D11SamplerState* sampler = nullptr;
		Example::context->PSGetSamplers(0, 1, &sampler);
		keep(sampler);
		ID3D11Buffer* constants = nullptr;
		Example::context->VSGetConstantBuffers(0, 1, &constants);
		keep(constants);
		return values;
	};
	const std::vector<std::uintptr_t> before = read();
	for (const litefont::Text& label : labels) {
		Example::painter.draw(label);
	}
	const std::vector<std::uintptr_t> after = read();

	// Back to the defaults for whatever is drawn next.
	Example::context->ClearState();
	Example::release(rasterizer);
	Example::release(blend);
	Example::release(depth);
	return before == after;
}

int main(int argumentCount, char** arguments) {
	return Example::run(argumentCount, arguments);
}
