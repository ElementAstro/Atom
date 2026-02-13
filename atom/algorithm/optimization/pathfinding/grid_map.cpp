#include "grid_map.hpp"

#include <array>
#include <limits>

namespace atom::algorithm {

//=============================================================================
// GridMap Implementation
//=============================================================================
GridMap::GridMap(i32 width, i32 height)
    : width_(width),
      height_(height),
      obstacles_(width * height, false),
      terrain_(width * height, TerrainType::Open) {}

GridMap::GridMap(std::span<const bool> obstacles, i32 width, i32 height)
    : width_(width),
      height_(height),
      obstacles_(obstacles.begin(), obstacles.end()),
      terrain_(width * height, TerrainType::Open) {
    for (usize i = 0; i < obstacles_.size(); ++i) {
        if (obstacles_[i]) {
            terrain_[i] = TerrainType::Obstacle;
        }
    }
}

GridMap::GridMap(std::span<const u8> obstacles, i32 width, i32 height)
    : width_(width),
      height_(height),
      obstacles_(width * height, false),
      terrain_(width * height, TerrainType::Open) {
    for (usize i = 0; i < obstacles_.size(); ++i) {
        if (obstacles[i] != 0) {
            obstacles_[i] = true;
            terrain_[i] = TerrainType::Obstacle;
        }
    }
}

std::vector<Point> GridMap::neighbors(const Point& p) const {
    std::vector<Point> result;
    result.reserve(8);

    static const std::array<std::pair<i32, i32>, 8> directions = {
        {{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};

    for (const auto& [dx, dy] : directions) {
        Point neighbor{p.x + dx, p.y + dy};
        if (isValid(neighbor)) {
            if (dx != 0 && dy != 0) {
                Point n1{p.x + dx, p.y};
                Point n2{p.x, p.y + dy};
                if (isValid(n1) && isValid(n2)) {
                    result.push_back(neighbor);
                }
            } else {
                result.push_back(neighbor);
            }
        }
    }

    return result;
}

std::vector<Point> GridMap::naturalNeighbors(const Point& p) const {
    std::vector<Point> result;
    result.reserve(8);

    static const std::array<std::pair<i32, i32>, 8> directions = {
        {{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}}};

    for (const auto& [dx, dy] : directions) {
        Point neighbor{p.x + dx, p.y + dy};

        if (isValid(neighbor)) {
            if (dx != 0 && dy != 0) {
                Point n1{p.x + dx, p.y};
                Point n2{p.x, p.y + dy};

                if (isValid(n1) && isValid(n2)) {
                    result.push_back(neighbor);
                }
            } else {
                result.push_back(neighbor);
            }
        }
    }

    return result;
}

f32 GridMap::cost(const Point& from, const Point& to) const {
    f32 baseCost;
    if (from.x != to.x && from.y != to.y) {
        baseCost = 1.414f;
    } else {
        baseCost = 1.0f;
    }

    return baseCost * getTerrainCost(getTerrain(to));
}

bool GridMap::isValid(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return false;
    }

    usize index = static_cast<usize>(p.y * width_ + p.x);
    return index < obstacles_.size() && !obstacles_[index] &&
           terrain_[index] != TerrainType::Obstacle;
}

void GridMap::setObstacle(const Point& p, bool isObstacle) {
    if (p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_) {
        usize index = p.y * width_ + p.x;
        obstacles_[index] = isObstacle;

        terrain_[index] =
            isObstacle ? TerrainType::Obstacle : TerrainType::Open;
    }
}

bool GridMap::hasObstacle(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return true;
    }

    usize index = static_cast<usize>(p.y * width_ + p.x);
    return index < obstacles_.size() && obstacles_[index];
}

void GridMap::setTerrain(const Point& p, TerrainType terrain) {
    if (p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_) {
        usize index = p.y * width_ + p.x;
        terrain_[index] = terrain;

        obstacles_[index] = (terrain == TerrainType::Obstacle);
    }
}

GridMap::TerrainType GridMap::getTerrain(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return TerrainType::Obstacle;
    }

