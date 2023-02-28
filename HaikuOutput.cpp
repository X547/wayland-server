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
	output->SendGeometry(0, 0, (float)width * 0.3528, (float)height * 0.3528, WlOutput::subpixelUnknown, "Unknown make", "Unknown model", WlOutput::transformNormal);
	output->SendMode(WlOutput::modeCurrent | WlOutput::modePreferred, width, height, 60000);
	output->SendScale(1);
	output->SendDone();
}
