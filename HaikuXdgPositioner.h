#pragma once
#include "XdgShell.h"
#include <Rect.h>


class HaikuXdgWmBase;

class HaikuXdgPositioner: public XdgPositioner {
public:
	union Valid {
		struct {
			uint32_t size: 1;
			uint32_t anchorRect: 1;
			uint32_t anchor: 1;
			uint32_t gravity: 1;
			uint32_t constraintAdjustment: 1;
			uint32_t offset: 1;
			uint32_t reactive: 1;
			uint32_t parentSize: 1;
			uint32_t parentConfigure: 1;
		};
		uint32_t val;
	};
	
	struct State {
		Valid valid{};
		struct {
			int32_t width;
			int32_t height;
		} size;
		struct {
			int32_t x;
			int32_t y;
			int32_t width;
			int32_t height;
		} anchorRect;
		uint32_t anchor;
		uint32_t gravity;
		uint32_t constraintAdjustment;
		struct {
			int32_t x;
			int32_t y;
		} offset;
		struct {
			int32_t width;
			int32_t height;
		} parentSize;
		struct {
			uint32_t serial;
		} parentConfigure;
	};
private:
	State fState{};

public:
	static HaikuXdgPositioner *Create(HaikuXdgWmBase *client, uint32_t id);
	static HaikuXdgPositioner *FromResource(struct wl_resource *resource) {return (HaikuXdgPositioner*)WlResource::FromResource(resource);}

	const State &GetState() const {return fState;}
	void GetPosition(BRect &pos, const BRect &workArea);

	void HandleSetSize(int32_t width, int32_t height) final;
	void HandleSetAnchorRect(int32_t x, int32_t y, int32_t width, int32_t height) final;
	void HandleSetAnchor(uint32_t anchor) final;
	void HandleSetGravity(uint32_t gravity) final;
	void HandleSetConstraintAdjustment(uint32_t constraint_adjustment) final;
	void HandleSetOffset(int32_t x, int32_t y) final;
	void HandleSetReactive() final;
	void HandleSetParentSize(int32_t parent_width, int32_t parent_height) final;
	void HandleSetParentConfigure(uint32_t serial) final;
};
