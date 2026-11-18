// Exercise the actual former movement implementation with a small simulated map.
#include "../src/move.h"

static VEH test_vehicles[2] = {};
static UNIT test_units[1] = {};
static CChassis test_chassis[1] = {};
static BASE test_bases[1] = {};
static CTerraform test_terraform[MaxTerrainNum] = {};
static int vehicle_count = 2;
static int base_count = 1;
static int map_width = 26;
static int map_height = 12;
static int map_tile_count = 156;
static bool terra_available[MaxTerrainNum] = {};
static std::map<std::pair<int, int>, MAP> test_tiles;
static std::vector<std::pair<int, int>> search_tiles;
static char shortcut[] = "F";

VEH* Vehs = test_vehicles;
UNIT* Units = test_units;
CChassis* Chassis = test_chassis;
BASE* Bases = test_bases;
CTerraform* Terraform = test_terraform;
int* const VehCount = &vehicle_count;
int* const BaseCount = &base_count;
int* const MapAreaX = &map_width;
int* const MapAreaY = &map_height;
int* const MapAreaTiles = &map_tile_count;
Path* const Paths = nullptr;
PMTable mapdata;

bool map_is_known(int) { return false; }
bool is_ocean(MAP* sq) { return sq && sq->alt_level() < ALT_SHORE_LINE; }
bool has_pact(int, int) { return false; }
bool can_repair(int) { return true; }
bool defend_tile(VEH*, MAP*) { return false; }
bool non_ally_in_tile(int, int, int) { return false; }
int wrap(int x) { return x; }
int x_dist(int a, int b) { return abs(a - b); }
int map_range(int x1, int y1, int x2, int y2) {
    return (abs(x1 - x2) + abs(y1 - y2))/2;
}
MAP* mapsq(int x, int y) {
    auto it = test_tiles.find({x, y});
    return it == test_tiles.end() ? nullptr : &it->second;
}
int __cdecl terrain_avail(FormerItem item, int, int) { return terra_available[item]; }
int __cdecl mod_veh_skip(int) { return VEH_SKIP; }
int set_action(int id, int order, char) {
    Vehs[id].order = order;
    return VEH_SYNC;
}
int set_move_to(int id, int x, int y) {
    Vehs[id].order = ORDER_MOVE_TO;
    Vehs[id].waypoint_x[0] = x;
    Vehs[id].waypoint_y[0] = y;
    return VEH_SYNC;
}
void TileSearch::init(int, int, int) { current = 0; }
MAP* TileSearch::get_next() {
    if ((size_t)current >= search_tiles.size()) {
        return nullptr;
    }
    auto p = search_tiles[current++];
    rx = p.first;
    ry = p.second;
    return mapsq(rx, ry);
}
static int __thiscall find_path(Path*, int x, int y, int tx, int ty, int, int, int, int) {
    if (y != ty) {
        return -1;
    }
    return x < tx ? 1 : 5;
}
FPath_find Path_find = find_path;

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static MAP& tile(int x, int y, int owner = 1, uint32_t items = 0) {
    MAP& sq = test_tiles[{x, y}];
    sq = {};
    sq.climate = ALT_SHORE_LINE << 5;
    sq.region = 1;
    sq.owner = owner;
    sq.visibility = 1 << 1;
    sq.items = items;
    return sq;
}

static void reset_test() {
    memset(test_vehicles, 0, sizeof(test_vehicles));
    memset(test_units, 0, sizeof(test_units));
    memset(test_chassis, 0, sizeof(test_chassis));
    memset(test_bases, 0, sizeof(test_bases));
    test_tiles.clear();
    search_tiles.clear();
    mapdata.clear();
    for (int i = 0; i < MaxTerrainNum; ++i) {
        test_terraform[i].bit = TerraformRules[i][0];
        test_terraform[i].bit_incompatible = TerraformRules[i][1];
        test_terraform[i].shortcuts = shortcut;
        terra_available[i] = true;
    }
    test_units[0].plan = PLAN_TERRAFORM;
    test_units[0].reactor_id = 1;
    test_chassis[0].triad = TRIAD_LAND;
    test_bases[0].x = 10;
    test_bases[0].y = 10;
    test_bases[0].faction_id = 1;
    test_vehicles[0].faction_id = 1;
    test_vehicles[0].home_base_id = 0;
    test_vehicles[0].x = 12;
    test_vehicles[0].y = 10;
    test_vehicles[0].state = VSTATE_ON_ALERT;
    test_vehicles[0].order_auto_type = ORDERA_PLANT_FOREST_HOME;
    test_vehicles[1].faction_id = 1;
    test_vehicles[1].x = 0;
    test_vehicles[1].y = 0;
    tile(12, 10);
}

