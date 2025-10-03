#include "XkbKeymap.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <string.h>

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

static uint32 GetOffsetForLevel(key_map *map, uint32 haikuKey, int level)
{
	if (haikuKey >= 128) return 0;

	switch (level) {
		case 0: return map->normal_map[haikuKey];
		case 1: return map->shift_map[haikuKey];
		case 2: return map->caps_map[haikuKey];
		case 3: return map->caps_shift_map[haikuKey];
		case 4: return map->option_map[haikuKey];
		case 5: return map->option_shift_map[haikuKey];
		case 6: return map->option_caps_map[haikuKey];
		case 7: return map->option_caps_shift_map[haikuKey];
		default: return map->normal_map[haikuKey];
	}
}

static uint32 GetTableMaskForLevel(int level)
{
	switch (level) {
		case 0: return B_NORMAL_TABLE;
		case 1: return B_SHIFT_TABLE;
		case 2: return B_CAPS_TABLE;
		case 3: return B_CAPS_SHIFT_TABLE;
		case 4: return B_OPTION_TABLE;
		case 5: return B_OPTION_SHIFT_TABLE;
		case 6: return B_OPTION_CAPS_TABLE;
		case 7: return B_OPTION_CAPS_SHIFT_TABLE;
		default: return 0;
	}
}

static uint32 GetCharacterAtOffset(char *keyBuffer, uint32 offset)
{
	if (offset == 0 || keyBuffer == NULL)
		return 0;

	uint32 len = (uint8)keyBuffer[offset];
	if (len == 0)
		return 0;

	const char *bytes = &keyBuffer[offset + 1];

	uint32 codepoint = 0;
	if (len == 1 && (uint8)bytes[0] < 0x80)
		codepoint = (uint32)bytes[0];
	else if (len > 1)
		codepoint = UTF8ToCharCode(&bytes);
	else
		codepoint = (uint8)bytes[0];

	return codepoint;
}

static bool IsDeadKeyForLevel(const key_map *map, char *keyBuffer, uint32 offset, int level)
{
	if (map == NULL || keyBuffer == NULL || offset == 0)
		return false;
	uint32 character = GetCharacterAtOffset(keyBuffer, offset);
	if (character == 0)
		return false;

	uint32 tableMask = GetTableMaskForLevel(level);
	if (tableMask == 0)
		return false;

	const int32 *deadKeyArrays[] = {
		map->acute_dead_key,
		map->grave_dead_key,
		map->circumflex_dead_key,
		map->dieresis_dead_key,
		map->tilde_dead_key
	};

	const uint32 tableMasks[] = {
		map->acute_tables,
		map->grave_tables,
		map->circumflex_tables,
		map->dieresis_tables,
		map->tilde_tables
	};

	for (int i = 0; i < 5; i++) {
		if ((tableMasks[i] & tableMask) == 0)
			continue;

		uint32 deadKeyOffset = deadKeyArrays[i][1];
		uint32 deadKeyChar = GetCharacterAtOffset(keyBuffer, deadKeyOffset);

		if (character == deadKeyChar)
			return true;
	}

	return false;
}

