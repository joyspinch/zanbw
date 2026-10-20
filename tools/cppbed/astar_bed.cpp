// C++ reference bed for the long-path (region level) A* search.
//
// The long path search is the coarse part of the pathfinder: it walks the
// region adjacency graph to produce a sequence of regions that the unit
// should pass through. The short path search then refines each hop.
//
// This bed builds the same synthetic terrain as tools/cppbed/regions_bed.cpp,
// runs the real regions_create(), and then runs the real
// pathfinder_find_long_path() over a set of start/destination pairs.
//
// The Zan port mirrors it in src/AStar.zan.

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

struct astar_bed {
	global_state global_st;
	game_state game_st;
	state st;

	astar_bed() {
		st.global = &global_st;
		st.game = &game_st;

		game_st.map_tile_width = TILE_W;
		game_st.map_tile_height = TILE_H;
		game_st.map_walk_width = TILE_W * 4;
		game_st.map_walk_height = TILE_H * 4;
		game_st.map_width = TILE_W * 32;
		game_st.map_height = TILE_H * 32;
		game_st.tileset_index = 0;

		st.tiles.resize(TILE_W * TILE_H);
		st.tiles_mega_tile_index.resize(TILE_W * TILE_H);

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

// A set of start/destination pairs, chosen so that they cover same-region,
// cross-region, unreachable and cross-group queries.
struct query_t {
	int sx, sy;
	int dx, dy;
};

int main(int argc, char** argv) {
	if (argc > 1) {
		g_variant = atoi(argv[1]);
	}

	astar_bed bed;
	game_load_functions loader(bed.st);
	loader.regions_create();

	state_functions fn(bed.st);

	std::vector<query_t> queries;
	queries.push_back({ 64, 64, 1984, 1984 });     // corner to corner
	queries.push_back({ 64, 64, 100, 100 });       // same region
	queries.push_back({ 200, 200, 1800, 200 });   // across the vertical wall
	queries.push_back({ 100, 100, 1500, 1500 });   // diagonal
	queries.push_back({ 64, 1984, 1984, 64 });     // other diagonal
	queries.push_back({ 500, 500, 700, 700 });     // inside the block
	queries.push_back({ 900, 200, 1000, 200 });    // same region, short
	queries.push_back({ 1600, 1600, 300, 300 });   // across the staircase
	queries.push_back({ 32, 32, 2000, 32 });      // along the top
	queries.push_back({ 32, 2000, 2000, 2000 });  // along the bottom
	queries.push_back({ 1000, 1000, 1000, 1000 }); // degenerate
	queries.push_back({ 1900, 1900, 64, 64 });     // reverse of the first

	for (size_t q = 0; q < queries.size(); ++q) {
		const auto& qq = queries[q];
		state_functions::pathfinder pf;
		xy from(qq.sx, qq.sy);
		xy to(qq.dx, qq.dy);
		bool ok = fn.pathfinder_find_long_path(pf, from, to);
		printf("Q %zu from=%d,%d to=%d,%d ok=%d", q, qq.sx, qq.sy, qq.dx, qq.dy, ok ? 1 : 0);
		if (ok) {
			printf(" src_region=%zu dst_region=%zu full=%zu path=%zu",
				pf.source_region->index, pf.destination_region->index,
				pf.full_long_path_size, pf.long_path.size());
			printf(" nodes=%zu highest_open=%zu",
				pf.long_all_nodes_size, pf.long_highest_open_size);
			printf(" path:");
			for (auto* r : pf.long_path) {
				printf(" %zu", r->index);
			}
		}
		printf("\n");
	}

	return 0;
}