    usize index = static_cast<usize>(p.y * width_ + p.x);
    return index < terrain_.size() ? terrain_[index] : TerrainType::Obstacle;
}

f32 GridMap::getTerrainCost(TerrainType terrain) const {
    switch (terrain) {
        case TerrainType::Open:
            return 1.0f;
        case TerrainType::Difficult:
            return 1.5f;
        case TerrainType::VeryDifficult:
            return 2.0f;
        case TerrainType::Road:
            return 0.8f;
        case TerrainType::Water:
            return 3.0f;
        case TerrainType::Obstacle:
        default:
            return std::numeric_limits<f32>::infinity();
    }
}

std::vector<Point> GridMap::getNeighborsForJPS(
    const Point& p, Direction allowedDirections) const {
    std::vector<Point> result;
    result.reserve(8);

    static const std::array<std::pair<i32, i32>, 8> offsets = {
        {{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}}};

    static const std::array<Direction, 8> dirs = {N, E, S, W, NE, SE, SW, NW};

    for (usize i = 0; i < offsets.size(); ++i) {
        if ((allowedDirections & dirs[i]) != dirs[i]) {
            continue;
        }

        const auto [dx, dy] = offsets[i];
        Point neighbor{p.x + dx, p.y + dy};

        if (isValid(neighbor)) {
            if (dx != 0 && dy != 0) {
                Point n1{p.x + dx, p.y};
                Point n2{p.x, p.y + dy};
                if (isValid(n1) && isValid(n2)) {
                    result.push_back(neighbor);
                }
            } else {
                result.push_back(neighbor);
            }
        }
    }

    return result;
}

bool GridMap::hasForced(const Point& p, Direction dir) const {
    if (!isValid(p)) {
        return false;
    }

    switch (dir) {
        case N:
            return (!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y + 1})) ||
                   (!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y + 1}));
        case E:
            return (!isValid({p.x, p.y - 1}) && isValid({p.x + 1, p.y - 1})) ||
                   (!isValid({p.x, p.y + 1}) && isValid({p.x + 1, p.y + 1}));
        case S:
            return (!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y - 1})) ||
                   (!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y - 1}));
        case W:
            return (!isValid({p.x, p.y - 1}) && isValid({p.x - 1, p.y - 1})) ||
                   (!isValid({p.x, p.y + 1}) && isValid({p.x - 1, p.y + 1}));
        case NE:
            return (dir == NE) &&
                   ((!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y + 1})) ||
                    (!isValid({p.x, p.y - 1}) && isValid({p.x + 1, p.y - 1})));
        case SE:
            return (dir == SE) &&
                   ((!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y - 1})) ||
                    (!isValid({p.x, p.y + 1}) && isValid({p.x + 1, p.y + 1})));
        case SW:
            return (dir == SW) &&
                   ((!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y - 1})) ||
                    (!isValid({p.x, p.y + 1}) && isValid({p.x - 1, p.y + 1})));
        case NW:
            return (dir == NW) &&
                   ((!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y + 1})) ||
                    (!isValid({p.x, p.y - 1}) && isValid({p.x - 1, p.y - 1})));
        default:
            return false;
    }
}

GridMap::Direction GridMap::getDirType(const Point& p,
                                       const Point& next) const {
    i32 dx = next.x - p.x;
    i32 dy = next.y - p.y;

    if (dx == 0 && dy == 1)
        return N;
    if (dx == 1 && dy == 0)
        return E;
    if (dx == 0 && dy == -1)
        return S;
    if (dx == -1 && dy == 0)
        return W;
    if (dx == 1 && dy == 1)
        return NE;
    if (dx == 1 && dy == -1)
        return SE;
    if (dx == -1 && dy == -1)
        return SW;
    if (dx == -1 && dy == 1)
        return NW;

    return NONE;
}

}  // namespace atom::algorithm
