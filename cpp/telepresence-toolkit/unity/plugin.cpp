#include <string>
#define NOMINMAX
#include <d3d11.h>
#undef NOMINMAX
#include "interfaces/IUnityGraphics.h"
#include "interfaces/IUnityGraphicsD3D11.h"
#include "texture_group.h"

// Unity and Direct3D varaibles that are saved for future use that is mainly through OnRenderEvent().
static IUnityInterfaces* unity_interfaces_ = nullptr;
static IUnityGraphics* unity_graphics_ = nullptr;
static ID3D11Device* d3d11_device_ = nullptr;
static ID3D11DeviceContext* d3d11_device_context_ = nullptr;

// A callback function for UnityGfxDeviceEvents.
#pragma warning(push)
#pragma warning(disable: 26812)
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
#pragma warning(pop)
{
    switch(eventType) {
    case kUnityGfxDeviceEventInitialize: {
        IUnityGraphicsD3D11* d3d = unity_interfaces_->Get<IUnityGraphicsD3D11>();
		d3d11_device_ = d3d->GetDevice();
		// Has to have this if statement since including audio spatializer invokes OnGraphicsDeviceEvent
		// with kUnityGfxDeviceEventInitialize when d3d->GetDevice() returns null.
		// I guess that this function gets invoked twice when the audio spatializer is turned on,
        // and d3d->GetDevice() returns null during the first time, but not the second time.
		if (d3d11_device_)
			d3d11_device_->GetImmediateContext(&d3d11_device_context_);
        break;
    }
    case kUnityGfxDeviceEventShutdown: {
        d3d11_device_ = nullptr;
        d3d11_device_context_ = nullptr;
        break;
    }
    };
}

// Gets called during a render thread through GL.IssuePluginEvent() in Unity scripts.
static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
    constexpr int COMMAND_COUNT{2};
    const int command_id{eventID % COMMAND_COUNT};
    const int texture_group_id{eventID / COMMAND_COUNT};
	switch (command_id) {
	case 0:
		texture_group_init(texture_group_id, d3d11_device_);
		break;
	case 1:
		texture_group_update(texture_group_id, d3d11_device_, d3d11_device_context_);
		break;
	}
}

// A function that gets called by Unity when the application using this plugin starts.
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* interfaces)
{
    unity_interfaces_ = interfaces;
    unity_graphics_ = unity_interfaces_->Get<IUnityGraphics>();
    unity_graphics_->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // OnGraphicsDeviceEvent() manually called since the editor of Unity may
    // call OnGraphicsDeviceEvent() only for the first time the scene gets played,
    // not every time.
    // For HoloLens, 64-bit means Unity editor since current Unity editors only support 64-bit,
    // while HoloLens runs on a 32-bit OS.
#ifdef _WIN64
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
#endif
}

// A function that Unity calls when the application using this plugin terminates.
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    unity_graphics_->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API has_unity_interfaces()
{
	return unity_interfaces_ != nullptr;
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API has_unity_graphics()
{
	return unity_graphics_ != nullptr;
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API has_d3d11_device()
{
	return d3d11_device_ != nullptr;
}

// A getter function that allows Unity to call OnRenderEvent() in a render thread.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API get_render_event_function_pointer()
{
    return OnRenderEvent;
}