
#include "workshop.h"
#include "veh.h"
#include "gui.h"
#include "game.h"
#include "patch.h"

static int __thiscall workshop_can_add(DesignWindow* This) {
    return bit_count(This->unit_ability_flags) < ability_slot_limit(This->faction_id);
}

static void __thiscall workshop_setup_veh(DesignWindow* This, int unit_id, int a3) {
    DesignWin_setup_veh(This, unit_id, a3);
    workshop_check_abilities(This);
}

static int __thiscall workshop_design_it(DesignWindow* This) {
    workshop_check_abilities(This);
    return DesignWin_design_it(This);
}

static RECT ability_row(DesignWindow* This, int slot) {
    RECT rc = This->rcSpecial2;
    int middle = (rc.top + rc.bottom) / 2;
    if (slot == 1) {
        rc.bottom = middle;
    } else {
        rc.top = middle;
    }
    return rc;
}

static const char* ability_text(DesignWindow* This, int slot) {
    int id = workshop_ability(This, slot);
    return id >= 0 ? Ability[id].name : label_get(25); // original "None" label
}

static void __thiscall workshop_draw_special(DesignWindow* This, int UNUSED(a2)) {
    RECT box = This->rcSpecial2;
    Buffer* canvas = &This->oCanvas;
    Buffer_set_clip(canvas, &box);
    Buffer_copy_2(&This->buffer, canvas, &box);
    Buffer_set_font(canvas, &This->font1, 0, 0, 0);
    for (int slot = 1; slot < 3; slot++) {
        RECT rc = ability_row(This, slot);
        bool locked = slot >= ability_slot_limit(This->faction_id);
        if (slot == 2) {
            Buffer_line_2(canvas, rc.left, rc.top, rc.right - 1, rc.top, 0x474F39);
        }
        char text[128];
        snprintf(text, sizeof(text), "%d: %s", slot + 1, locked ? "Locked" : ability_text(This, slot));
        Buffer_set_text_color(canvas, locked ? ColorMediumBlue : ColorMineralLight, 0, 1, 1);
        int length = strlen(text);
        while (length > 4 && Buffer_text_width_2(canvas, text, length) > rc.right - rc.left - 8) {
            text[--length] = 0;
            memcpy(text + length - 3, "...", 3);
        }
        Buffer_write_cent_l_7(canvas, text, &rc, length);
    }
}

static void __thiscall workshop_left_click(DesignWindow* This, int x, int y) {
    RECT rc = ability_row(This, 2);
    POINT pt = {x, y};
    if (PtInRect(&rc, pt)) {
        if (ability_slot_limit(This->faction_id) >= 3) {
            DesignWin_select_special(This, 2);
        }
        return;
    }
    DesignWin_on_left_click(This, x, y);
}

static void __thiscall workshop_right_click(DesignWindow* This, int x, int y) {
    POINT pt = {x, y};
    if (!PtInRect(&This->rcSpecial2, pt)) {
        DesignWin_on_right_click(This, x, y);
        return;
    }
    RECT rc = ability_row(This, 2);
    int slot = PtInRect(&rc, pt) ? 2 : 1;
    int tech = slot == 1 ? Rules->tech_preq_allow_2_spec_abil : conf.third_ability_tech;
    static char text[256];
    if (slot >= ability_slot_limit(This->faction_id)) {
        snprintf(text, sizeof(text), "Special ability %d: requires %s%s", slot + 1,
            tech >= 0 ? Tech[tech].name : "an unavailable technology",
            slot == 2 ? " and the two-ability prerequisite." : ".");
    } else {
        snprintf(text, sizeof(text), "Special ability %d: %s", slot + 1, ability_text(This, slot));
    }
    Win_client_to_screen_2(This, &x, &y);
    RECT anchor = {x, y, x, y};
    Win_set_bubble_text(This, text, &anchor);
}

static int __thiscall workshop_preview_abilities(Buffer* canvas, char* text, int x, int y, int len) {
    int result = Buffer_write_l_3(canvas, text, x, y, len);
    size_t canvas_offset = (byte*)&DesignWin->oCanvas - (byte*)DesignWin;
    DesignWindow* This = (DesignWindow*)((byte*)canvas - canvas_offset);
    if (ability_slot_limit(This->faction_id) >= 3) {
        int id = workshop_ability(This, 2);
        char extra[32];
        snprintf(extra, sizeof(extra), "3: %.12s", id >= 0 ? Ability[id].abbreviation : "-");
        Font* font = &This->font2;
        int pitch = font->field_0 >= 0 ? font->field_0 + font->height : font->lineHeight;
        Buffer_write_l_3(canvas, extra, x, y + pitch, strlen(extra));
    }
    return result;
}

/*
Small cdecl/thiscall adapters inside the original picker. Keep its native
technology, faction FREEABIL, component checks, cost display and Cancel path.
All replaced bytes are checked against Alien Crossfire 2.0 before writing.
*/
static void workshop_call(int addr, int original, int replacement) {
    byte old_bytes[5] = {0xE8};
    byte new_bytes[5] = {0xE8};
    int old_offset = original - addr - 5;
    int new_offset = replacement - addr - 5;
    memcpy(old_bytes + 1, &old_offset, sizeof(old_offset));
    memcpy(new_bytes + 1, &new_offset, sizeof(new_offset));
    write_bytes(addr, old_bytes, new_bytes, sizeof(old_bytes));
}

