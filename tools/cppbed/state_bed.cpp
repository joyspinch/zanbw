// Reference bed for the movement state machine: the path-following states
// of openbw's execute_movement() dispatcher.
//
// It drives a real ground unit through scripted move orders and lets the
// real state machine run, so the movement state, the path state and the
// unit's position all evolve exactly like in the engine.
//
//   g++ -std=c++17 -I../../../openbw -O0 -g state_bed.cpp -o state_bed.exe
//
//   state_bed.exe <terrain variant> <scenario>

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
	{ "retarget",   100, 100, {{ 200, 200, 1 }, { 600, 600, 1 }, { 100, 900, 1 }}, 3 },
	{ "stuck",      100, 100, {{ 100, 100, 1 }, { 1900, 100, 1 }}, 2 },
	{ "reverse",    800, 100, {{ 600, 100, 2 }, { 100, 400, 2 }}, 2 },
	{ "tight",      400, 400, {{ 40, 40, 1 }, { -40, 0, 1 }}, 2 },
	{ "slow",       100, 100, {{ 300, 300, 6 }}, 1 },
	{ "far",        100, 100, {{ 1700, 300, 3 }}, 1 },
};

static const char* state_name(int s) {
	switch (s) {
	case movement_states::UM_Init: return "Init";
	case movement_states::UM_InitSeq: return "InitSeq";
	case movement_states::UM_Lump: return "Lump";
	case movement_states::UM_Turret: return "Turret";
	case movement_states::UM_Bunker: return "Bunker";
	case movement_states::UM_BldgTurret: return "BldgTurret";
	case movement_states::UM_Hidden: return "Hidden";
	case movement_states::UM_Flyer: return "Flyer";
	case movement_states::UM_FakeFlyer: return "FakeFlyer";
	case movement_states::UM_AtRest: return "AtRest";
	case movement_states::UM_Dormant: return "Dormant";
	case movement_states::UM_AtMoveTarget: return "AtMoveTarget";
	case movement_states::UM_CheckIllegal: return "CheckIllegal";
	case movement_states::UM_MoveToLegal: return "MoveToLegal";
	case movement_states::UM_LumpWannabe: return "LumpWannabe";
	case movement_states::UM_FailedPath: return "FailedPath";
	case movement_states::UM_RetryPath: return "RetryPath";
	case movement_states::UM_StartPath: return "StartPath";
	case movement_states::UM_UIOrderDelay: return "UIOrderDelay";
	case movement_states::UM_TurnAndStart: return "TurnAndStart";
	case movement_states::UM_FaceTarget: return "FaceTarget";
	case movement_states::UM_NewMoveTarget: return "NewMoveTarget";
	case movement_states::UM_AnotherPath: return "AnotherPath";
	case movement_states::UM_Repath: return "Repath";
	case movement_states::UM_RepathMovers: return "RepathMovers";
	case movement_states::UM_FollowPath: return "FollowPath";
	case movement_states::UM_ScoutPath: return "ScoutPath";
	case movement_states::UM_ScoutFree: return "ScoutFree";
	case movement_states::UM_FixCollision: return "FixCollision";
	case movement_states::UM_WaitFree: return "WaitFree";
	case movement_states::UM_GetFree: return "GetFree";
	case movement_states::UM_SlidePrep: return "SlidePrep";
	case movement_states::UM_SlideFree: return "SlideFree";
	case movement_states::UM_ForceMoveFree: return "ForceMoveFree";
	case movement_states::UM_FixTerrain: return "FixTerrain";
	case movement_states::UM_TerrainSlide: return "TerrainSlide";
	default: return "?";
	}
}

static void dump_unit(const char* tag, unit_t* u) {
	printf("%s state=%s pos=%d,%d exact=%d,%d speed=%d next_speed=%d"
		   " vel=%d,%d vdir=%d cdir=%d ddir=%d heading=%d"
		   " mt=%d,%d waypoint=%d,%d target_waypoint=%d,%d flags=0x%x"
		   " pcc=%d status=0x%x has_path=%d",
		tag, state_name(u->movement_state),
		u->sprite->position.x, u->sprite->position.y,
		(int)u->exact_position.x.raw_value, (int)u->exact_position.y.raw_value,
		(int)u->current_speed.raw_value, (int)u->next_speed.raw_value,
		(int)u->velocity.x.raw_value, (int)u->velocity.y.raw_value,
		(int)u->next_velocity_direction.raw_value, (int)u->current_velocity_direction.raw_value,
		(int)u->desired_velocity_direction.raw_value, (int)u->heading.raw_value,
		u->move_target.pos.x, u->move_target.pos.y,
		u->next_movement_waypoint.x, u->next_movement_waypoint.y,
		u->next_target_waypoint.x, u->next_target_waypoint.y,
		u->movement_flags, u->pathing_collision_counter,
		u->status_flags, u->path ? 1 : 0);
	if (u->path) {
		path_t* p = u->path;
		printf(" path(delay=%d sf=%d cli=%zu csi=%zu src=%d,%d dst=%d,%d next=%d,%d long=%zu short=%zu)",
			p->delay, p->state_flags, p->current_long_path_index,
			p->current_short_path_index, p->source.x, p->source.y,
			p->destination.x, p->destination.y, p->next.x, p->next.y,
			p->long_path.size(), p->short_path.size());
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
	loader.generate_sight_values();
	loader.regions_create();
	state_functions fn(bed.st);

	unit_t* u = fn.create_unit(&bed.game_st.unit_types.vec[0], xy(sc.sx, sc.sy), 0);
	if (!u) { printf("create_unit failed\n"); return 1; }
	u->next_speed = bed_flingy_type.top_speed;
	u->flingy_acceleration = bed_flingy_type.acceleration;
	u->next_velocity_direction = fn.direction_from_index(64);
	u->current_velocity_direction = fn.direction_from_index(64);
	u->desired_velocity_direction = fn.direction_from_index(64);
	// openbw: initialize_unit hides every non-building unit; the engine
	// reveals it through show_unit() once it comes into vision. The vision
	// refresh is not part of this bed, so reveal the unit directly.
	u->sprite->flags &= ~sprite_t::flag_hidden;
	bed.unit = u;

	printf("scenario=%s variant=%d\n", sc.name, g_variant);

	int step = 0;
	int frame = 0;
	int total_frames = 0;
	for (int i = 0; i < sc.step_count; ++i) total_frames += sc.steps[i][2];
	// A few extra frames so the path gets consumed and the unit settles.
	total_frames += 10;
	int waits[8] = { 0 };

	while (frame < total_frames) {
		if (step < sc.step_count) {
			bool issue = (step == 0) || (waits[step - 1] >= sc.steps[step - 1][2]);
			if (issue) {
				xy to = xy(sc.sx + sc.steps[step][0], sc.sy + sc.steps[step][1]);
				// The order sets the move target; the state machine then
				// asks for a path towards it.
				fn.set_unit_move_target(u, to);
				printf("S %d frame=%d to=%d,%d\n", step, frame, to.x, to.y);
				dump_unit("  U", u);
				step++;
			} else {
				waits[step - 1]++;
			}
		}

		// One frame of the real movement state machine.
		fn.execute_movement(u);
		++frame;
		bed.st.current_frame = frame;

		if ((frame % 4) == 0) {
			char tag[8];
			snprintf(tag, sizeof(tag), "F%d", frame);
			dump_unit(tag, u);
		}
	}

	dump_unit("FINAL", u);
	return 0;
}
