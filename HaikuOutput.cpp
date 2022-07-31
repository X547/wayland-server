#include "HaikuOutput.h"
#include "Wayland.h"
#include <Screen.h>

extern const struct wl_interface wl_output_interface;


enum {
	OUTPUT_VERSION = 3,
};


class HaikuOutput: public WlOutput {
public:
	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

	void HandleRelease() final;
};


void HaikuOutput::Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
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
	output->SendGeometry(0, 0, (float)width * 0.3528, (float)height * 0.3528, subpixelUnknown, "Unknown make", "Unknown model", transformNormal);
	output->SendMode(modeCurrent | modePreferred, width, height, 60000);
	output->SendScale(1);
	output->SendDone();
}

void HaikuOutput::HandleRelease()
{
}


struct wl_global *HaikuOutputCreate(struct wl_display *display)
{
	return wl_global_create(display, &wl_output_interface, OUTPUT_VERSION, NULL, HaikuOutput::Bind);
}
