
#include "veh.h"
#include "workshop.h"

int ability_slot_limit(int faction_id) {
    if (!has_tech(Rules->tech_preq_allow_2_spec_abil, faction_id)) {
        return 1;
    }
    return conf.third_ability_tech >= 0
        && has_tech(conf.third_ability_tech, faction_id) ? 3 : 2;
}

bool ability_conflict(uint32_t abls, VehAbl abl) {
    return (abls & (1u << abl))
        || (abl == ABL_ID_CARRIER && (abls & ABL_DEEP_PRESSURE_HULL))
        || (abl == ABL_ID_DEEP_PRESSURE_HULL && (abls & ABL_CARRIER));
}

bool has_ability(int faction_id, VehAbl abl, VehChassis chs, VehWeapon wpn) {
    int F = Ability[abl].flags;
    int triad = Chassis[chs].triad;
    bool is_combat = Weapon[wpn].offense_value != 0;
    bool is_former = Weapon[wpn].mode == WMODE_TERRAFORM;
    bool is_probe = Weapon[wpn].mode == WMODE_PROBE;

    if ((triad == TRIAD_LAND && !(F & AFLAG_ALLOWED_LAND_UNIT))
    || (triad == TRIAD_SEA && !(F & AFLAG_ALLOWED_SEA_UNIT))
    || (triad == TRIAD_AIR && !(F & AFLAG_ALLOWED_AIR_UNIT))) {
        return false;
    }
    if ((!(F & AFLAG_ALLOWED_COMBAT_UNIT) && is_combat)
    || (!(F & AFLAG_ALLOWED_TERRAFORM_UNIT) && is_former)
    || (!(F & AFLAG_ALLOWED_NONCOMBAT_UNIT) && !is_combat && !is_former)) {
        return false;
    }
    if ((F & AFLAG_NOT_ALLOWED_PROBE_TEAM && is_probe)
    || (F & AFLAG_ONLY_PROBE_TEAM && !is_probe)) {
        return false;
    }
    if (F & AFLAG_TRANSPORT_ONLY_UNIT && Weapon[wpn].mode != WMODE_TRANSPORT) {
        return false;
    }
    if (F & AFLAG_NOT_ALLOWED_FAST_UNIT && Chassis[chs].speed > 1) {
        return false;
    }
    if (F & AFLAG_NOT_ALLOWED_PSI_UNIT && Weapon[wpn].offense_value < 0) {
        return false;
    }
    return has_tech(Ability[abl].preq_tech, faction_id);
}

static uint32_t ability_bit(int id) {
    return id >= 0 && id < MaxAbilityNum ? 1u << id : 0;
}

/*
The original window has two selection fields immediately followed by the custom
name. Never index a third field there: derive slot three from the remaining bit.
UNIT::ability_flags already stores the complete mask, including in savegames.
*/
int workshop_ability(DesignWindow* This, int slot) {
    if (slot < 0 || slot > 2) {
        return -1;
    }
    int id = slot == 0 ? This->unit_ability_id1 : This->unit_ability_id2;
    uint32_t flags = This->unit_ability_flags;
    if (slot < 2) {
        return flags & ability_bit(id) ? id : -1;
    }
    flags &= ~(ability_bit(This->unit_ability_id1) | ability_bit(This->unit_ability_id2));
    for (id = 0; id < MaxAbilityNum; id++) {
        if (flags & ability_bit(id)) {
            return id;
        }
    }
    return -1;
}

int __thiscall workshop_check_abilities(DesignWindow* This) {
    uint32_t flags = This->unit_ability_flags & ((1u << MaxAbilityNum) - 1);
    int limit = ability_slot_limit(This->faction_id);
    if (Chassis[This->unit_chassis_type].missile) {
        flags = 0;
    }
    for (int id = 0; id < MaxAbilityNum; id++) {
        if ((flags & ability_bit(id))
        && (!abil_cond(id, This->unit_chassis_type, This->unit_weapon_type, This->unit_armor_type)
        || (id == ABL_ID_CARRIER && (flags & ABL_DEEP_PRESSURE_HULL)))) {
            flags &= ~ability_bit(id);
        }
    }
    // Match the original priority: discard lower-numbered abilities first.
    for (int id = 0; bit_count(flags) > limit && id < MaxAbilityNum; id++) {
        flags &= ~ability_bit(id);
    }
    This->unit_ability_flags = flags;
    if (!(flags & ability_bit(This->unit_ability_id1))) {
        This->unit_ability_id1 = -1;
    }
    if (!(flags & ability_bit(This->unit_ability_id2))
    || This->unit_ability_id2 == This->unit_ability_id1) {
        This->unit_ability_id2 = -1;
    }
    if (limit == 1 && This->unit_ability_id2 >= 0) {
        This->unit_ability_id1 = This->unit_ability_id2;
        This->unit_ability_id2 = -1;
    }
    // With fewer than three slots no selections may remain invisible in the mask.
    if (limit < 3) {
        uint32_t remaining = flags
            & ~(ability_bit(This->unit_ability_id1) | ability_bit(This->unit_ability_id2));
        for (int id = 0; id < MaxAbilityNum; id++) {
            if (remaining & ability_bit(id)) {
                if (This->unit_ability_id1 < 0) {
                    This->unit_ability_id1 = id;
                } else {
                    This->unit_ability_id2 = id;
                }
            }
        }
    }
    return 0;
}

int __cdecl workshop_exclude_ability(DesignWindow* This, int slot, int ability_id) {
    uint32_t other = This->unit_ability_flags & ~ability_bit(workshop_ability(This, slot));
    return ability_id < 0 || ability_id >= MaxAbilityNum
        || ability_conflict(other, (VehAbl)ability_id);
}

void __cdecl workshop_select_ability(DesignWindow* This, int slot, int ability_id) {
    if (slot < 0 || slot >= ability_slot_limit(This->faction_id)
    || (ability_id >= 0 && (workshop_exclude_ability(This, slot, ability_id)
    || !has_ability(This->faction_id, (VehAbl)ability_id,
        (VehChassis)This->unit_chassis_type, (VehWeapon)This->unit_weapon_type)))) {
        return;
    }
    This->unit_ability_flags &= ~ability_bit(workshop_ability(This, slot));
    This->unit_ability_flags |= ability_bit(ability_id);
    if (!slot) {
        This->unit_ability_id1 = ability_id;
    } else if (slot == 1) {
        This->unit_ability_id2 = ability_id;
    }
    workshop_check_abilities(This);
}