void patch_workshop() {
    // The native prototype picker ignores many ability differences (including
    // Clean Reactor and Fuel Nanocells). Match the complete mask so requested
    // three-ability designs are not silently replaced by one/two-ability units.
    const int ability_mask = (1u << MaxAbilityNum) - 1;
    write_word(0x580B52, 0x0044AE1B, ability_mask);
    write_word(0x580BF9, 0x0048841A, ability_mask);

    const byte check[] = {0x57, 0x8B, 0xF9, 0x8B, 0x87, 0xFC, 0x41, 0x01, 0x00};
    write_bytes(0x439C00, check, NULL, sizeof(check));
    write_jump(0x439C00, (int)workshop_check_abilities);

    const byte filter[] = {
        0x33, 0xD2, 0x33, 0xC0, 0x8D, 0x8E, 0x18, 0x42, 0x01, 0x00, 0x3B, 0x45,
        0x08, 0x74, 0x0C, 0x8B, 0x7D, 0xEC, 0x39, 0x39, 0x75, 0x05, 0xBA, 0x01,
        0x00, 0x00, 0x00, 0x40, 0x83, 0xC1, 0x04, 0x83, 0xF8, 0x02, 0x7C, 0xE6,
    };
    write_bytes(0x43AFB9, filter, NULL, sizeof(filter));
    // push [ebp-14h]; push [ebp+8]; push esi; call; add esp,12; mov edx,eax
    const byte filter_call[] = {0xFF,0x75,0xEC, 0xFF,0x75,0x08, 0x56,
        0xE8,0,0,0,0, 0x83,0xC4,0x0C, 0x8B,0xD0};
    memcpy((void*)0x43AFB9, filter_call, sizeof(filter_call));
    write_call(0x43AFC0, (int)workshop_exclude_ability);

    const byte commit[] = {
        0x8B,0x55,0x08, 0x89,0x84,0x96,0x18,0x42,0x01,0x00,
        0xC7,0x86,0x08,0x42,0x01,0x00,0,0,0,0, 0x8D,0x86,0x18,0x42,0x01,0x00,
        0xBA,0x02,0,0,0, 0x8B,0x08,0x85,0xC9,0x7C,0x0D, 0xBF,0x01,0,0,0,
        0xD3,0xE7,0x09,0xBE,0x08,0x42,0x01,0x00,0x83,0xC0,0x04,0x4A,0x75,0xE7,
    };
    write_bytes(0x43BAE3, commit, NULL, sizeof(commit));
    // push eax; push [ebp+8]; push esi; call; add esp,12
    const byte commit_call[] = {0x50, 0xFF,0x75,0x08, 0x56,
        0xE8,0,0,0,0, 0x83,0xC4,0x0C};
    memcpy((void*)0x43BAE3, commit_call, sizeof(commit_call));
    write_call(0x43BAE8, (int)workshop_select_ability);

    const byte auto_add[] = {
        0x8B,0x86,0x08,0x42,0x01,0x00,0x50,0xE8,0x71,0x1C,0x0D,0x00,0x83,0xC4,0x04,
        0x83,0xF8,0x01,0x0F,0x8F,0xCF,0,0,0,0x8B,0x8E,0x08,0x42,0x01,0x00,0x51,
        0xE8,0x59,0x1C,0x0D,0x00,0x83,0xC4,0x04,0x85,0xC0,0x74,0x1D,
        0x8B,0x96,0xF4,0x41,0x01,0x00,0xA1,0x80,0x97,0x94,0x00,0x52,0x50,
        0xE8,0x30,0x01,0x18,0x00,0x83,0xC4,0x08,0x85,0xC0,0x0F,0x84,0x9F,0,0,0,
    };
    write_bytes(0x439DB3, auto_add, NULL, sizeof(auto_add));
    // mov ecx,esi; call; test eax,eax; jz original_full
    const byte auto_call[] = {0x8B,0xCE,0xE8,0,0,0,0,0x85,0xC0,0x0F,0x84,0,0,0,0};
    memcpy((void*)0x439DB3, auto_call, sizeof(auto_call));
    write_call(0x439DB5, (int)workshop_can_add);
    *(int*)0x439DBE = 0x439E9A - 0x439DC2;

    // Validate both when opening/switching designs and immediately before committing.
    const int setup_calls[] = {0x438578,0x4385FA,0x43872E,0x43891D,
        0x438A74,0x438E0A,0x43D26B,0x43EA94};
    for (int addr : setup_calls) {
        workshop_call(addr, (int)DesignWin_setup_veh, (int)workshop_setup_veh);
    }
    for (int addr : {0x43853F,0x438563,0x4385E4}) {
        workshop_call(addr, (int)DesignWin_design_it, (int)workshop_design_it);
    }

    const byte draw[] = {0x55,0x8B,0xEC,0x83,0xEC,0x20};
    write_bytes(0x435DE0, draw, NULL, sizeof(draw));
    write_jump(0x435DE0, (int)workshop_draw_special);
    workshop_call(0x43798B, (int)Buffer_write_l_3, (int)workshop_preview_abilities);
    // The window vtable is in read-only data, outside the normal code patch range.
    DWORD old_protect;
    DWORD unused;
    if (!VirtualProtect((void*)0x66B194, 16, PAGE_READWRITE, &old_protect)) {
        exit_fail(0x66B194);
    }
    write_word(0x66B194, (int)DesignWin_on_left_click, (int)workshop_left_click);
    write_word(0x66B1A0, (int)DesignWin_on_right_click, (int)workshop_right_click);
    if (!VirtualProtect((void*)0x66B194, 16, old_protect, &unused)) {
        exit_fail(0x66B194);
    }
}