int main() {
    reset_test();
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SYNC);
    CHECK(Vehs[0].order == ORDER_PLANT_FOREST);

    reset_test();
    Vehs[0].order_auto_type = ORDERA_PLANT_FUNGUS_HOME;
    mapsq(12, 10)->items = BIT_FOREST | BIT_ROAD;
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SYNC);
    CHECK(Vehs[0].order == ORDER_PLANT_FUNGUS);

    reset_test();
    Vehs[0].x = 20;
    Vehs[0].order_auto_type = ORDERA_PLANT_FOREST_GLOBAL;
    tile(20, 10, -1);
    CHECK(planting_former_move(0, mapsq(20, 10)) == VEH_SYNC);
    CHECK(Vehs[0].order == ORDER_PLANT_FOREST);

    reset_test();
    mapsq(12, 10)->items = BIT_FUNGUS | BIT_ROAD;
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SYNC);
    CHECK(Vehs[0].order == ORDER_REMOVE_FUNGUS);
    Vehs[0].movement_turns = 3;
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SYNC);
    CHECK(Vehs[0].movement_turns == 3);
    mapsq(12, 10)->items |= BIT_FARM;
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SKIP);
    CHECK(Vehs[0].order == ORDER_NONE && Vehs[0].movement_turns == 0);
    CHECK(Vehs[0].state & VSTATE_ON_ALERT);

    reset_test();
    terra_available[FORMER_FOREST] = false;
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SKIP);
    CHECK(Vehs[0].order_auto_type == ORDERA_PLANT_FOREST_HOME);

    reset_test();
    Vehs[0].x = 18;
    tile(18, 10, 1, BIT_FARM);
    tile(16, 10);
    tile(14, 10); // Distance two, but outside the 21-tile base radius.
    search_tiles = {{14, 10}, {12, 10}};
    CHECK(planting_former_move(0, mapsq(18, 10)) == VEH_SYNC);
    CHECK(Vehs[0].order == ORDER_MOVE_TO && Vehs[0].waypoint_x[0] == 12);

    reset_test();
    Vehs[0].x = 18;
    Vehs[0].order_auto_type = ORDERA_PLANT_FUNGUS_GLOBAL;
    tile(18, 10, 1, BIT_FARM);
    tile(20, 10, -1, BIT_FOREST);
    mapsq(12, 10)->items = BIT_FARM;
    search_tiles = {{20, 10}};
    CHECK(planting_former_move(0, mapsq(18, 10)) == VEH_SYNC);
    CHECK(Vehs[0].order == ORDER_MOVE_TO && Vehs[0].waypoint_x[0] == 20);
    mapsq(20, 10)->owner = 2; // Foreign destinations are never used.
    CHECK(planting_former_move(0, mapsq(18, 10)) == VEH_SKIP);
    CHECK(Vehs[0].order == ORDER_NONE && (Vehs[0].state & VSTATE_ON_ALERT));

    reset_test();
    Vehs[0].x = 18;
    Vehs[0].order_auto_type = ORDERA_PLANT_FOREST_GLOBAL;
    tile(18, 10, 1, BIT_FARM);
    mapsq(12, 10)->items = BIT_FARM;
    tile(20, 10, 2); // The actual route crosses foreign territory.
    tile(22, 10, -1);
    search_tiles = {{22, 10}};
    CHECK(planting_former_move(0, mapsq(18, 10)) == VEH_SKIP);
    CHECK(Vehs[0].order == ORDER_NONE);
    mapsq(20, 10)->owner = -1;
    mapdata[{20, 10}].safety = PM_SAFE - 1;
    CHECK(planting_former_move(0, mapsq(18, 10)) == VEH_SKIP);

    // The fixed-size TileSearch can miss distant tiles; the fallback finds them.
    reset_test();
    Vehs[0].x = 18;
    Vehs[0].order_auto_type = ORDERA_PLANT_FOREST_GLOBAL;
    mapsq(12, 10)->items = BIT_FARM;
    tile(18, 10, 1, BIT_FARM);
    tile(20, 10, -1);
    CHECK(planting_former_move(0, mapsq(18, 10)) == VEH_SYNC);
    CHECK(Vehs[0].order == ORDER_MOVE_TO && Vehs[0].waypoint_x[0] == 20);

    reset_test();
    Vehs[1].x = 12;
    Vehs[1].y = 10;
    Vehs[1].order = ORDER_PLANT_FOREST;
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SKIP);
    CHECK(Vehs[0].order == ORDER_NONE);

    reset_test();
    Bases[0].faction_id = 2;
    CHECK(planting_former_move(0, mapsq(12, 10)) == VEH_SYNC);
    CHECK(!(Vehs[0].state & VSTATE_ON_ALERT));
    CHECK(Vehs[0].order_auto_type == ORDERA_TERRA_AUTO_FULL);
    return 0;
}
