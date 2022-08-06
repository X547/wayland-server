#include "HaikuXdgPositioner.h"
#include "HaikuXdgShell.h"


HaikuXdgPositioner *HaikuXdgPositioner::Create(HaikuXdgWmBase *client, uint32_t id)
{
	HaikuXdgPositioner *positioner = new(std::nothrow) HaikuXdgPositioner();
	if (positioner == NULL) {
		wl_client_post_no_memory(client->Client());
		return NULL;
	}
	if (!positioner->Init(client->Client(), wl_resource_get_version(client->ToResource()), id)) {
		return NULL;
	}
	return positioner;
}


void HaikuXdgPositioner::HandleDestroy()
{
}

void HaikuXdgPositioner::HandleSetSize(int32_t width, int32_t height)
{
	fState.valid.size = true;
	fState.size = {width, height};
}

void HaikuXdgPositioner::HandleSetAnchorRect(int32_t x, int32_t y, int32_t width, int32_t height)
{
	fState.valid.anchorRect = true;
	fState.anchorRect = {x, y, width, height};
}

void HaikuXdgPositioner::HandleSetAnchor(uint32_t anchor)
{
	fState.valid.anchor = true;
	fState.anchor = anchor;
}

void HaikuXdgPositioner::HandleSetGravity(uint32_t gravity)
{
	fState.valid.gravity = true;
	fState.gravity = gravity;
}

void HaikuXdgPositioner::HandleSetConstraintAdjustment(uint32_t constraint_adjustment)
{
	fState.valid.constraintAdjustment = true;
	fState.constraintAdjustment = constraint_adjustment;
}

void HaikuXdgPositioner::HandleSetOffset(int32_t x, int32_t y)
{
	fState.valid.offset = true;
	fState.offset = {x, y};
}

void HaikuXdgPositioner::HandleSetReactive()
{
	fState.valid.reactive = true;
}

void HaikuXdgPositioner::HandleSetParentSize(int32_t parent_width, int32_t parent_height)
{
	fState.valid.parentSize = true;
	fState.parentSize = {parent_width, parent_height};
}

void HaikuXdgPositioner::HandleSetParentConfigure(uint32_t serial)
{
	fState.valid.parentConfigure = true;
	fState.parentConfigure = {serial};
}
