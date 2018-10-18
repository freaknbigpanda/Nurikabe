//
//  Grid.hpp
//  Nurikabe
//
//  Created by Benjamin Luke on 8/26/18.
//  Copyright © 2018 Benjamin Luke. All rights reserved.
//

#ifndef Grid_hpp
#define Grid_hpp

#include <stdio.h>
#include <string>
#include <set>
#include <utility>
#include <vector>
#include <memory>
#include <ostream>

class Grid
{
public:
    Grid(const Grid&) = delete;
    Grid& operator=(const Grid&) = delete;
    
    friend std::ostream& operator<<(std::ostream&, const Grid&);
    
    /// Default constructor for the Nurikabe grid.
    ///
    /// - Parameters:
    ///     - width: The width of the grid.
    ///     - height: The height of the grid.
    ///     - nummbers: A string that specifies all of the starting numbers in the grid seperated by spaces. The largest number a cell can currently have is 9.
    Grid(int width, int height);
    
    /// After you construct the grid, you use this method to load it with data before calling solve
    ///
    /// - Parameters:
    ///     - nummbers: A string that specifies all of the starting numbers in the grid seperated by spaces. The largest number a cell can currently have is 9.
    void loadGrid(const std::string& numbers);
    
    void solve();
    int width;
    int height;
    
private:
    struct Region;
    struct Cell
    {
        enum class Type : unsigned short
        { White, Black, Unknown, Numbered };
        
        /// The coordinate of the cell - this value is guaranteed to be unique unless the string input to the solver is malformed
        struct Coordinate
        {
            Coordinate(int aX, int aY): x(aX), y(aY) {};
            unsigned short x;
            unsigned short y;
            
            bool operator <(const Coordinate& coord) const
            {
                // If our y coordinates are equal then use x to determine order,
                // otherwise use y. i.e. top left corner is lowest bottom right corner is highest.
                if (y == coord.y )
                {
                    return x < coord.x;
                }
                else
                {
                    return y < coord.y;
                }
            }
        };
        
        struct CoordinateTypePair
        {
            CoordinateTypePair(Coordinate aCoord, Type aType): coord(aCoord), type(aType) {};
            Cell::Coordinate coord;
            Cell::Type type;
            
            bool operator <(const CoordinateTypePair& coordTypePair) const
            {
                return coord < coordTypePair.coord;
            }
        };
        
        Cell(Type type, Coordinate coordinate);
        
        std::shared_ptr<Region> region;
        int number = -1;
        Coordinate coordinate;
        Type type = Type::Unknown;
    };
    
    struct Region
    {
        enum class Type
        { White, Black, Numbered };
        
        Region(Type type);
        Region(const Region&) = delete;
        Region& operator=(const Region&) = delete;
        
        Type type;
        int size = 0;
        int totalSize = -1;
        
        void addCell(const Cell& cell);
        bool isComplete() const { return totalSize == size; };
        
        bool operator <(const Region& region) const
        {
            return size < region.size;
        }
        
        void mergeWith(const Region&);
        
        std::set<Cell::Coordinate> coordinates = std::set<Cell::Coordinate>();
        std::set<Cell::Coordinate> adjacentUnknownCells = std::set<Cell::Coordinate>();
    };
    
    friend std::vector<Cell::CoordinateTypePair> debugOutputHelper(std::vector<Cell::CoordinateTypePair>(Grid::*)(), Grid&, const std::string&);
    
    /// This rule states that any complete white regions must be bordered by black cells
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Cell::CoordinateTypePair> applyRuleCompleteRegions();
    
    /// This rule states that if a cell is adjacent to two or more numbers it must be black
    /// TODO: I belive this can be generalized to apply to cells that are adjacent to incomplete white regions
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Cell::CoordinateTypePair> applyRuleMutipleAdjacency();
    
    /// This rule states that we cannot have a 2x2 square of black cells so if there is a 'L' shaped elbow of black cells then the cell in the curve must be white
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Cell::CoordinateTypePair> applyRuleElbow();
    
