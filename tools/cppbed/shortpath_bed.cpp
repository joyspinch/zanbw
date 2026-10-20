// C++ reference bed for the short-path (contour) search.
//
// The short path search refines each hop of the long path: it walks the
// contour edges of the walkable area, avoiding units and terrain, and
// produces a sequence of positions for the unit to follow.
//
// This bed builds the same synthetic terrain as tools/cppbed/regions_bed.cpp,
// creates one ground unit, and runs the real pathfinder_find() over a set of
// start/destination pairs.
//
// The Zan port mirrors it in src/ShortPath.zan.

#include "bwgame.h"
#include "game_types.h"
#include "data_types.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace bwgame;

static const int TILE_W = 64;
static const int TILE_H = 64;

static int g_variant = 0;

// ---------------------------------------------------------------- terrain

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
	if (g_variant == 2) {
		return (tx % 8 == 0) != (ty % 8 == 0);
	}
	if (g_variant == 3) {
		if (ty >= 20 && ty < 44) {
			if (tx >= 8 && tx < 30) return true;
			if (tx >= 34 && tx < 56) return true;
		}
		return false;
	}
	if (g_variant == 4) {
		return false;
	}
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
	return false;
}

static bool tile_is_middle(int tx, int ty) {
	return tile_is_wall(tx, ty);
}

static uint16_t tile_flags(int tx, int ty) {
	if (g_variant == 6) {
		return tile_t::flag_partially_walkable | tile_t::flag_walkable;
	}
	if (tile_is_middle(tx, ty)) return tile_t::flag_middle;
	return tile_t::flag_walkable;
}

static int mega_tile_index(int tx, int ty) {
	if (g_variant == 6) return 2;
	return tile_is_wall(tx, ty) ? 1 : 0;
}

// ------------------------------------------------------------------- bed

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

// A ground unit: 16x16 pixels, no flyer flag.
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

struct shortpath_bed {
	global_state global_st;
	game_state game_st;
	state st;
	unit_t* unit = nullptr;

	shortpath_bed() {
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
				t.flags = tile_flags(x, y);
				st.tiles_mega_tile_index[y * TILE_W + x] = mega_tile_index(x, y);
			}
		}
	}
};

struct query_t {
	int sx, sy;
	int dx, dy;
};

int main(int argc, char** argv) {
	if (argc > 1) {
		g_variant = atoi(argv[1]);
	}

	shortpath_bed bed;
	game_load_functions loader(bed.st);
	loader.regions_create();

	state_functions fn(bed.st);

	// A ground unit at a fixed position; the search starts from it.
	unit_t u{};
	u.unit_type = &bed.game_st.unit_types.vec[0];
	u.owner = 0;
	u.sprite = nullptr;
	u.position = xy(100, 100);
	bed.unit = &u;

	std::vector<query_t> queries;
	queries.push_back({ 100, 100, 1800, 1800 });
	queries.push_back({ 100, 100, 400, 400 });
	queries.push_back({ 100, 100, 1900, 100 });
	queries.push_back({ 100, 100, 100, 1900 });
	queries.push_back({ 100, 100, 1600, 1600 });
	queries.push_back({ 100, 100, 100, 100 });
	queries.push_back({ 100, 100, 900, 100 });
	queries.push_back({ 100, 100, 100, 900 });
	queries.push_back({ 100, 100, 1984, 1984 });
	queries.push_back({ 100, 100, 500, 1200 });

	for (size_t q = 0; q < queries.size(); ++q) {
		const auto& qq = queries[q];
		state_functions::pathfinder pf;
		pf.u = &u;
		pf.source = xy(qq.sx, qq.sy);
		pf.destination = xy(qq.dx, qq.dy);
		bool ok = fn.pathfinder_find(pf);
		printf("Q %zu from=%d,%d to=%d,%d ok=%d", q, qq.sx, qq.sy, qq.dx, qq.dy, ok ? 1 : 0);
		if (ok) {
			printf(" reached=%d stuck=%d", pf.destination_reached ? 1 : 0, pf.is_stuck ? 1 : 0);
			printf(" dest=%d,%d", pf.destination.x, pf.destination.y);
			printf(" long=%zu short=%zu", pf.long_path.size(), pf.short_path.size());
			printf(" short_nodes=%zu short_open=%zu",
				pf.short_all_nodes_size, pf.short_highest_open_size);
			printf(" short_path:");
			for (auto p : pf.short_path) {
				printf(" %d,%d", p.x, p.y);
			}
		}
		printf("\n");
	}

	return 0;
}
