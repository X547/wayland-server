#include "HaikuXdgPositioner.h"
#include "HaikuXdgShell.h"

#include <algorithm>
#include <stdio.h>


static void GetPosition(const HaikuXdgPositioner::State &state, BRect &pos)
{
	pos = BRect(
		BPoint(state.offset.x, state.offset.y),
		BSize(state.size.width - 1, state.size.height - 1)
	);

	switch (state.anchor) {
	case XdgPositioner::anchorTop:
	case XdgPositioner::anchorTopLeft:
	case XdgPositioner::anchorTopRight:
		pos.OffsetBy(0, state.anchorRect.y);
		break;
	case XdgPositioner::anchorBottom:
	case XdgPositioner::anchorBottomLeft:
	case XdgPositioner::anchorBottomRight:
		pos.OffsetBy(0, state.anchorRect.y + state.anchorRect.height);
		break;
	default:
		pos.OffsetBy(0, state.anchorRect.y + state.anchorRect.height/2);
		break;
	}

	switch (state.anchor) {
	case XdgPositioner::anchorLeft:
	case XdgPositioner::anchorTopLeft:
	case XdgPositioner::anchorBottomLeft:
		pos.OffsetBy(state.anchorRect.x, 0);
		break;
	case XdgPositioner::anchorRight:
	case XdgPositioner::anchorTopRight:
	case XdgPositioner::anchorBottomRight:
		pos.OffsetBy(state.anchorRect.x + state.anchorRect.width, 0);
		break;
	default:
		pos.OffsetBy(state.anchorRect.x + state.anchorRect.width/2, 0);
		break;
	}

	switch (state.gravity) {
	case XdgPositioner::gravityTop:
	case XdgPositioner::gravityTopLeft:
	case XdgPositioner::gravityTopRight:
		pos.OffsetBy(0, -(pos.Height() + 1));
		break;
	case XdgPositioner::gravityBottom:
	case XdgPositioner::gravityBottomLeft:
	case XdgPositioner::gravityBottomRight:
		break;
	default:
		pos.OffsetBy(0, -(((int32)pos.Height() + 1) / 2));
		break;
	}

	switch (state.gravity) {
	case XdgPositioner::gravityLeft:
	case XdgPositioner::gravityTopLeft:
	case XdgPositioner::gravityBottomLeft:
		pos.OffsetBy(-(pos.Width() + 1), 0);
		break;
	case XdgPositioner::gravityRight:
	case XdgPositioner::gravityTopRight:
	case XdgPositioner::gravityBottomRight:
		break;
	default:
		pos.OffsetBy(-(((int32)pos.Width() + 1) / 2), 0);
		break;
	}
}

static void GetConstrainedOffsets(BRect &offsets, const BRect &pos, const BRect &workArea)
{
	offsets.left   =  workArea.left   - pos.left;
	offsets.top    =  workArea.top    - pos.top;
	offsets.right  = -workArea.right  + pos.right;
	offsets.bottom = -workArea.bottom + pos.bottom;
}

static bool IsUnconstrained(const BRect &offsets)
{
	return
		offsets.left   <= 0 &&
		offsets.top    <= 0 &&
		offsets.right  <= 0 &&
		offsets.bottom <= 0;
}

