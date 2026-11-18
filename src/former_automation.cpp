#include "main.h"

bool is_planting_order(int order) {
    return order >= ORDERA_PLANT_FOREST_HOME && order <= ORDERA_PLANT_FUNGUS_GLOBAL;
}

bool planting_order_is_home(int order) {
    return order == ORDERA_PLANT_FOREST_HOME || order == ORDERA_PLANT_FUNGUS_HOME;
}

FormerItem planting_order_item(int order) {
    return (order == ORDERA_PLANT_FOREST_HOME || order == ORDERA_PLANT_FOREST_GLOBAL)
        ? FORMER_FOREST : FORMER_PLANT_FUNGUS;
}

const char* planting_order_name(int order) {
    switch (order) {
    case ORDERA_PLANT_FOREST_HOME: return "Forest near home";
    case ORDERA_PLANT_FUNGUS_HOME: return "Fungus near home";
    case ORDERA_PLANT_FOREST_GLOBAL: return "Forest globally";
    case ORDERA_PLANT_FUNGUS_GLOBAL: return "Fungus globally";
    default: return "";
    }
}

bool planting_in_home_radius(int dx, int dy) {
    dx = abs(dx);
    dy = abs(dy);
    // The 21-tile base radius excludes the four cardinal tiles at distance two.
    return dx + dy <= 4 && dx < 4 && dy < 4;
}

int planting_tile_action(int order, MAP* sq, bool can_plant, bool can_remove_fungus) {
    if (!is_planting_order(order) || !sq || !can_plant || sq->is_pole_tile()
    || sq->items & (BIT_BASE_IN_TILE | BIT_MONOLITH) || sq->volcano_center()) {
        return FORMER_NONE;
    }
    FormerItem item = planting_order_item(order);
    uint32_t opposite = item == FORMER_FOREST ? BIT_FUNGUS : BIT_FOREST;
    if (sq->items & Terraform[item].bit) {
        return FORMER_NONE;
    }
    // Do not remove any improvement other than the opposite terrain type.
    if (sq->items & (Terraform[item].bit_incompatible & ~opposite)) {
        return FORMER_NONE;
    }
    if (item == FORMER_FOREST) {
        if (sq->alt_level() < ALT_SHORE_LINE || sq->is_rocky()
        || sq->landmarks & LM_VOLCANO) {
            return FORMER_NONE;
        }
        if (sq->items & BIT_FUNGUS) {
            if (!can_remove_fungus
            || sq->items & (Terraform[FORMER_REMOVE_FUNGUS].bit_incompatible & ~BIT_FUNGUS)) {
                return FORMER_NONE;
            }
            return FORMER_REMOVE_FUNGUS;
        }
    } else if (sq->alt_level() < ALT_OCEAN_SHELF) {
        return FORMER_NONE;
    }
    return item;
}
