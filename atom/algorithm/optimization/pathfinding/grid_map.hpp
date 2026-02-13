#pragma once

#include <span>
#include <vector>

#include "graph.hpp"
#include "point.hpp"

namespace atom::algorithm {

//=============================================================================
// Grid Map Implementation
//=============================================================================
class GridMap : public IGraph<Point> {
public:
    // Movement direction flags
    enum Direction : u8 {
        NONE = 0,
        N = 1,       // 0001
        E = 2,       // 0010
        S = 4,       // 0100
        W = 8,       // 1000
        NE = N | E,  // 0011
        SE = S | E,  // 0110
        SW = S | W,  // 1100
        NW = N | W   // 1001
    };

    // Terrain types with associated costs
    enum class TerrainType : u8 {
        Open = 0,           // Normal passage area
        Difficult = 1,      // Difficult terrain (like gravel, tall grass)
        VeryDifficult = 2,  // Very difficult terrain (like swamps)
        Road = 3,           // Roads (faster movement)
        Water = 4,          // Water (passable by some units)
        Obstacle = 5        // Obstacle (impassable)
    };

    /**
     * @brief Construct an empty grid map
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(i32 width, i32 height);

    /**
     * @brief Construct a grid map with obstacles
     * @param obstacles Array of obstacles (true = obstacle, false = free)
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(std::span<const bool> obstacles, i32 width, i32 height);

    /**
     * @brief Construct a grid map with obstacles from u8 values
     * @param obstacles Array of obstacles (non-zero = obstacle, 0 = free)
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(std::span<const u8> obstacles, i32 width, i32 height);

    // IGraph implementation
    std::vector<Point> neighbors(const Point& p) const override;
    f32 cost(const Point& from, const Point& to) const override;

    // Advanced neighborhood function with directional constraints for JPS
    std::vector<Point> getNeighborsForJPS(const Point& p,
                                          Direction allowedDirections) const;

    // Natural neighbors - returns only naturally accessible neighbors (no
    // diagonal movement if blocked)
    std::vector<Point> naturalNeighbors(const Point& p) const;

    // GridMap specific methods
    bool isValid(const Point& p) const;
    void setObstacle(const Point& p, bool isObstacle);
    bool hasObstacle(const Point& p) const;

    // Terrain functions
    void setTerrain(const Point& p, TerrainType terrain);
    TerrainType getTerrain(const Point& p) const;
    f32 getTerrainCost(TerrainType terrain) const;

    // Utility methods for JPS algorithm
    bool hasForced(const Point& p, Direction dir) const;
    Direction getDirType(const Point& p, const Point& next) const;

    // Accessors
    i32 getWidth() const { return width_; }
    i32 getHeight() const { return height_; }

    // Get position from index
    Point indexToPoint(i32 index) const {
        return {index % width_, index / width_};
    }

    // Get index from position
    i32 pointToIndex(const Point& p) const { return p.y * width_ + p.x; }

private:
    i32 width_;
    i32 height_;
    std::vector<bool>
        obstacles_;  // Can be replaced with terrain type matrix in the future
    std::vector<TerrainType> terrain_;  // Terrain types
};

}  // namespace atom::algorithm
