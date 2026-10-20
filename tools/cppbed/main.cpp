// ZanBW C++ reference bed.
//
// This drives the REAL openbw code (bwgame.h is #included verbatim) against a
// minimal synthetic data environment, and prints a per-frame trace. The trace
// is the bitwise ground truth that the Zan port is diffed against.
//
// Build:
//   g++ -std=c++17 -I../../../openbw -O0 -g main.cpp -o bed.exe
// Run:
//   ./bed.exe <frames>            -> trace on stdout
//   ./bed.exe --dump-tables       -> emit src/Tables.zan
#include "bwgame.h"
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace bwgame;

// ---------------------------------------------------------------- data setup

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

static flingy_type_t bed_flingy_types[2] = {
	[] {
		flingy_type_t t{};
		t.id = (FlingyTypes)0;
		t.sprite = type_id<sprite_type_t>(&bed_sprite_type);
		t.top_speed = fp8::integer(385);
		t.acceleration = 12_fp8;
		t.halt_distance = fp8::integer(500);
		t.turn_rate = 11_fp8;
		t.movement_type = 1;
		return t;
	}(),
	[] {
		flingy_type_t t{};
		t.id = (FlingyTypes)1;
		t.sprite = type_id<sprite_type_t>(&bed_sprite_type);
		t.top_speed = fp8::integer(100);
		t.acceleration = 4_fp8;
		t.halt_distance = fp8::integer(1000);
		t.turn_rate = 5_fp8;
		t.movement_type = 1;
		return t;
	}(),
};

static unit_type_t bed_unit_types[2] = {
	[] {
		unit_type_t t{};
		t.id = (UnitTypes)0;
		t.flingy = type_id<const flingy_type_t>(&bed_flingy_types[0]);
		t.hitpoints = fp8::integer(400);
		t.elevation_level = 12;
		t.flags = (unit_type_t::flags_t)(unit_type_t::flag_can_move | unit_type_t::flag_can_turn | unit_type_t::flag_flyer);
		t.sight_range = 7;
		t.dimensions.from = xy(-21, -15);
		t.dimensions.to = xy(21, 15);
		t.supply_provided = fp1::zero();
		t.supply_required = fp1::zero();
		t.group_flags = GroupFlags::Independant;
		return t;
	}(),
	[] {
		unit_type_t t{};
		t.id = (UnitTypes)1;
		t.flingy = type_id<const flingy_type_t>(&bed_flingy_types[1]);
		t.hitpoints = fp8::integer(400);
		t.elevation_level = 12;
		t.flags = (unit_type_t::flags_t)(unit_type_t::flag_can_move | unit_type_t::flag_can_turn | unit_type_t::flag_flyer);
		t.sight_range = 7;
		t.dimensions.from = xy(-21, -15);
		t.dimensions.to = xy(21, 15);
		t.supply_provided = fp1::zero();
		t.supply_required = fp1::zero();
		t.group_flags = GroupFlags::Independant;
		return t;
	}(),
};

