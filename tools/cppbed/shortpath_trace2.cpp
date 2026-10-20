// Trace bed for the short-path search.
//
// It compiles a traced copy of openbw (openbw_traced.h) so that the search
// loop prints every expansion and every neighbour, and the Zan port can be
// compared step by step.
//
//   g++ -std=c++17 -I. -O0 -g shortpath_trace2.cpp -o shortpath_trace2.exe

#include "openbw_traced.h"

#include <cstdio>
#include <cstdlib>
#include <list>
#include <vector>

using namespace bwgame;

static const int TILE_W = 64;
static const int TILE_H = 64;
static int g_variant = 0;

static bool tile_is_wall(int tx, int ty) {
	if (g_variant == 0) {
		if (tx >= 28 && tx < 36 && ty >= 28 && ty < 36) return true;
		if (tx >= 48 && tx < 50 && ty >= 8 && ty < 40) return true;
		if (ty >= 48 && ty < 50 && tx >= 8 && tx < 40) return true;
		if ((tx + ty) % 17 == 0 && tx >= 8 && tx < 56 && ty >= 8 && ty < 56) return true;
		return false;
	}
	if (g_variant == 1) {
		bool armX = ty >= 12 && ty < 16;
		bool armY = tx >= 12 && tx < 16;
		if (armX && (tx < 20 || tx > 44)) return true;
		if (armY && (ty < 20 || ty > 44)) return true;
		if (!armX && !armY && tx >= 20 && tx < 44 && ty >= 20 && ty < 44) {
			return !(tx >= 26 && tx < 38 && ty >= 26 && ty < 38);
		}
		return false;
	}
	if (g_variant == 2) return (tx % 8 == 0) != (ty % 8 == 0);
	if (g_variant == 3) {
		if (ty >= 20 && ty < 44) {
			if (tx >= 8 && tx < 30) return true;
			if (tx >= 34 && tx < 56) return true;
		}
		return false;
	}
	if (g_variant == 4) return false;
	if (tx >= 8 && tx < 56 && ty >= 8 && ty < 56) {
		int ring = (tx < 16 || tx >= 48 || ty < 16 || ty >= 48) ? 0
				 : ((tx < 24 || tx >= 40 || ty < 24 || ty >= 40) ? 1 : 2);
		if (ring == 0) {
			bool door = (ty == 16 && tx >= 24 && tx < 32) || (ty == 48 && tx >= 24 && tx < 32)
						|| (tx == 16 && ty >= 24 && ty < 32) || (tx == 48 && ty >= 24 && ty < 32);
			return !door;
		}
		if (ring == 1) {
			bool door = (ty == 24 && tx >= 28 && tx < 36) || (ty == 40 && tx >= 28 && tx < 36)
						|| (tx == 24 && ty >= 28 && ty < 36) || (tx == 40 && ty >= 28 && ty < 36);
			return !door;
		}
		return false;
	}
	if (tx >= 24 && tx < 40 && ty >= 24 && ty < 40) return true;
	return false;
}

static iscript_t bed_iscript = [] {
	iscript_t t;
	t.program_data.push_back((int)iscript_opcodes::opc_end + 0x808091);
	t.program_data.push_back((int)iscript_opcodes::opc_wait + 0x808091);
	t.program_data.push_back(1);
	t.program_data.push_back((int)iscript_opcodes::opc_goto + 0x808091);
	t.program_data.push_back(1);
	iscript_t::script s;
	s.id = 0;
	s.animation_pc = a_vector<size_t>(64, 1);
	t.scripts[0] = s;
	return t;
}();

static grp_t bed_grp = [] {
	grp_t g{};
	g.width = 42;
	g.height = 42;
	return g;
}();

static image_type_t bed_image_type = [] {
	image_type_t t{};
	t.id = (ImageTypes)0;
	t.grp_filename_index = 0;
	t.has_directional_frames = true;
	t.is_clickable = false;
	t.has_iscript_animations = true;
	t.always_visible = false;
	t.modifier = 0;
	t.iscript_id = 0;
	return t;
}();

static sprite_type_t bed_sprite_type = [] {
	sprite_type_t t{};
	t.id = (SpriteTypes)0;
	t.image = type_id<image_type_t>(&bed_image_type);
	t.health_bar_size = 0;
	t.visible = true;
	t.selection_circle = 0;
	t.selection_circle_vpos = 0;
	return t;
}();

static flingy_type_t bed_flingy_type = [] {
	flingy_type_t t{};
	t.id = (FlingyTypes)0;
	t.sprite = type_id<sprite_type_t>(&bed_sprite_type);
	t.top_speed = fp8::integer(256);
	t.acceleration = fp8::integer(3);
	t.halt_distance = fp8::integer(512);
	t.turn_rate = fp8::integer(8);
	t.movement_type = 0;
	return t;
}();