static void WriteSymbol(FILE *file, uint32 haikuKey, char *keyBuffer, uint32 offset, const key_map* map, int level)
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
		case 0x0e: fprintf(file, "Print"); return;

		case 0x4b: fprintf(file, "Shift_L"); return;
		case 0x56: fprintf(file, "Shift_R"); return;
		case 0x5c: fprintf(file, "Control_L"); return;
		case 0x60: fprintf(file, "Control_R"); return;
		case 0x5d: fprintf(file, "Alt_L"); return;
		case 0x5f: fprintf(file, "Alt_R"); return;
		case 0x66: fprintf(file, "Super_L"); return;
		case 0x67: fprintf(file, "Super_R"); return;
		case 0x68: fprintf(file, "Menu"); return;

		case 0x0f: fprintf(file, "Scroll_Lock"); return;
		case 0x10: fprintf(file, "Pause"); return;
		case 0x22: fprintf(file, "Num_Lock"); return;
		case 0x3b: fprintf(file, "Caps_Lock"); return;
		default: break;
	}

	if (offset == 0 || keyBuffer == NULL) {
		fprintf(file, "NoSymbol");
		return;
	}

	uint32 len = (uint8)keyBuffer[offset];
	if (len == 0) {
		fprintf(file, "NoSymbol");
		return;
	}

	const char *bytes = &keyBuffer[offset + 1];

	if (len == 1) {
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
			case B_SPACE:       fprintf(file, "space"); return;
			default: break;
		}
	}

	uint32 codepoint = 0;
	if (len == 1 && (uint8)bytes[0] < 0x80)
		codepoint = (uint32)bytes[0];
	else if (len > 1)
		codepoint = UTF8ToCharCode(&bytes);
	else
		codepoint = (uint8)bytes[0];

	if (codepoint == 0) {
		fprintf(file, "NoSymbol");
		return;
	}

	if (map != NULL && IsDeadKeyForLevel(map, keyBuffer, offset, level)) {
		const char* dead = NULL;
		switch (codepoint) {
			case 0x0027:
				dead = "dead_acute";
				break;
			case 0x0022:
				dead = "dead_diaeresis";
				break;
			case 0x0060:
				dead = "dead_grave";
				break;
			case 0x007E:
				dead = "dead_tilde";
				break;
			case 0x005E:
				dead = "dead_circumflex";
				break;
			case 0x00B4:
				dead = "dead_acute";
				break;
			case 0x00A8:
				dead = "dead_diaeresis";
				break;
			default:
				break;
		}
		if (dead != NULL) {
			fprintf(file, "%s", dead);
			return;
		}
	}

	switch (codepoint) {
		case 32:  fprintf(file, "space"); return;
		case 33:  fprintf(file, "exclam"); return;
		case 34:  fprintf(file, "quotedbl"); return;
		case 35:  fprintf(file, "numbersign"); return;
		case 36:  fprintf(file, "dollar"); return;
		case 37:  fprintf(file, "percent"); return;
		case 38:  fprintf(file, "ampersand"); return;
		case 39:  fprintf(file, "apostrophe"); return;
		case 40:  fprintf(file, "parenleft"); return;
		case 41:  fprintf(file, "parenright"); return;
		case 42:  fprintf(file, "asterisk"); return;
		case 43:  fprintf(file, "plus"); return;
		case 44:  fprintf(file, "comma"); return;
		case 45:  fprintf(file, "minus"); return;
		case 46:  fprintf(file, "period"); return;
		case 47:  fprintf(file, "slash"); return;
		case 58:  fprintf(file, "colon"); return;
		case 59:  fprintf(file, "semicolon"); return;
		case 60:  fprintf(file, "less"); return;
		case 61:  fprintf(file, "equal"); return;
		case 62:  fprintf(file, "greater"); return;
		case 63:  fprintf(file, "question"); return;
		case 64:  fprintf(file, "at"); return;
		case 91:  fprintf(file, "bracketleft"); return;
		case 92:  fprintf(file, "backslash"); return;
		case 93:  fprintf(file, "bracketright"); return;
		case 94:  fprintf(file, "asciicircum"); return;
		case 95:  fprintf(file, "underscore"); return;
		case 96:  fprintf(file, "grave"); return;
		case 123: fprintf(file, "braceleft"); return;
		case 124: fprintf(file, "bar"); return;
		case 125: fprintf(file, "braceright"); return;
		case 126: fprintf(file, "asciitilde"); return;
		default: break;
	}

	if (codepoint >= 32 && codepoint != 127)
		fprintf(file, "U%04X", codepoint);
	else
		fprintf(file, "NoSymbol");
}

static void GenerateKeycodes(FILE *file)
{
	fprintf(file, "\txkb_keycodes \"(unnamed)\" {\n");
	int minimum = 8;
	int maximum = 708;
	fprintf(file, "\t\tminimum = %d;\n", minimum);
	fprintf(file, "\t\tmaximum = %d;\n", maximum);

	for (int i = minimum + 1; i <= maximum; i++)
		fprintf(file, "\t\t<I%d> = %d;\n", i, i);

	fprintf(file, "\t};\n");
}

