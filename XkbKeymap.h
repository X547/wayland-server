#pragma once
#include <SupportDefs.h>


uint32_t FromHaikuKeyCode(uint32 haikuKey);

status_t ProduceXkbKeymap(int &fd);