static unit_type_t bed_unit_type = [] {
	unit_type_t t{};
	t.id = (UnitTypes)0;
	t.flingy = type_id<const flingy_type_t>(&bed_flingy_type);
	t.hitpoints = fp8::integer(400);
	t.elevation_level = 0;
	t.flags = (unit_type_t::flags_t)(unit_type_t::flag_can_move | unit_type_t::flag_can_turn);
	t.sight_range = 7;
	t.dimensions.from = xy(-8, -8);
	t.dimensions.to = xy(8, 8);
	t.supply_provided = fp1::zero();
	t.supply_required = fp1::zero();
	t.group_flags = GroupFlags::Independant;
	return t;
}();

struct bed_t {
	global_state global_st;
	game_state game_st;
	state st;
	unit_t* unit = nullptr;

	bed_t() {
		global_st.iscript = bed_iscript;
		global_st.image_grp.push_back(&bed_grp);
		global_st.flingy_types.vec.push_back(bed_flingy_type);
		game_st.unit_types.vec.push_back(bed_unit_type);
		global_st.order_types.vec.resize(189);
		global_st.order_types.vec[(size_t)Orders::Nothing].id = Orders::Nothing;
		global_st.order_types.vec[(size_t)Orders::Fatal].id = Orders::Fatal;
		global_st.image_types.vec.push_back(bed_image_type);
		global_st.sprite_types.vec.push_back(bed_sprite_type);

		game_st.map_tile_width = TILE_W;
		game_st.map_tile_height = TILE_H;
		game_st.map_walk_width = TILE_W * 4;
		game_st.map_walk_height = TILE_H * 4;
		game_st.map_width = TILE_W * 32;
		game_st.map_height = TILE_H * 32;
		game_st.tileset_index = 0;

		st.global = &global_st;
		st.game = &game_st;
		st.current_frame = 0;
		st.lcg_rand_state = 42;

		game_st.repulse_field_width = (game_st.map_width + 47) / 48;
		game_st.repulse_field_height = (game_st.map_height + 47) / 48;
		st.repulse_field.resize(game_st.repulse_field_width * game_st.repulse_field_height);

		st.tiles.resize(game_st.map_tile_width * game_st.map_tile_height);
		st.tiles_mega_tile_index.resize(game_st.map_tile_width * game_st.map_tile_height);
		st.sprites_on_tile_line.resize(game_st.map_tile_height);

		for (int i = 0; i < 12; ++i) {
			st.players[i].controller = player_t::controller_computer;
		}

		game_st.vf4.resize(3);
		for (int i = 0; i < 16; ++i) {
			game_st.vf4[0].flags[i] = vf4_entry::flag_walkable;
			game_st.vf4[1].flags[i] = 0;
			game_st.vf4[2].flags[i] = (i / 4 >= 2) ? vf4_entry::flag_walkable : 0;
		}

		for (int y = 0; y < TILE_H; ++y) {
			for (int x = 0; x < TILE_W; ++x) {
				auto& t = st.tiles[y * TILE_W + x];
				t.flags = tile_is_wall(x, y) ? tile_t::flag_middle : tile_t::flag_walkable;
				st.tiles_mega_tile_index[y * TILE_W + x] = tile_is_wall(x, y) ? 1 : 0;
			}
		}
	}
};

int main(int argc, char** argv) {
	if (argc > 1) g_variant = atoi(argv[1]);
	int sx = argc > 2 ? atoi(argv[2]) : 100;
	int sy = argc > 3 ? atoi(argv[3]) : 100;
	int dx = argc > 4 ? atoi(argv[4]) : 400;
	int dy = argc > 5 ? atoi(argv[5]) : 400;

	bed_t bed;
	game_load_functions loader(bed.st);
	loader.regions_create();
	state_functions fn(bed.st);

	unit_t u{};
	u.unit_type = &bed.game_st.unit_types.vec[0];
	u.owner = 0;
	u.sprite = nullptr;
	u.position = xy(sx, sy);
	bed.unit = &u;

	state_functions::pathfinder pf;
	pf.u = &u;
	pf.source = xy(sx, sy);
	pf.destination = xy(dx, dy);
	pf.source_region = fn.get_region_at(xy(sx, sy));
	pf.destination_region = fn.get_region_at(xy(dx, dy));
	fn.pathfinder_find_long_path(pf, xy(sx, sy), xy(dx, dy));

	printf("long:");
	for (auto* r : pf.long_path) printf(" %zu", r->index);
	printf("\n");

	// The traced short path search.
	const regions_t::region* source_region = pf.source_region;
	const regions_t::region* destination_region = fn.get_region_at(pf.destination);
	const regions_t::region* target_region = pf.long_path.size() > 1 ? pf.long_path[1] : nullptr;
	xy target = pf.destination;
	printf("source=%zu dest=%zu target_region=%zu target=%d,%d\n",
		source_region->index, destination_region->index,
		target_region ? target_region->index : (size_t)-1, target.x, target.y);

	g_trace = true;
	g_step = 0;
	fn.pathfinder_find_short_path(pf, target, target_region);
	g_trace = false;

	printf("nodes=%zu open=%zu path=%zu\n", pf.short_all_nodes_size, pf.short_highest_open_size, pf.short_path.size());
	for (auto p : pf.short_path) printf(" %d,%d", p.x, p.y);
	printf("\n");
	return 0;
}
