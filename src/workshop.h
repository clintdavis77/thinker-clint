#pragma once

#include "main.h"

int workshop_ability(DesignWindow* This, int slot);
int __thiscall workshop_check_abilities(DesignWindow* This);
int __cdecl workshop_exclude_ability(DesignWindow* This, int slot, int ability_id);
void __cdecl workshop_select_ability(DesignWindow* This, int slot, int ability_id);
void patch_workshop();