static bool UnconstrainByFlip(const HaikuXdgPositioner::State &state, BRect &pos, BRect &offsets, const BRect &workArea)
{
	bool flipX = ((offsets.left > 0) ^ (offsets.right  > 0)) && (state.constraintAdjustment & XdgPositioner::constraintAdjustmentFlipX);
	bool flipY = ((offsets.top  > 0) ^ (offsets.bottom > 0)) && (state.constraintAdjustment & XdgPositioner::constraintAdjustmentFlipY);

	if (!flipX && !flipY)
		return false;

	HaikuXdgPositioner::State flipped = state;
	if (flipX) {
		switch (flipped.anchor) {
		case XdgPositioner::anchorLeft:        flipped.anchor = XdgPositioner::anchorRight;       break;
		case XdgPositioner::anchorRight:       flipped.anchor = XdgPositioner::anchorLeft;        break;
		case XdgPositioner::anchorTopLeft:     flipped.anchor = XdgPositioner::anchorTopRight;    break;
		case XdgPositioner::anchorBottomLeft:  flipped.anchor = XdgPositioner::anchorBottomRight; break;
		case XdgPositioner::anchorTopRight:    flipped.anchor = XdgPositioner::anchorTopLeft;     break;
		case XdgPositioner::anchorBottomRight: flipped.anchor = XdgPositioner::anchorBottomLeft;  break;
		default: break;
		}
		switch (flipped.gravity) {
		case XdgPositioner::gravityLeft:        flipped.gravity = XdgPositioner::gravityRight;       break;
		case XdgPositioner::gravityRight:       flipped.gravity = XdgPositioner::gravityLeft;        break;
		case XdgPositioner::gravityTopLeft:     flipped.gravity = XdgPositioner::gravityTopRight;    break;
		case XdgPositioner::gravityBottomLeft:  flipped.gravity = XdgPositioner::gravityBottomRight; break;
		case XdgPositioner::gravityTopRight:    flipped.gravity = XdgPositioner::gravityTopLeft;     break;
		case XdgPositioner::gravityBottomRight: flipped.gravity = XdgPositioner::gravityBottomLeft;  break;
		default: break;
		}
	}
	if (flipY) {
		switch (flipped.anchor) {
		case XdgPositioner::anchorTop:         flipped.anchor = XdgPositioner::anchorBottom;      break;
		case XdgPositioner::anchorBottom:      flipped.anchor = XdgPositioner::anchorTop;         break;
		case XdgPositioner::anchorTopLeft:     flipped.anchor = XdgPositioner::anchorBottomLeft;  break;
		case XdgPositioner::anchorBottomLeft:  flipped.anchor = XdgPositioner::anchorTopLeft;     break;
		case XdgPositioner::anchorTopRight:    flipped.anchor = XdgPositioner::anchorBottomRight; break;
		case XdgPositioner::anchorBottomRight: flipped.anchor = XdgPositioner::anchorTopRight;    break;
		default: break;
		}
		switch (flipped.gravity) {
		case XdgPositioner::gravityTop:         flipped.gravity = XdgPositioner::gravityBottom;      break;
		case XdgPositioner::gravityBottom:      flipped.gravity = XdgPositioner::gravityTop;         break;
		case XdgPositioner::gravityTopLeft:     flipped.gravity = XdgPositioner::gravityBottomLeft;  break;
		case XdgPositioner::gravityBottomLeft:  flipped.gravity = XdgPositioner::gravityTopLeft;     break;
		case XdgPositioner::gravityTopRight:    flipped.gravity = XdgPositioner::gravityBottomRight; break;
		case XdgPositioner::gravityBottomRight: flipped.gravity = XdgPositioner::gravityTopRight;    break;
		default: break;
		}
	}

	BRect flippedPos;
	BRect flippedOffsets;
	GetPosition(flipped, flippedPos);
	GetConstrainedOffsets(flippedOffsets, flippedPos, workArea);

	if (flippedOffsets.left <= 0 && flippedOffsets.right <= 0) {
		pos.left  = flippedPos.left;
		pos.right = flippedPos.right;
		offsets.left  = flippedOffsets.left;
		offsets.right = flippedOffsets.right;
	}
	if (flippedOffsets.top <= 0 && flippedOffsets.bottom <= 0) {
		pos.top    = flippedPos.top;
		pos.bottom = flippedPos.bottom;
		offsets.top    = flippedOffsets.top;
		offsets.bottom = flippedOffsets.bottom;
	}

	return IsUnconstrained(offsets);
}

