#include "../src/workshop.h"
#include "../src/veh.h"
#include "../src/tech.h"

static CRules rules_data = {};
static CAbility ability_data[MaxAbilityNum] = {};
static CChassis chassis_data[MaxChassisNum] = {};
static CWeapon weapon_data[MaxWeaponNum] = {};
static bool known[MaxPlayerNum][MaxTechnologyNum] = {};
static int forbidden = -1;

Config conf;
CRules* Rules = &rules_data;
CAbility* Ability = ability_data;
CChassis* Chassis = chassis_data;
CWeapon* Weapon = weapon_data;

int __cdecl has_tech(int tech_id, int faction_id) {
    return tech_id == TECH_None || (tech_id >= 0 && tech_id < MaxTechnologyNum
        && known[faction_id][tech_id]);
}

static int __cdecl allowed(int id, int, int, int) {
    return id != forbidden;
}
Fabil_cond abil_cond = allowed;

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main() {
    Rules->tech_preq_allow_2_spec_abil = TECH_Neural;
    CHECK(ability_slot_limit(1) == 1);
    known[1][TECH_MindMac] = true; // Cannot bypass the normal two-ability prerequisite.
    CHECK(ability_slot_limit(1) == 1);
    known[1][TECH_MindMac] = false;
    known[1][TECH_Neural] = true;
    CHECK(ability_slot_limit(1) == 2);
    known[1][TECH_MindMac] = true;
    CHECK(ability_slot_limit(1) == 3);
    CHECK(ability_slot_limit(2) == 1);
    conf.third_ability_tech = -1;
    CHECK(ability_slot_limit(1) == 2);
    conf.third_ability_tech = TECH_DigSent;
    CHECK(ability_slot_limit(1) == 2);
    known[1][TECH_DigSent] = true;
    CHECK(ability_slot_limit(1) == 3);
    conf.third_ability_tech = TECH_MindMac;

    for (CAbility& ability : ability_data) {
        ability.preq_tech = TECH_None;
        ability.flags = AFLAG_ALLOWED_LAND_UNIT | AFLAG_ALLOWED_SEA_UNIT
            | AFLAG_ALLOWED_COMBAT_UNIT | AFLAG_ALLOWED_NONCOMBAT_UNIT;
    }
    Weapon[WPN_HAND_WEAPONS].offense_value = 1;
    static DesignWindow draft = {};
    draft.faction_id = 1;
    draft.unit_chassis_type = CHS_INFANTRY;
    draft.unit_weapon_type = WPN_HAND_WEAPONS;
    draft.unit_armor_type = ARM_NO_ARMOR;
    draft.unit_ability_id1 = -1;
    draft.unit_ability_id2 = -1;
    strcpy(draft.custom_name, "Do not overwrite this name");

    workshop_select_ability(&draft, 0, ABL_ID_AAA);
    workshop_select_ability(&draft, 1, ABL_ID_COMM_JAMMER);
    workshop_select_ability(&draft, 2, ABL_ID_TRANCE);
    CHECK(draft.unit_ability_flags == (ABL_AAA | ABL_COMM_JAMMER | ABL_TRANCE));
    CHECK(workshop_ability(&draft, 0) == ABL_ID_AAA);
    CHECK(workshop_ability(&draft, 1) == ABL_ID_COMM_JAMMER);
    CHECK(workshop_ability(&draft, 2) == ABL_ID_TRANCE);
    CHECK(!strcmp(draft.custom_name, "Do not overwrite this name"));
    CHECK(workshop_exclude_ability(&draft, 0, ABL_ID_TRANCE));
    CHECK(!workshop_exclude_ability(&draft, 2, ABL_ID_TRANCE));
    workshop_select_ability(&draft, 0, ABL_ID_TRANCE); // Duplicate: unchanged.
    CHECK(workshop_ability(&draft, 0) == ABL_ID_AAA);
    workshop_select_ability(&draft, 0, ABL_ID_TRAINED);
    CHECK(workshop_ability(&draft, 2) == ABL_ID_TRANCE);
    workshop_select_ability(&draft, 2, ABL_ID_POLICE_2X);
    CHECK(workshop_ability(&draft, 2) == ABL_ID_POLICE_2X);
    CHECK(!(draft.unit_ability_flags & ABL_TRANCE));
    workshop_select_ability(&draft, 2, -1); // "None" removes just slot three.
    CHECK(draft.unit_ability_flags == (ABL_TRAINED | ABL_COMM_JAMMER));
    workshop_select_ability(&draft, 2, ABL_ID_TRANCE);

    // A missing first/second selection must not erase the third.
    workshop_select_ability(&draft, 0, -1);
    CHECK(workshop_ability(&draft, 0) == -1);
    CHECK(workshop_ability(&draft, 2) == ABL_ID_TRANCE);
    workshop_select_ability(&draft, 0, ABL_ID_AAA);

    // Reloading/switching designs: the native engine reconstructs the first two IDs.
    draft.unit_ability_id1 = ABL_ID_AAA;
    draft.unit_ability_id2 = ABL_ID_COMM_JAMMER;
    workshop_check_abilities(&draft);
    CHECK(workshop_ability(&draft, 2) == ABL_ID_TRANCE);

    // Normal tech and component eligibility still apply.
    Ability[ABL_ID_POLICE_2X].preq_tech = TECH_DocLoy;
    workshop_select_ability(&draft, 2, ABL_ID_POLICE_2X);
    CHECK(workshop_ability(&draft, 2) == ABL_ID_TRANCE);
    forbidden = ABL_ID_TRANCE;
    workshop_check_abilities(&draft);
    CHECK(workshop_ability(&draft, 2) == -1);
    forbidden = -1;
    Ability[ABL_ID_TRANCE].flags &= ~AFLAG_ALLOWED_LAND_UNIT;
    workshop_select_ability(&draft, 2, ABL_ID_TRANCE);
    CHECK(workshop_ability(&draft, 2) == -1);
    Ability[ABL_ID_TRANCE].flags |= AFLAG_ALLOWED_LAND_UNIT;
    workshop_select_ability(&draft, 2, MaxAbilityNum);
    CHECK(workshop_ability(&draft, 2) == -1);
    CHECK(workshop_ability(&draft, -1) == -1);
    CHECK(workshop_ability(&draft, 3) == -1);
    workshop_select_ability(&draft, 2, ABL_ID_ALGO_ENHANCEMENT); // Highest valid bit.
    CHECK(workshop_ability(&draft, 2) == ABL_ID_ALGO_ENHANCEMENT);
    workshop_select_ability(&draft, 2, -1);

    // Reducing the cap clears hidden selections without changing the saved prototype.
    workshop_select_ability(&draft, 2, ABL_ID_TRANCE);
    known[1][TECH_MindMac] = false;
    workshop_check_abilities(&draft);
    CHECK(bit_count(draft.unit_ability_flags) == 2);
    CHECK(workshop_ability(&draft, 2) == -1);
    int two_flags = draft.unit_ability_flags;
    workshop_select_ability(&draft, 2, ABL_ID_TRAINED);
    CHECK(draft.unit_ability_flags == two_flags);
    known[1][TECH_Neural] = false;
    workshop_check_abilities(&draft);
    CHECK(bit_count(draft.unit_ability_flags) == 1);
    CHECK(workshop_ability(&draft, 0) >= 0);
    CHECK(workshop_ability(&draft, 1) == -1);

    CHECK(ability_conflict(ABL_CARRIER, ABL_ID_DEEP_PRESSURE_HULL));
    CHECK(ability_conflict(ABL_DEEP_PRESSURE_HULL, ABL_ID_CARRIER));
    CHECK(!ability_conflict(ABL_AAA, ABL_ID_TRANCE));
    known[1][TECH_Neural] = known[1][TECH_MindMac] = true;
    draft.unit_ability_flags = ABL_CARRIER | ABL_DEEP_PRESSURE_HULL | ABL_AAA;
    workshop_check_abilities(&draft);
    CHECK(!(draft.unit_ability_flags & ABL_CARRIER));
    Chassis[CHS_INFANTRY].missile = 1;
    workshop_check_abilities(&draft);
    CHECK(!draft.unit_ability_flags);
    CHECK(draft.unit_ability_id1 == -1 && draft.unit_ability_id2 == -1);
    CHECK(!strcmp(draft.custom_name, "Do not overwrite this name"));
    puts("Workshop ability tests passed.");
    return 0;
}
