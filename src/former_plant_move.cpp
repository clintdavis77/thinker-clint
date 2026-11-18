#include "main.h"

static int planting_candidate(VEH* veh, int x, int y, MAP* sq) {
    int faction_id = veh->faction_id;
    if (!sq || (sq->is_owned() && sq->owner != faction_id) || !sq->is_known(faction_id)
    || (veh->triad() != TRIAD_AIR && is_ocean(sq) != (veh->triad() == TRIAD_SEA))
    || mapdata[{x, y}].safety < PM_SAFE || non_ally_in_tile(x, y, faction_id)) {
        return FORMER_NONE;
    }
    if (planting_order_is_home(veh->order_auto_type)) {
        int base_id = veh->home_base_id;
        if (base_id < 0 || base_id >= *BaseCount || Bases[base_id].faction_id != faction_id
        || !planting_in_home_radius(x_dist(x, Bases[base_id].x), y - Bases[base_id].y)) {
            return FORMER_NONE;
        }
    }
    bool sea = is_ocean(sq);
    return planting_tile_action(veh->order_auto_type, sq,
        has_terra(planting_order_item(veh->order_auto_type), sea, faction_id),
        has_terra(FORMER_REMOVE_FUNGUS, sea, faction_id));
}

static bool planting_reserved(int veh_id, int x, int y) {
    VEH* veh = &Vehs[veh_id];
    for (int i = 0; i < *VehCount; i++) {
        VEH* other = &Vehs[i];
        if (i == veh_id || other->faction_id != veh->faction_id || !other->is_former()) {
            continue;
        }
        if ((other->x == x && other->y == y
        && other->order >= VehOrderFormerFirst && other->order <= VehOrderFormerLast)
        || ((other->state & VSTATE_ON_ALERT) && other->order == ORDER_MOVE_TO
        && other->waypoint_x[0] == x && other->waypoint_y[0] == y)) {
            return true;
        }
    }
    return false;
}

static bool planting_path_safe(VEH* veh, int tx, int ty) {
    int x = veh->x;
    int y = veh->y;
    int faction_id = veh->faction_id;
    Points visited;
    for (int i = 0; i < *MapAreaTiles; i++) {
        if (x == tx && y == ty) {
            return true;
        }
        if (!visited.insert({x, y}).second) {
            return false;
        }
        int direction = Path_find(Paths, x, y, tx, ty, veh->unit_id, faction_id, 0, -1);
        if (direction < 0 || direction >= 8) {
            return false;
        }
        x = wrap(x + BaseOffsetX[direction]);
        y += BaseOffsetY[direction];
        MAP* sq = mapsq(x, y);
        if (!sq || !sq->is_known(faction_id)
        || (sq->is_owned() && sq->owner != faction_id && !has_pact(faction_id, sq->owner))
        || mapdata[{x, y}].safety < PM_SAFE || non_ally_in_tile(x, y, faction_id)) {
            return false;
        }
    }
    return x == tx && y == ty;
}

int planting_former_move(int id, MAP* sq) {
    VEH* veh = &Vehs[id];
    int order = veh->order_auto_type;
    int base_id = veh->home_base_id;
    if (planting_order_is_home(order)
    && (base_id < 0 || base_id >= *BaseCount || Bases[base_id].faction_id != veh->faction_id)) {
        // Do not silently turn a home-base order into a global order.
        veh->state &= ~VSTATE_ON_ALERT;
        veh->order = ORDER_NONE;
        veh->order_auto_type = ORDERA_TERRA_AUTO_FULL;
        veh->movement_turns = 0;
        return VEH_SYNC;
    }
    if (veh->in_transit() || (veh->triad() == TRIAD_LAND && is_ocean(sq))
    || (veh->need_heals() && defend_tile(veh, sq))
    || mapdata[{veh->x, veh->y}].safety < PM_SAFE) {
        return mod_veh_skip(id);
    }

    // Recheck destination and work every turn, including after a save/reload:
    // another former or a change of ownership must not cause unwanted replacement.
    if (veh->order == ORDER_MOVE_TO && !veh->at_target()) {
        int tx = veh->waypoint_x[0];
        int ty = veh->waypoint_y[0];
        if (planting_candidate(veh, tx, ty, mapsq(tx, ty)) >= 0
        && !planting_reserved(id, tx, ty) && planting_path_safe(veh, tx, ty)) {
            return VEH_SYNC;
        }
        veh->order = ORDER_NONE;
        veh->movement_turns = 0;
    }
    if (veh->order == ORDER_MOVE_TO && veh->at_target()) {
        veh->order = ORDER_NONE;
        veh->waypoint_x[0] = -1;
        veh->waypoint_y[0] = -1;
    }
    int item = planting_candidate(veh, veh->x, veh->y, sq);
    if (veh->order >= VehOrderFormerFirst && veh->order <= VehOrderFormerLast) {
        if (item == veh->order - VehOrderFormerFirst) {
            return VEH_SYNC;
        }
        veh->order = ORDER_NONE;
        veh->movement_turns = 0;
    }
    if (item >= 0 && !planting_reserved(id, veh->x, veh->y)) {
        debug("former_plant_action %d %d %s\n", veh->x, veh->y, Terraform[item].name);
        return set_action(id, item + VehOrderFormerFirst, *Terraform[item].shortcuts);
    }

    TileSearch ts;
    ts.init(veh->x, veh->y, veh->triad());
    while ((sq = ts.get_next()) != NULL) {
        if (planting_candidate(veh, ts.rx, ts.ry, sq) < 0
        || planting_reserved(id, ts.rx, ts.ry)
        || !planting_path_safe(veh, ts.rx, ts.ry)) {
            continue;
        }
        debug("former_plant_move %d %d -> %d %d\n", veh->x, veh->y, ts.rx, ts.ry);
        return set_move_to(id, ts.rx, ts.ry);
    }
    // TileSearch has a fixed queue. On very large maps it can fill before
    // reaching distant eligible tiles, so inspect the remaining map as well.
    int best_x = -1;
    int best_y = -1;
    int best_dist = INT_MAX;
    auto consider = [&](int x, int y) {
        MAP* candidate = mapsq(x, y);
        int dist = map_range(veh->x, veh->y, x, y);
        if (dist < best_dist && !ts.oldtiles.count({x, y})
        && planting_candidate(veh, x, y, candidate) >= 0
        && !planting_reserved(id, x, y) && planting_path_safe(veh, x, y)) {
            best_x = x;
            best_y = y;
            best_dist = dist;
        }
    };
    if (planting_order_is_home(order)) {
        for (int i = 1; i < 21; i++) {
            consider(wrap(Bases[base_id].x + TableOffsetX[i]), Bases[base_id].y + TableOffsetY[i]);
        }
    } else {
        for (int y = 0; y < *MapAreaY; y++) {
            for (int x = y & 1; x < *MapAreaX; x += 2) {
                consider(x, y);
            }
        }
    }
    if (best_x >= 0) {
        return set_move_to(id, best_x, best_y);
    }
    // Keep the order, so new territory or a later technology can provide work.
    return mod_veh_skip(id);
}