    /// Since all black cells must be connected if there is a region of black cells with only one adjacent unknown cell then that cell must be black
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Cell::CoordinateTypePair> applyRuleSinglePathwayBlack();
    
    /// If there is only one way for an incomplete white region to expand then that unknown cell is white
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Cell::CoordinateTypePair> applyRuleSinglePathwayWhite();
    
    /// If a numbered region has N-1 cells (it only needs one more marked cell to be considered complete) and there are only two options left to expand and those options touch diagonally then the cell in between is black
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Grid::Cell::CoordinateTypePair> applyRuleN1();
    
    /// If the shortest path from an unknown cell to a numbered region would make the numbered region too big then that cell is unreachable and should be marked black
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Grid::Cell::CoordinateTypePair> applyRuleUnreachable();
    
    /// If you have a 2x2 square of cells with 2 black and 2 unknown and marking one of the unknown cells as black causes the other to be unreachable then that is a contradiction via the no-pool-rule so the guessed black cell must be white.
    ///
    /// - Returns: A vector containing pairs of coordinates along with the type that they should be marked or an empty vector if the rule could not be applied.
    std::vector<Grid::Cell::CoordinateTypePair> applyRuleGuessingUnreachable();
    
    /// Find if a unknown cell is not connectable to any white or numbered region
    ///
    /// - Parameters:
    ///     - coord: The coordinate of the unknown cell that will be used as a starting point for the depth first search
    /// - Returns: true if no path was found, otherwise false
    bool unreachable(Cell::Coordinate unknownCoord, std::set<Cell::Coordinate>) const;
    
    /// This is a helper function that calls markCell for each CoordinateTypePair in the std::vector of CoordinateTypePairs
    void markCells(const std::vector<Cell::CoordinateTypePair>&);
    
    /// Modifies the rows and regions containers in Grid with the CoordinateTypePair information
    ///
    /// - Discussion: This method is responsible for keeping all of the internal state inside Grid consistent.
    /// This includes merging regions, creating new regions, erasing regions that have been merged into other regions etc etc.
    /// This method is not thread safe at all. When this method executes all of the internal state of Grid will be modified.
    void markCell(Cell::CoordinateTypePair);
    
    // TODO: Think about adding noexcept everywhere
    // Internal State
    long numberOfKnownCells = 0;
    int maxRegionSize = 0;
    int totalBlackCells = 0;
    std::vector<std::vector<Cell>> rows;
    std::set<std::shared_ptr<Region>> regions = std::set<std::shared_ptr<Region>>();
    std::shared_ptr<Region> mergeRegions(std::vector<std::shared_ptr<Region>>&);
    std::set<Cell::Coordinate> unknownCellCoords = std::set<Cell::Coordinate>();
    std::set<Cell::Coordinate> blackCellCoords = std::set<Cell::Coordinate>();
    
    // Helpers
    std::vector<Cell::Coordinate> cellCoordinatesAdjacentTo(Cell::CoordinateTypePair) const;
    std::vector<Cell::Coordinate> cellCoordinatesAdjacentTo(Cell::Coordinate) const;
    std::vector<Cell::Coordinate> cellCoordinatesAdjacentTo(const Cell&) const;
    bool isCoordinateInBounds(Cell::Coordinate) const;
    bool areCoordinatesDiagonal(Cell::Coordinate, Cell::Coordinate) const;
    Cell::Coordinate coordinateToChangeWhiteBasedOnElbowRule(Cell::Coordinate) const;
    const Cell& cellForCoordinate(Cell::Coordinate) const;
    void solve(const std::vector<Cell::CoordinateTypePair>&);
    Cell::Coordinate coordinateFromIterator(const std::string::const_iterator&, const std::string&) const;

    friend std::ostream& operator<<(std::ostream&, const Grid::Cell&);
};

std::ostream& operator<<(std::ostream&, const Grid::Cell&);
std::ostream& operator<<(std::ostream&, const Grid&);

#endif /* Grid_hpp */
