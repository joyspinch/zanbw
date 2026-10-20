// C++ reference bed for the regions layer.
//
// The regions layer is the input of the pathfinder. It is normally built from
// the original game's Tileset/*.vf4 terrain data plus a map's tile layout;
// this bed drives the real regions_create() on a *synthetic* terrain so that
// the layer can be developed and diffed without the MPQ assets.
//
// The synthetic terrain is specified in TerrainSpec (below) and is mirrored
// by src/Terrain.zan in the Zan port. Both sides must generate exactly the
// same tiles/vf4 data.

#include "bwgame.h"
#include "game_types.h"
#include "data_types.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace bwgame;

static const int TILE_W = 64;
static const int TILE_H = 64;

// Which synthetic terrain to build; mirrored by src/Terrain.zan.
static int g_variant = 0;

// ---------------------------------------------------------------- terrain

// The synthetic terrains. Each one is picked by the variant index; both sides
// must generate exactly the same tiles.
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

// The "middle level" ground is a distinct tile flag, so that the region
// splitter sees more than one flag value.
static bool tile_is_middle(int tx, int ty) {
	return tile_is_wall(tx, ty);
}

// The tile flags of the synthetic map.
static uint16_t tile_flags(int tx, int ty) {
	if (g_variant == 6) {
		// partially walkable tiles: the split region pass has to split them
		// into two regions each
		return tile_t::flag_partially_walkable | tile_t::flag_walkable;
	}
	if (tile_is_middle(tx, ty)) return tile_t::flag_middle;
	return tile_t::flag_walkable;
}

// The mega tile index of each tile: 0 = fully walkable, 1 = fully blocked,
// 2 = partially walkable.
static int mega_tile_index(int tx, int ty) {
	if (g_variant == 6) return 2;
	return tile_is_wall(tx, ty) ? 1 : 0;
}

// ------------------------------------------------------------------- bed

struct regions_bed {
	global_state global_st;
	game_state game_st;
	state st;

	regions_bed() {
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

		// Synthetic vf4 table: entry 0 = fully walkable, entry 1 = fully
		// blocked, entry 2 = partially walkable (lower two rows only).
		// Each mega tile covers 4x4 tiles, each tile 4x4 walk cells.
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

static void dump_regions(regions_bed& bed) {
	const auto& regions = bed.game_st.regions;

	printf("regions: %zu\n", regions.regions.size());
	for (size_t i = 0; i < regions.regions.size(); ++i) {
		const auto& r = regions.regions[i];
		printf("R %zu idx=%zu flags=0x%04x tile_center=%d,%d tile_area=%d,%d,%d,%d "
		       "center=%d,%d area=%d,%d,%d,%d tiles=%zu group=%zu wn=%zu nwn=%zu walkable=%d\n",
			i, r.index, r.flags,
			(int)r.tile_center.x, (int)r.tile_center.y,
			(int)r.tile_area.from.x, (int)r.tile_area.from.y,
			(int)r.tile_area.to.x, (int)r.tile_area.to.y,
			(int)r.center.x.raw_value, (int)r.center.y.raw_value,
			r.area.from.x, r.area.from.y, r.area.to.x, r.area.to.y,
			r.tile_count, r.group_index,
			r.walkable_neighbors.size(), r.non_walkable_neighbors.size(),
			r.walkable() ? 1 : 0);
	}

	// neighbour lists, in order (the pathfinder iterates them in order)
	for (size_t i = 0; i < regions.regions.size(); ++i) {
		const auto& r = regions.regions[i];
		printf("N %zu w:", i);
		for (auto* n : r.walkable_neighbors) printf(" %zu", n->index);
		printf(" nw:");
		for (auto* n : r.non_walkable_neighbors) printf(" %zu", n->index);
		printf("\n");
	}

	// tile_region_index is indexed by tile; only the first TILE_W columns of
	// each 256-wide row are meaningful.
	printf("TRI");
	for (int y = 0; y < TILE_H; ++y) {
		for (int x = 0; x < TILE_W; ++x) {
			printf(" %zu", regions.tile_region_index[y * 256 + x]);
		}
	}
	printf("\n");

	printf("split_regions: %zu\n", regions.split_regions.size());
	for (size_t i = 0; i < regions.split_regions.size(); ++i) {
		const auto& s = regions.split_regions[i];
		printf("S %zu mask=0x%04x a=%zu b=%zu\n", i, s.mask,
			s.a ? s.a->index : (size_t)-1, s.b ? s.b->index : (size_t)-1);
	}

	for (size_t d = 0; d < 4; ++d) {
		const auto& c = regions.contours[d];
		printf("C %zu count=%zu\n", d, c.size());
		for (size_t i = 0; i < c.size(); ++i) {
			printf("C %zu %zu %d,%d,%d dir=%zu flags=0x%02x\n",
				d, i, c[i].v[0], c[i].v[1], c[i].v[2], c[i].dir, c[i].flags);
		}
	}
}

int main(int argc, char** argv) {
	if (argc > 1) {
		g_variant = atoi(argv[1]);
	}
	regions_bed bed;
	game_load_functions fn(bed.st);
	fn.regions_create();
	dump_regions(bed);
	return 0;
}
