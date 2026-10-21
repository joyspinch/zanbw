#!/usr/bin/env bash
# The source lists of the differential test beds.
#
# Every check script needs a slightly different subset of src/, and the
# lists used to be spelled out inline in each script, which let them drift
# apart (a bed that missed a file failed to compile for reasons that had
# nothing to do with the port). The lists live here instead: one place to
# add a file, and the scripts only say which layer they need.
#
# The C++ reference beds (tools/cppbed) are excluded from the repository:
# they are development tools that include the real openbw source verbatim.

# The fixed point and direction arithmetic, plus the tables and the random
# number generator.
ZAN_SOURCES_BASE=(
    src/math/Fp.zan
    src/math/Dir.zan
    src/math/Tables.zan
    src/math/Box.zan
    src/core/Rng.zan
)

# The enums, the unit, the simulation, the movement kinematics and the
# scenarios.
ZAN_SOURCES_CORE=(
    src/core/Enums.zan
    src/core/Unit.zan
    src/core/UnitFinder.zan
    src/core/Sim.zan
    src/core/Kinematics.zan
    src/core/Scenario.zan
    src/core/TileMap.zan
    src/core/IntrusiveList.zan
)

# The regions layer: the tile map and the region decomposition.
ZAN_SOURCES_REGIONS=(
    src/regions/Regions.zan
    src/regions/RegionsCreate.zan
    src/regions/RegionsRefresh.zan
    src/regions/RegionsMain.zan
    src/regions/RegionsContours.zan
    src/regions/RegionsDump.zan
    src/regions/Terrain.zan
)

# The long path: the A* over the regions.
ZAN_SOURCES_ASTAR=(
    src/pathfind/Pathfinder.zan
    src/pathfind/AStar.zan
)

# The short path: the contour walk that fills in the path between the
# region waypoints.
ZAN_SOURCES_SHORTPATH=(
    src/pathfind/ShortPath.zan
    src/pathfind/ShortPathNeighbors.zan
    src/pathfind/ShortPathAddNeighbors.zan
    src/pathfind/ShortPathSearch.zan
    src/pathfind/ShortPathRun.zan
    src/pathfind/ShortPathNext.zan
)

# The path the unit carries, and the layer that turns a move target into
# one. It needs the short path, so the two are always used together.
ZAN_SOURCES_PATH=(
    "${ZAN_SOURCES_ASTAR[@]}"
    "${ZAN_SOURCES_SHORTPATH[@]}"
    src/pathfind/Path.zan
    src/pathfind/PathProgress.zan
)

# The collision layer: the unit finder, the collision rules and the
# movement states that drive them.
# The scenario tables the differential beds draw their situations from.
ZAN_SOURCES_SCENARIOS=(
    src/beds/ScenarioTables.zan
)

ZAN_SOURCES_COLLISION=(
    src/collision/Collision.zan
    src/collision/CollisionStates.zan
    src/collision/MovementStates.zan
    src/collision/StateMachine.zan
)

# The visibility layer: the sight range table and the tile reveal logic.
ZAN_SOURCES_VISION=(
    src/vision/SightValues.zan
    src/vision/Vision.zan
)

# The creep layer: the creep life bookkeeping and the creep rules.
ZAN_SOURCES_CREEP=(
    src/creep/Creep.zan
    src/creep/CreepLogic.zan
)