static void GenerateTypes(FILE *file)
{
	fprintf(file, "\txkb_types \"(unnamed)\" {\n");

	fprintf(file, "\t\ttype \"FOUR_LEVEL\" {\n");
	fprintf(file, "\t\t\tmodifiers = Shift+Mod5;\n");
	fprintf(file, "\t\t\tmap[Shift] = 2;\n");
	fprintf(file, "\t\t\tmap[Mod5] = 3;\n");
	fprintf(file, "\t\t\tmap[Mod5+Shift] = 4;\n");
	fprintf(file, "\t\t\tlevel_name[1] = \"Base\";\n");
	fprintf(file, "\t\t\tlevel_name[2] = \"Shift\";\n");
	fprintf(file, "\t\t\tlevel_name[3] = \"AltGr\";\n");
	fprintf(file, "\t\t\tlevel_name[4] = \"AltGr+Shift\";\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\ttype \"KEYPAD\" {\n");
	fprintf(file, "\t\t\tmodifiers = Shift+Mod2;\n");
	fprintf(file, "\t\t\tmap[None] = 1;\n");
	fprintf(file, "\t\t\tmap[Shift] = 1;\n");
	fprintf(file, "\t\t\tmap[Mod2] = 2;\n");
	fprintf(file, "\t\t\tmap[Shift+Mod2] = 2;\n");
	fprintf(file, "\t\t\tlevel_name[1] = \"Base\";\n");
	fprintf(file, "\t\t\tlevel_name[2] = \"Number\";\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t};\n");
}

static void GenerateCompatibility(FILE *file)
{
	fprintf(file, "\txkb_compatibility \"(unnamed)\" {\n");

	fprintf(file, "\t\tinterpret Shift_L+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetMods(modifiers=Shift);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Shift_R+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetMods(modifiers=Shift);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Control_L+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetMods(modifiers=Control);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Control_R+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetMods(modifiers=Control);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Alt_L+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetMods(modifiers=Mod1);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Alt_R+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetMods(modifiers=Mod5);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Super_L+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetGroup(group=2);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Super_R+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = SetGroup(group=2);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Num_Lock+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = LockMods(modifiers=Mod2);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t\tinterpret Caps_Lock+AnyOf(all) {\n");
	fprintf(file, "\t\t\tuseModMapMods = level1;\n");
	fprintf(file, "\t\t\taction = LockMods(modifiers=Lock);\n");
	fprintf(file, "\t\t};\n");

	fprintf(file, "\t};\n");
}

static bool IsNumpadDigitKey(uint32 haikuKey)
{
	return (haikuKey >= 0x37 && haikuKey <= 0x39) ||  // KP7-KP9
		   (haikuKey >= 0x48 && haikuKey <= 0x4a) ||  // KP4-KP6
		   (haikuKey >= 0x58 && haikuKey <= 0x5a) ||  // KP1-KP3
		   (haikuKey == 0x64 || haikuKey == 0x65);    // KP0, KP_Decimal
}

static bool IsNumpadOperatorKey(uint32 haikuKey)
{
	return (haikuKey >= 0x23 && haikuKey <= 0x25) ||  // KP_Divide, KP_Multiply, KP_Subtract
		   (haikuKey == 0x3a) ||                       // KP_Add
		   (haikuKey == 0x5b);                         // KP_Enter
}

static void WriteNumpadDigitSymbol(FILE *file, uint32 haikuKey)
{
	switch (haikuKey) {
		case 0x37: fprintf(file, "KP_Home, KP_7"); break;      // KP_7
		case 0x38: fprintf(file, "KP_Up, KP_8"); break;        // KP_8
		case 0x39: fprintf(file, "KP_Prior, KP_9"); break;     // KP_9
		case 0x48: fprintf(file, "KP_Left, KP_4"); break;      // KP_4
		case 0x49: fprintf(file, "KP_Begin, KP_5"); break;     // KP_5
		case 0x4a: fprintf(file, "KP_Right, KP_6"); break;     // KP_6
		case 0x58: fprintf(file, "KP_End, KP_1"); break;       // KP_1
		case 0x59: fprintf(file, "KP_Down, KP_2"); break;      // KP_2
		case 0x5a: fprintf(file, "KP_Next, KP_3"); break;      // KP_3
		case 0x64: fprintf(file, "KP_Insert, KP_0"); break;    // KP_0
		case 0x65: fprintf(file, "KP_Delete, KP_Decimal"); break; // KP_Decimal
		default: fprintf(file, "NoSymbol, NoSymbol"); break;
	}
}

