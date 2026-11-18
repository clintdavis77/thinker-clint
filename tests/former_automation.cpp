#include "../src/move.h"

static CTerraform terraform_data[MaxTerrainNum] = {};
CTerraform* Terraform = terraform_data;

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main() {
    for (int i = 0; i < MaxTerrainNum; ++i) {
        Terraform[i].bit = TerraformRules[i][0];
        Terraform[i].bit_incompatible = TerraformRules[i][1];
    }
    CHECK(!is_planting_order(ORDERA_AUTOMATE_AIR_DEFENSE));
    CHECK(!is_planting_order(17));
    CHECK(is_planting_order(ORDERA_PLANT_FOREST_HOME));
    CHECK(is_planting_order(ORDERA_PLANT_FUNGUS_GLOBAL));
    CHECK(planting_order_is_home(ORDERA_PLANT_FOREST_HOME));
    CHECK(planting_order_is_home(ORDERA_PLANT_FUNGUS_HOME));
    CHECK(!planting_order_is_home(ORDERA_PLANT_FOREST_GLOBAL));
    CHECK(planting_order_item(ORDERA_PLANT_FUNGUS_GLOBAL) == FORMER_PLANT_FUNGUS);

    // Exactly the 21 positions in the home base radius, including map-wrap distances.
    int radius_count = 0;
    for (int dx = -4; dx <= 4; ++dx) {
        for (int dy = -4; dy <= 4; ++dy) {
            if ((abs(dx) + abs(dy)) % 2 == 0 && planting_in_home_radius(dx, dy)) {
                ++radius_count;
            }
        }
    }
    CHECK(radius_count == 21);
    CHECK(!planting_in_home_radius(4, 0));
    CHECK(!planting_in_home_radius(0, -4));
    CHECK(planting_in_home_radius(3, 1));

    MAP sq = {};
    sq.climate = ALT_SHORE_LINE << 5;
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_FOREST);
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_GLOBAL, &sq, true, true) == FORMER_PLANT_FUNGUS);
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, false, true) == FORMER_NONE);
    CHECK(planting_tile_action(99, &sq, true, true) == FORMER_NONE);
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, nullptr, true, true) == FORMER_NONE);
    sq.items = BIT_FUNGUS | BIT_ROAD | BIT_SENSOR;
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_REMOVE_FUNGUS);
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, false) == FORMER_NONE);
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_HOME, &sq, true, true) == FORMER_NONE);
    sq.items = BIT_FOREST | BIT_ROAD | BIT_SENSOR;
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_GLOBAL, &sq, true, true) == FORMER_PLANT_FUNGUS);
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_GLOBAL, &sq, true, true) == FORMER_NONE);

    for (uint32_t improvement : {BIT_FARM, BIT_SOIL_ENRICHER, BIT_MINE, BIT_SOLAR}) {
        sq.items = improvement | BIT_FOREST;
        CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_HOME, &sq, true, true) == FORMER_NONE);
        sq.items = improvement | BIT_FUNGUS;
        CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_NONE);
    }
    // Use the live incompatibility masks rather than assuming default alpha rules.
    sq.items = BIT_FOREST | BIT_SENSOR;
    Terraform[FORMER_PLANT_FUNGUS].bit_incompatible |= BIT_SENSOR;
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_HOME, &sq, true, true) == FORMER_NONE);
    Terraform[FORMER_PLANT_FUNGUS].bit_incompatible &= ~BIT_SENSOR;
    sq.items = BIT_FUNGUS | BIT_ROAD;
    Terraform[FORMER_REMOVE_FUNGUS].bit_incompatible |= BIT_ROAD;
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_NONE);
    Terraform[FORMER_REMOVE_FUNGUS].bit_incompatible &= ~BIT_ROAD;
    sq.items = BIT_MONOLITH;
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_HOME, &sq, true, true) == FORMER_NONE);
    sq.items = BIT_BASE_IN_TILE;
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_NONE);
    sq.items = 0;
    sq.val3 = TILE_ROCKY;
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_NONE);
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_HOME, &sq, true, true) == FORMER_PLANT_FUNGUS);
    sq.val3 = 0;
    sq.climate = (ALT_OCEAN_SHELF - 1) << 5;
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_HOME, &sq, true, true) == FORMER_NONE);
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_NONE);
    sq.climate = ALT_OCEAN_SHELF << 5;
    CHECK(planting_tile_action(ORDERA_PLANT_FUNGUS_HOME, &sq, true, true) == FORMER_PLANT_FUNGUS);
    CHECK(planting_tile_action(ORDERA_PLANT_FOREST_HOME, &sq, true, true) == FORMER_NONE);
    return 0;
}
