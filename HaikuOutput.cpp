#include "HaikuOutput.h"
#include <Screen.h>
#include <AutoDeleter.h>

extern const struct wl_interface wl_output_interface;


enum {
	OUTPUT_VERSION = 3,
};


class HaikuOutput: public WlOutput {
protected:
	virtual ~HaikuOutput() = default;
};


HaikuOutputGlobal *HaikuOutputGlobal::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuOutputGlobal> global(new(std::nothrow) HaikuOutputGlobal());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &wl_output_interface, OUTPUT_VERSION)) return NULL;
	return global.Detach();
}

void HaikuOutputGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	HaikuOutput *output = new(std::nothrow) HaikuOutput();
	if (output == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!output->Init(wl_client, version, id)) {
		return;
	}

	BScreen screen;
	int32_t width = (int32_t)screen.Frame().Width() + 1;
	int32_t height = (int32_t)screen.Frame().Height() + 1;

	monitor_info info;
	float width_mm = (float)width * 0.3528;
	float height_mm = (float)height * 0.3528;
	const char *make = "Unknown make";
	const char *model = "Unknown model";
	if (screen.GetMonitorInfo(&info) == B_OK) {
		// From cm to mm.
		width_mm = info.width * 10.0;
		height_mm = info.height * 10.0;
		make = info.vendor;
		model = info.name;
	}

	output->SendGeometry(0, 0, width_mm, height_mm, WlOutput::subpixelUnknown, make, model, WlOutput::transformNormal);
	output->SendMode(WlOutput::modeCurrent | WlOutput::modePreferred, width, height, 60000);
	output->SendScale(1);
	output->SendDone();
}