static void WriteNumpadOperatorSymbol(FILE *file, uint32 haikuKey)
{
	switch (haikuKey) {
		case 0x23: fprintf(file, "KP_Divide, KP_Divide, KP_Divide, KP_Divide"); break;     // KP_Divide
		case 0x24: fprintf(file, "KP_Multiply, KP_Multiply, KP_Multiply, KP_Multiply"); break; // KP_Multiply
		case 0x25: fprintf(file, "KP_Subtract, KP_Subtract, KP_Subtract, KP_Subtract"); break; // KP_Subtract
		case 0x3a: fprintf(file, "KP_Add, KP_Add, KP_Add, KP_Add"); break;                 // KP_Add
		case 0x5b: fprintf(file, "KP_Enter, KP_Enter, KP_Enter, KP_Enter"); break;         // KP_Enter
		default: fprintf(file, "NoSymbol, NoSymbol, NoSymbol, NoSymbol"); break;
	}
}

static void GenerateSymbols(FILE *file, key_map *map, char *keyBuffer)
{
	fprintf(file, "\txkb_symbols \"(unnamed)\" {\n");
	fprintf(file, "\t\tname[Group1]=\"Current\";\n");
	fprintf(file, "\t\tname[Group2]=\"Latin\";\n");

	struct USLayout {
		uint32 haikuKey;
		const char* symbols[4]; // Base, Shift, AltGr, AltGr+Shift
	} usLayout[] = {
		{0x27, {"q", "Q", "NoSymbol", "NoSymbol"}},
		{0x28, {"w", "W", "NoSymbol", "NoSymbol"}},
		{0x29, {"e", "E", "NoSymbol", "NoSymbol"}},
		{0x2a, {"r", "R", "NoSymbol", "NoSymbol"}},
		{0x2b, {"t", "T", "NoSymbol", "NoSymbol"}},
		{0x2c, {"y", "Y", "NoSymbol", "NoSymbol"}},
		{0x2d, {"u", "U", "NoSymbol", "NoSymbol"}},
		{0x2e, {"i", "I", "NoSymbol", "NoSymbol"}},
		{0x2f, {"o", "O", "NoSymbol", "NoSymbol"}},
		{0x30, {"p", "P", "NoSymbol", "NoSymbol"}},
		{0x3c, {"a", "A", "NoSymbol", "NoSymbol"}},
		{0x3d, {"s", "S", "NoSymbol", "NoSymbol"}},
		{0x3e, {"d", "D", "NoSymbol", "NoSymbol"}},
		{0x3f, {"f", "F", "NoSymbol", "NoSymbol"}},
		{0x40, {"g", "G", "NoSymbol", "NoSymbol"}},
		{0x41, {"h", "H", "NoSymbol", "NoSymbol"}},
		{0x42, {"j", "J", "NoSymbol", "NoSymbol"}},
		{0x43, {"k", "K", "NoSymbol", "NoSymbol"}},
		{0x44, {"l", "L", "NoSymbol", "NoSymbol"}},
		{0x4c, {"z", "Z", "NoSymbol", "NoSymbol"}},
		{0x4d, {"x", "X", "NoSymbol", "NoSymbol"}},
		{0x4e, {"c", "C", "NoSymbol", "NoSymbol"}},
		{0x4f, {"v", "V", "NoSymbol", "NoSymbol"}},
		{0x50, {"b", "B", "NoSymbol", "NoSymbol"}},
		{0x51, {"n", "N", "NoSymbol", "NoSymbol"}},
		{0x52, {"m", "M", "NoSymbol", "NoSymbol"}},
		{0x12, {"1", "exclam", "NoSymbol", "NoSymbol"}},
		{0x13, {"2", "at", "NoSymbol", "NoSymbol"}},
		{0x14, {"3", "numbersign", "NoSymbol", "NoSymbol"}},
		{0x15, {"4", "dollar", "NoSymbol", "NoSymbol"}},
		{0x16, {"5", "percent", "NoSymbol", "NoSymbol"}},
		{0x17, {"6", "asciicircum", "NoSymbol", "NoSymbol"}},
		{0x18, {"7", "ampersand", "NoSymbol", "NoSymbol"}},
		{0x19, {"8", "asterisk", "NoSymbol", "NoSymbol"}},
		{0x1a, {"9", "parenleft", "NoSymbol", "NoSymbol"}},
		{0x1b, {"0", "parenright", "NoSymbol", "NoSymbol"}},
		{0, {NULL, NULL, NULL, NULL}}
	};

	for (uint32 haikuKey = 0x01; haikuKey <= 0x6b; haikuKey++) {
		uint32 wlKey = FromHaikuKeyCode(haikuKey);
		if (wlKey == 0) continue;

		if (IsNumpadDigitKey(haikuKey)) {
			fprintf(file, "\t\tkey <I%" B_PRIu32 "> {type = \"KEYPAD\", symbols[Group1] = [", wlKey + 8);
			WriteNumpadDigitSymbol(file, haikuKey);
			fprintf(file, "]};\n");
		} else if (IsNumpadOperatorKey(haikuKey)) {
			fprintf(file, "\t\tkey <I%" B_PRIu32 "> {type = \"FOUR_LEVEL\", symbols[Group1] = [", wlKey + 8);
			WriteNumpadOperatorSymbol(file, haikuKey);
			fprintf(file, "], symbols[Group2] = [");
			WriteNumpadOperatorSymbol(file, haikuKey);
			fprintf(file, "]};\n");
		} else {
			fprintf(file, "\t\tkey <I%" B_PRIu32 "> {type = \"FOUR_LEVEL\", symbols[Group1] = [", wlKey + 8);

			// Group1 - current keymap
			uint32 offsets[4] = {
				GetOffsetForLevel(map, haikuKey, 0), // normal
				GetOffsetForLevel(map, haikuKey, 1), // shift
				GetOffsetForLevel(map, haikuKey, 4), // option (AltGr)
				GetOffsetForLevel(map, haikuKey, 5)  // option+shift
			};

			int levels[4] = {0, 1, 4, 5};
			for (int level = 0; level < 4; level++) {
				WriteSymbol(file, haikuKey, keyBuffer, offsets[level], map, levels[level]);
				if (level < 3) fprintf(file, ", ");
			}

			// Group2 - latin keymap for hotkeys
			fprintf(file, "], symbols[Group2] = [");

			bool foundUS = false;
			for (int i = 0; usLayout[i].haikuKey != 0; i++) {
				if (usLayout[i].haikuKey == haikuKey) {
					for (int level = 0; level < 4; level++) {
						fprintf(file, "%s", usLayout[i].symbols[level]);
						if (level < 3) fprintf(file, ", ");
					}
					foundUS = true;
					break;
				}
			}

			if (!foundUS) {
				for (int level = 0; level < 4; level++) {
					WriteSymbol(file, haikuKey, keyBuffer, offsets[level], NULL, levels[level]);
					if (level < 3) fprintf(file, ", ");
				}
			}

			fprintf(file, "]};\n");
		}
	}

	fprintf(file, "\t\tmodifier_map Shift {<I50>, <I62>};\n");
	fprintf(file, "\t\tmodifier_map Lock {<I66>};\n");
	fprintf(file, "\t\tmodifier_map Control {<I37>, <I105>};\n");
	fprintf(file, "\t\tmodifier_map Mod1 {<I64>};\n");            // Left Alt
	fprintf(file, "\t\tmodifier_map Mod2 {<I77>};\n");            // NumLock
	fprintf(file, "\t\tmodifier_map Mod4 {<I133>, <I134>};\n");   // Super (Command)
	fprintf(file, "\t\tmodifier_map Mod5 {<I108>};\n");           // Right Alt (AltGr)

	fprintf(file, "\t};\n");
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

	GenerateKeycodes(file);
	GenerateTypes(file);
	GenerateCompatibility(file);
	GenerateSymbols(file, map, keyBuffer);

	fprintf(file, "};\n");
	fputc(0, file);

	fdCloser.Detach();
	return B_OK;
}