static bool UnconstrainBySlide(const HaikuXdgPositioner::State &state, BRect &pos, BRect &offsets, const BRect &workArea)
{
	bool slideX = (offsets.left > 0 || offsets.right  > 0) && (state.constraintAdjustment & XdgPositioner::constraintAdjustmentSlideX);
	bool slideY = (offsets.top  > 0 || offsets.bottom > 0) && (state.constraintAdjustment & XdgPositioner::constraintAdjustmentSlideY);

	if (!slideX && !slideY)
		return false;

	if (slideX) {
		if (offsets.left > 0 && offsets.right > 0) {
			switch (state.gravity) {
				case XdgPositioner::gravityLeft:
				case XdgPositioner::gravityTopLeft:
				case XdgPositioner::gravityBottomLeft:
					pos.OffsetBy(-offsets.right, 0);
					break;
				default:
					pos.OffsetBy(offsets.left, 0);
					break;
			}
		} else {
			if (std::abs(offsets.left) < std::abs(offsets.right)) {
				pos.OffsetBy(offsets.left, 0);
			} else {
				pos.OffsetBy(-offsets.right, 0);
			}
		}
	}

	if (slideY) {
		if (offsets.top > 0 && offsets.bottom > 0) {
			switch (state.gravity) {
				case XdgPositioner::gravityTop:
				case XdgPositioner::gravityTopLeft:
				case XdgPositioner::gravityTopRight:
					pos.OffsetBy(0, -offsets.bottom);
					break;
				default:
					pos.OffsetBy(0, offsets.top);
					break;
			}
		} else {
			if (std::abs(offsets.top) < std::abs(offsets.bottom)) {
				pos.OffsetBy(0, offsets.top);
			} else {
				pos.OffsetBy(0, -offsets.bottom);
			}
		}
	}

	GetConstrainedOffsets(offsets, pos, workArea);
	return IsUnconstrained(offsets);
}

static bool UnconstrainByResize(const HaikuXdgPositioner::State &state, BRect &pos, BRect &offsets, const BRect &workArea)
{
	bool resizeX = (offsets.left > 0 || offsets.right  > 0) && (state.constraintAdjustment & XdgPositioner::constraintAdjustmentResizeX);
	bool resizeY = (offsets.top  > 0 || offsets.bottom > 0) && (state.constraintAdjustment & XdgPositioner::constraintAdjustmentResizeY);

	if (!resizeX && !resizeY)
		return false;

	if (offsets.left   < 0) offsets.left   = 0;
	if (offsets.right  < 0) offsets.right  = 0;
	if (offsets.top    < 0) offsets.top    = 0;
	if (offsets.bottom < 0) offsets.bottom = 0;

	BRect resizedPos = pos;
	if (resizeX) {
		resizedPos.OffsetBy(offsets.left, 0);
		resizedPos.right -= offsets.left + offsets.right;
	}
	if (resizeY) {
		resizedPos.OffsetBy(0, offsets.top);
		resizedPos.bottom -= offsets.top + offsets.bottom;
	}

	if (!resizedPos.IsValid())
		return false;

	GetConstrainedOffsets(offsets, pos, workArea);
	return IsUnconstrained(offsets);
}

static void UnconstrainPosition(const HaikuXdgPositioner::State &state, BRect &pos, const BRect &workArea)
{
	BRect offsets;
	GetConstrainedOffsets(offsets, pos, workArea);
	if (IsUnconstrained(offsets)) return;
	if (UnconstrainByFlip  (state, pos, offsets, workArea)) return;
	if (UnconstrainBySlide (state, pos, offsets, workArea)) return;
	if (UnconstrainByResize(state, pos, offsets, workArea)) return;
}


//#pragma mark - HaikuXdgPositioner

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


void HaikuXdgPositioner::GetPosition(BRect &pos, const BRect &workArea)
{
	::GetPosition(fState, pos);
	UnconstrainPosition(fState, pos, workArea);
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