static iscript_t bed_iscript = [] {
	iscript_t t;
	// script 0: [wait 1][goto 1] - spins forever, never touches images.
	// index 0 is reserved (iscript rejects program_counter == 0), data starts at 1.
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

struct bed_t {
	global_state global_st;
	game_state game_st;
	state st;

	bed_t() {
		global_st.iscript = bed_iscript;
		global_st.image_grp.push_back(&bed_grp);

		for (auto& v : bed_flingy_types) global_st.flingy_types.vec.push_back(v);
		for (auto& v : bed_unit_types) game_st.unit_types.vec.push_back(v);

		// order types vector indexed by Orders enum value; Nothing=23, Fatal=188
		global_st.order_types.vec.resize(189);
		global_st.order_types.vec[(size_t)Orders::Nothing].id = Orders::Nothing;
		global_st.order_types.vec[(size_t)Orders::Fatal].id = Orders::Fatal;

		global_st.image_types.vec.push_back(bed_image_type);
		global_st.sprite_types.vec.push_back(bed_sprite_type);

		game_st.map_tile_width = 128;
		game_st.map_tile_height = 128;
		game_st.map_walk_width = 512;
		game_st.map_walk_height = 512;
		game_st.map_width = 4096;
		game_st.map_height = 4096;
		game_st.tileset_index = 0;

		// sight values for range 7; only the air path (ext_masked_count) is used
		game_st.sight_values = {};
		{
			sight_values_t& sv = game_st.sight_values.at(7);
			sv.min_width = 15;
			sv.max_width = 15;
			sv.min_height = 15;
			sv.max_height = 15;
			sv.min_mask_size = 225;
			sv.ext_masked_count = 225;
			for (int y = -7; y <= 7; ++y) {
				for (int x = -7; x <= 7; ++x) {
					sight_values_t::maskdat_node_t n{};
					n.prev = 0;
					n.prev2 = ~(size_t)0;
					n.x = x;
					n.y = y;
					n.relative_tile_index = y * 128 + x;
					sv.maskdat.push_back(n);
				}
			}
		}

		st.global = &global_st;
		st.game = &game_st;
		st.current_frame = 0;
		st.lcg_rand_state = 42;
		st.random_counts = {};
		st.total_random_counts = 0;

		game_st.repulse_field_width = (game_st.map_width + 47) / 48;
		game_st.repulse_field_height = (game_st.map_height + 47) / 48;
		st.repulse_field.resize(game_st.repulse_field_width * game_st.repulse_field_height);

		st.tiles.resize(game_st.map_tile_width * game_st.map_tile_height);
		st.tiles_mega_tile_index.resize(game_st.map_tile_width * game_st.map_tile_height);
		st.sprites_on_tile_line.resize(game_st.map_tile_height);

		for (int i = 0; i < 12; ++i) {
			st.players[i].controller = player_t::controller_computer;
		}
	}
};

// ------------------------------------------------------------------- driver

struct bed_functions : state_functions {
	bed_functions(state& st) : state_functions(st) {}

	void play_sound(int, xy, const unit_t*, bool) override {}
	void on_unit_deselect(unit_t*) override {}
	void on_unit_destroy(unit_t*) override {}
	void on_kill_unit(unit_t*) override {}
	void on_player_eliminated(int) override {}
	void on_victory_state(int, int) override {}
};

static int dump_tables() {
	printf("using System;\n\n");
	printf("// Static tables mechanically dumped from openbw (bwgame.h) by tools/cppbed.\n");
	printf("// Do not hand-edit: regenerate with `bed.exe --dump-tables`.\n\n");
	printf("public class Tabs {\n");
	printf("    public static long[] TanTable = new long[] {\n        ");
	for (size_t i = 0; i < tan_table.size(); ++i) {
		printf("%u, ", (unsigned)tan_table[i]);
	}
	printf("\n    };\n\n");
	printf("    // 256 entries, each {x_raw, y_raw} of fp8\n");
	printf("    public static long[] DirTable = new long[] {\n");
	for (size_t i = 0; i < 256; i += 4) {
		printf("        ");
		for (size_t j = 0; j < 4; ++j) {
			printf("%d, %d, ", (int)direction_table[i + j].x.raw_value, (int)direction_table[i + j].y.raw_value);
		}
		printf("\n");
	}
	printf("    };\n\n");
	printf("    public static long[] RepulseAdjustTable = new long[] {\n        ");
	for (size_t i = 0; i < repulse_adjust_table.size(); ++i) {
		printf("%d, ", repulse_adjust_table[i]);
	}
	printf("\n    };\n}\n");
	return 0;
}

int main(int argc, char** argv) {
	if (argc > 1 && strcmp(argv[1], "--dump-tables") == 0) {
		return dump_tables();
	}

	// Argument order is flexible: the scenario token (starting with 's') and
	// the frame count may appear in either order.
	int frames = 600;
	std::string scenario;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (!a.empty() && a[0] == 's') scenario = a;
		else frames = atoi(a.c_str());
	}

	// Scenario configuration. The same scenario string is fed to the Zan
	// port, so both sides run identical situations.
	//
	//   s<idx>:<x>,<y>:<tx>,<ty>[:<speed>,<accel>,<turn>][;<unit>...]
	//
	// A unit spec is  x,y,tx,ty  (start position and move target).
	// The first unit uses flingy type 0, the rest use type 1.
	//
	// Built-in scenarios (used when no scenario is given):
	//   0  two units crossing diagonally (straight-line + turning)
	//   1  two units converging on the same spot (repulse field)
	//   2  one unit driving into the map edge (bounds clamping)
	struct unit_cfg {
		int x, y, tx, ty;
	};
	std::vector<unit_cfg> units;
	int top_speed = 385, acceleration = 12, turn_rate = 11;

	if (!scenario.empty()) {
		// parse "s<idx>:..." or "s<idx>:x,y,tx,ty;x,y,tx,ty"
		std::string spec = scenario;
		size_t colon = spec.find(':');
		std::string head = (colon == std::string::npos) ? spec : spec.substr(0, colon);
		std::string body = (colon == std::string::npos) ? std::string() : spec.substr(colon + 1);
		int idx = atoi(head.c_str() + 1);
		if (idx == 0) {
			units.push_back({500, 500, 3600, 3600});
			units.push_back({3500, 3500, 400, 400});
		} else if (idx == 1) {
			units.push_back({1500, 1500, 2000, 2000});
			units.push_back({2000, 2000, 1500, 1500});
		} else if (idx == 2) {
			units.push_back({3900, 2000, 4096, 2000});
		} else if (idx == 3) {
			units.push_back({100, 100, 2000, 2000});
			units.push_back({150, 150, 2100, 2100});
		} else if (idx == 4) {
			units.push_back({2000, 2000, 2200, 2000});
			units.push_back({2000, 2050, 2200, 2050});
		} else if (idx == 5) {
			// two units on the exact same spot, both heading the same way:
			// they share a repulse cell and push each other apart
			units.push_back({2000, 2000, 3000, 2000});
			units.push_back({2000, 2000, 3000, 2000});
		} else if (idx == 6) {
			// three units converging on one point from three directions
			units.push_back({1900, 2000, 2000, 2000});
			units.push_back({2100, 2000, 2000, 2000});
			units.push_back({2000, 1900, 2000, 2000});
		} else if (idx == 7) {
			// unit reversing direction: target behind the start position
			units.push_back({2000, 2000, 1000, 2000});
		} else if (idx == 8) {
			// diagonal move with a long turn at the end
			units.push_back({1000, 1000, 3000, 1000});
			units.push_back({3000, 1000, 3000, 3000});
		}
		if (!body.empty()) {
			units.clear();
			std::stringstream ss(body);
			std::string item;
			while (std::getline(ss, item, ';')) {
				int x, y, tx, ty;
				if (sscanf(item.c_str(), "%d,%d,%d,%d", &x, &y, &tx, &ty) == 4) {
					units.push_back({x, y, tx, ty});
				}
			}
		}
	} else {
		units.push_back({500, 500, 3600, 3600});
		units.push_back({3500, 3500, 400, 400});
	}

	bed_t bed;
	bed_functions fn(bed.st);
	fn.update_tiles = false;

	// patch the flingy type parameters for this scenario
	bed_flingy_types[0].top_speed = fp8::integer(top_speed);
	bed_flingy_types[0].acceleration = fp8::from_raw(acceleration);
	bed_flingy_types[0].turn_rate = fp8::from_raw(turn_rate);

	std::vector<unit_t*> made;
	for (size_t i = 0; i < units.size(); ++i) {
		unit_t* u = fn.create_unit((UnitTypes)(i == 0 ? 0 : 1), xy(units[i].x, units[i].y), 0);
		if (!u) { fprintf(stderr, "create_unit failed (last_error=%d)\n", bed.st.last_error); return 1; }
		made.push_back(u);
	}

	// real "unit becomes active" path: unhide, move to visible list, insert
	// finder, reset movement state
	for (unit_t* u : made) fn.refresh_unit_position(u);

	// movement issued directly at the flingy layer (bypasses orders/pathing)
	for (size_t i = 0; i < made.size(); ++i) {
		fn.set_flingy_move_target(made[i], xy(units[i].tx, units[i].ty));
		fn.set_next_target_waypoint(made[i], xy(units[i].tx, units[i].ty));
	}

	for (int frame = 0; frame < frames; ++frame) {
		bed.st.current_frame = frame;

		for (unit_t* u : ptr(bed.st.visible_units)) {
			fn.iscript_flingy = u;
			fn.iscript_unit = u;
			fn.update_unit_movement(u);
		}

		printf("F %d", frame);
		for (unit_t* u : ptr(bed.st.visible_units)) {
			printf(" | %d %d %d %d %d %d %lld %lld %d %d %d %d %d %d %lld %d",
				(int)u->position.x, (int)u->position.y,
				(int)u->exact_position.x.raw_value, (int)u->exact_position.y.raw_value,
				(int)u->heading.raw_value,
				(int)u->current_velocity_direction.raw_value,
				(long long)u->current_speed.raw_value,
				(long long)u->velocity.x.raw_value,
				(int)u->movement_state,
				(int)u->movement_flags,
				(int)u->flingy_turn_rate.raw_value,
				(int)u->repulse_flags,
				(int)u->repulse_direction.raw_value,
				(int)u->next_velocity_direction.raw_value,
				(long long)u->next_speed.raw_value,
				(int)(u->path ? 1 : 0));
		}
		printf("\n");
	}

	// Audit line: identical format to the Zan port, printed last so that a
	// plain diff of the two traces covers it as well.
	printf("lcg_state=%u total_rand=%d sources:", bed.st.lcg_rand_state, bed.st.total_random_counts);
	for (int i = 0; i < 256; ++i) {
		if (bed.st.random_counts[i]) printf(" %d:%d", i, bed.st.random_counts[i]);
	}
	printf("\n");
	return 0;
}
