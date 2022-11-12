#include "XkbKeymap.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>

#include <InterfaceDefs.h>
#include <AutoDeleter.h>
#include <AutoDeleterPosix.h>
#include <utf8_functions.h>

#include "WaylandKeycodes.h"


static void RandName(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A' + (r&15) + (r&16)*2;
		r >>= 5;
	}
}

static int CreateMemoryFile()
{
	char name[] = "/wayland-XXXXXX";
	int retries = 100;

	do {
		RandName(name + strlen(name) - 6);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}


static void WriteSymbol(FILE *file, uint32 haikuKey, char *keyBuffer, uint32 offset)
{
	switch (haikuKey) {
		case 0x02: fprintf(file, "F1"); return;
		case 0x03: fprintf(file, "F2"); return;
		case 0x04: fprintf(file, "F3"); return;
		case 0x05: fprintf(file, "F4"); return;
		case 0x06: fprintf(file, "F5"); return;
		case 0x07: fprintf(file, "F6"); return;
		case 0x08: fprintf(file, "F7"); return;
		case 0x09: fprintf(file, "F8"); return;
		case 0x0a: fprintf(file, "F9"); return;
		case 0x0b: fprintf(file, "F10"); return;
		case 0x0c: fprintf(file, "F11"); return;
		case 0x0d: fprintf(file, "F12"); return;
		default: break;
	}
	uint32 codepoint = 0;
	uint32 len = (uint8)keyBuffer[offset];
	if (len > 0) {
		const char *bytes = &keyBuffer[offset + 1];
		switch (bytes[0]) {
			case B_BACKSPACE:   fprintf(file, "BackSpace"); return;
			case B_RETURN:      fprintf(file, "Return"); return;
			case B_TAB:         fprintf(file, "Tab"); return;
			case B_ESCAPE:      fprintf(file, "Escape"); return;
			case B_LEFT_ARROW:  fprintf(file, "Left"); return;
			case B_RIGHT_ARROW: fprintf(file, "Right"); return;
			case B_UP_ARROW:    fprintf(file, "Up"); return;
			case B_DOWN_ARROW:  fprintf(file, "Down"); return;
			case B_INSERT:      fprintf(file, "Insert"); return;
			case B_DELETE:      fprintf(file, "Delete"); return;
			case B_HOME:        fprintf(file, "Home"); return;
			case B_END:         fprintf(file, "End"); return;
			case B_PAGE_UP:     fprintf(file, "Prior"); return;
			case B_PAGE_DOWN:   fprintf(file, "Next"); return;
			default: break;
		}
		codepoint = UTF8ToCharCode(&bytes);
	}
	fprintf(file, "U%04" B_PRIx32, codepoint);
}

static void GenerateSymbols(FILE *file, key_map *map, char *keyBuffer)
{
	for (uint32 haikuKey = 0x01; haikuKey <= 0x6b; haikuKey++) {
		uint32 wlKey = FromHaikuKeyCode(haikuKey);
		if (wlKey == 0) continue;
		fprintf(file, "\t\tkey <I%" B_PRIu32 "> {type = \"Haiku\", symbols[Group1] = [", wlKey + 8);
		WriteSymbol(file, haikuKey, keyBuffer, map->normal_map[haikuKey]); fprintf(file, ", ");
		WriteSymbol(file, haikuKey, keyBuffer, map->shift_map[haikuKey]); fprintf(file, ", ");
		WriteSymbol(file, haikuKey, keyBuffer, map->caps_map[haikuKey]); fprintf(file, ", ");
		WriteSymbol(file, haikuKey, keyBuffer, map->caps_shift_map[haikuKey]); fprintf(file, ", ");
		WriteSymbol(file, haikuKey, keyBuffer, map->option_map[haikuKey]); fprintf(file, ", ");
		WriteSymbol(file, haikuKey, keyBuffer, map->option_shift_map[haikuKey]); fprintf(file, ", ");
		WriteSymbol(file, haikuKey, keyBuffer, map->option_caps_map[haikuKey]); fprintf(file, ", ");
		WriteSymbol(file, haikuKey, keyBuffer, map->option_caps_shift_map[haikuKey]); //fprintf(file, ", ");
		//WriteSymbol(file, haikuKey, keyBuffer, map->control_map[haikuKey]);
		fprintf(file, "]};\n");
	}
}

status_t ProduceXkbKeymap(int &fd)
{
	key_map *map;
	char *keyBuffer;
	get_key_map(&map, &keyBuffer);
	MemoryDeleter mapDeleter(map);
	MemoryDeleter keyBufferDeleter(keyBuffer);
	if (!mapDeleter.IsSet() || !keyBufferDeleter.IsSet())
		return B_NO_MEMORY;

	fd = CreateMemoryFile();
	FileDescriptorCloser fdCloser(fd);
	if (!fdCloser.IsSet())
		return B_ERROR; // Use `errno`?

	FILE *file = fdopen(dup(fd), "w");
	FileCloser fileCloser(file);
	if (!fileCloser.IsSet())
		return B_ERROR; // Use `errno`?

	fprintf(file, "xkb_keymap {\n");

	fprintf(file, "\txkb_keycodes \"(unnamed)\" {\n");
	int minimum = 8;
	int maximum = 708;
	fprintf(file, "\t\tminimum = %d;\n", minimum);
	fprintf(file, "\t\tmaximum = %d;\n", maximum);
	for (int i = minimum + 1; i <= maximum; i++) {
		fprintf(file, "\t\t<I%d> = %d;\n", i, i);
	}
	fprintf(file, "\t};\n");

	fprintf(file, "\txkb_types \"(unnamed)\" {\n");
	fprintf(file, "\t\tvirtual_modifiers NumLock,Alt,LevelThree,LevelFive,Meta,Super,Hyper,ScrollLock;\n");
	fprintf(file, "\t\ttype \"Haiku\" {\n");
	fprintf(file, "\t\t\tmodifiers = Shift+Lock+Mod4;\n");
	fprintf(file, "\t\t\tmap[Shift] = 2;\n");
	fprintf(file, "\t\t\tmap[Lock] = 3;\n");
	fprintf(file, "\t\t\tmap[Shift+Lock] = 4;\n");
	fprintf(file, "\t\t\tmap[Mod4] = 5;\n");
	fprintf(file, "\t\t\tmap[Mod4+Shift] = 6;\n");
	fprintf(file, "\t\t\tmap[Mod4+Lock] = 7;\n");
	fprintf(file, "\t\t\tmap[Mod4+Lock+Shift] = 8;\n");
	//fprintf(file, "\t\t\tmap[Control] = 9;\n");
	fprintf(file, "\t\t\tlevel_name[1] = \"Base\";\n");
	fprintf(file, "\t\t\tlevel_name[2] = \"Shift\";\n");
	fprintf(file, "\t\t\tlevel_name[3] = \"Caps\";\n");
	fprintf(file, "\t\t\tlevel_name[4] = \"Caps+Shift\";\n");
	fprintf(file, "\t\t\tlevel_name[5] = \"Option\";\n");
	fprintf(file, "\t\t\tlevel_name[6] = \"Option+Shift\";\n");
	fprintf(file, "\t\t\tlevel_name[7] = \"Option+Caps\";\n");
	fprintf(file, "\t\t\tlevel_name[8] = \"Option+Caps+Shift\";\n");
	//fprintf(file, "\t\t\tlevel_name[9] = \"Control\";\n");
	fprintf(file, "\t\t};\n");
	fprintf(file, "\t};\n");

	fprintf(file, "\txkb_compatibility \"(unnamed)\" {\n");
	fprintf(file, "\t};\n");

	fprintf(file, "\txkb_symbols \"(unnamed)\" {\n");
	fprintf(file, "\t\tname[Group1]=\"Unnamed\";\n");
	GenerateSymbols(file, map, keyBuffer);
	fprintf(file, "\t\tmodifier_map Shift {<I50>, <I62>};\n");
	fprintf(file, "\t\tmodifier_map Lock {<I66>};\n");
	fprintf(file, "\t\tmodifier_map Control {<I37>, <I105>};\n");
	fprintf(file, "\t\tmodifier_map Mod1 {<I64>, <I108>, <I204>, <I205>};\n"); // Alt
	fprintf(file, "\t\tmodifier_map Mod2 {<I77>};\n");
	fprintf(file, "\t\tmodifier_map Mod3 {<I203>};\n");
	fprintf(file, "\t\tmodifier_map Mod4 {<I133>, <I134>, <I206>, <I207>};\n"); // Option
	fprintf(file, "\t\tmodifier_map Mod5 {<I92>};\n");
	fprintf(file, "\t};\n");

	fprintf(file, "};\n");

	fdCloser.Detach();
	return B_OK;
}
