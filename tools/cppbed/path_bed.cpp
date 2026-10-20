// Reference bed for the path execution layer: path_t lifecycle,
// path_progress() and unit_path_to().
//
// It drives a real ground unit through a sequence of move orders and
// advances it with the real movement integration, so the path state
// (waypoints, indices, re-planning) evolves exactly like in the engine.
//
//   g++ -std=c++17 -I../../../openbw -O0 -g path_bed.cpp -o path_bed.exe

#include "bwgame.h"

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
	return false;
}

static bool tile_is_middle(int tx, int ty) {
	return tile_is_wall(tx, ty);
}

// The tile flags of the synthetic map, matching src/Terrain.zan.
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
		game_st.max_unit_width = 64;
		game_st.max_unit_height = 64;

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

// One scripted scenario: a sequence of move orders. Each order is issued
// when the previous one's wait counter runs out.
struct scenario_t {
	const char* name;
	int sx, sy;
	int steps[6][3];   // dx, dy, wait_frames
	int step_count;
};

static const scenario_t scenarios[] = {
	{ "straight",   100, 100, {{ 800, 100, 4 }}, 1 },
	{ "diagonal",   100, 100, {{ 700, 700, 4 }}, 1 },
	{ "around",     100, 100, {{ 400, 400, 3 }, { 100, 700, 3 }}, 2 },
	{ "multistep",  100, 100, {{ 300, 100, 2 }, { 300, 300, 2 }, { 100, 600, 2 }}, 3 },
	{ "back",       600, 600, {{ 200, 200, 3 }}, 1 },
	{ "wallhit",    100, 100, {{ 1000, 1000, 4 }}, 1 },
	{ "same",       100, 100, {{ 0, 0, 3 }}, 1 },
	{ "near",       100, 100, {{ 12, 12, 2 }}, 1 },
	{ "turnback",   400, 400, {{ 600, 100, 3 }, { 100, 100, 3 }}, 2 },
	{ "longway",    100, 100, {{ 1800, 1800, 4 }}, 1 },
};

static void dump_path(const char* tag, unit_t* u) {
	printf("%s has_path=%d", tag, u->path ? 1 : 0);
	if (!u->path) {
		printf("\n");
		return;
	}
	path_t* p = u->path;
	printf(" delay=%d state_flags=%d cli=%zu csi=%zu full=%zu",
		p->delay, p->state_flags, p->current_long_path_index,
		p->current_short_path_index, p->full_long_path_size);
	printf(" source=%d,%d destination=%d,%d next=%d,%d",
		p->source.x, p->source.y, p->destination.x, p->destination.y, p->next.x, p->next.y);
	printf(" long:");
	// openbw's circular_vector has a const-correctness bug in begin() const,
	// so the non-const overload is the only one that compiles.
	for (auto i = p->long_path.begin(); i != p->long_path.end(); ++i) {
		printf(" %d", *i ? (int)(*i)->index : -1);
	}
	printf(" short:");
	for (auto i = p->short_path.begin(); i != p->short_path.end(); ++i) {
		printf(" %d,%d", i->x, i->y);
	}
	printf("\n");
}

int main(int argc, char** argv) {
	if (argc > 1) g_variant = atoi(argv[1]);
	int scen = argc > 2 ? atoi(argv[2]) : 0;
	if (scen < 0 || scen >= (int)(sizeof(scenarios) / sizeof(scenarios[0]))) return 1;
	const scenario_t& sc = scenarios[scen];

	bed_t bed;
	game_load_functions loader(bed.st);
	loader.regions_create();
	state_functions fn(bed.st);

	// A real unit with a real sprite, so the path code can use the sprite
	// position and the waypoint machinery.
	unit_t* u = fn.create_unit(&bed.game_st.unit_types.vec[0], xy(sc.sx, sc.sy), 0);
	if (!u) { printf("create_unit failed\n"); return 1; }
	u->next_speed = bed_flingy_type.top_speed;
	u->flingy_acceleration = bed_flingy_type.acceleration;
	u->next_velocity_direction = fn.direction_from_index(64);
	u->current_velocity_direction = fn.direction_from_index(64);
	u->desired_velocity_direction = fn.direction_from_index(64);
	bed.unit = u;

	printf("INITIAL flags=0x%x move_target=%d,%d next_waypoint=%d,%d next_target=%d,%d\n",
		u->movement_flags, u->move_target.pos.x, u->move_target.pos.y,
		u->next_movement_waypoint.x, u->next_movement_waypoint.y,
		u->next_target_waypoint.x, u->next_target_waypoint.y);
	printf("scenario=%s variant=%d\n", sc.name, g_variant);

	int step = 0;
	int frame = 0;
	int total_frames = 0;
	for (int i = 0; i < sc.step_count; ++i) total_frames += sc.steps[i][2];
	// A few extra frames so the path gets consumed and re-planned.
	total_frames += 8;
	int waits[8] = { 0 };

	while (frame < total_frames) {
		// Issue the next order when the previous step's wait is over.
		if (step < sc.step_count) {
			bool issue = (step == 0) || (waits[step - 1] >= sc.steps[step - 1][2]);
			if (issue) {
				xy to = xy(sc.sx + sc.steps[step][0], sc.sy + sc.steps[step][1]);
				bool moving = fn.is_moving_along_path(u);
				// The order sets the move target first, then the movement
				// state machine asks for a path towards it.
				fn.set_unit_move_target(u, to);
				bool ok = fn.unit_path_to(u, u->move_target.pos);
				printf("S %d frame=%d to=%d,%d moving=%d path_to=%d",
					step, frame, to.x, to.y, moving ? 1 : 0, ok ? 1 : 0);
				printf(" mt=%d,%d waypoint=%d,%d flags=0x%x",
					u->move_target.pos.x, u->move_target.pos.y,
					u->next_movement_waypoint.x, u->next_movement_waypoint.y,
					u->movement_flags);
				printf("\n");
				dump_path("  P", u);
				step++;
			} else {
				waits[step - 1]++;
			}
		}

		// Advance the unit one frame with the real movement integration.
		state_functions::execute_movement_struct ems;
		fn.update_unit_movement_values(u, ems);
		u->position = ems.position;
		u->exact_position = ems.exact_position;
		if (u->sprite) u->sprite->position = ems.position;
		u->next_speed = ems.speed;

		++frame;
		bed.st.current_frame = frame;
	}

	dump_path("FINAL", u);
	printf("pos=%d,%d mt=%d,%d\n", u->position.x, u->position.y,
		u->move_target.pos.x, u->move_target.pos.y);
	return 0;
}
