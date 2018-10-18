//
//  Grid.cpp
//  Nurikabe
//
//  Created by Benjamin Luke on 8/26/18.
//  Copyright © 2018 Benjamin Luke. All rights reserved.
//

#include "Grid.hpp"
#include <iostream>
#include <algorithm>
#include <queue>

using namespace std;

Grid::Grid(int aWidth, int aHeight)
{
    // Init the grid with unknown cells of unknown coordinates
    // We actually need these unknown cells in the grid before loadGrid is executed because Region's addCell method checks the adjacent cells so there has to be something there otherwise we get exc bad access
    rows = vector<vector<Cell>>(aHeight, vector<Cell>(aWidth, Cell(Cell::Type::Unknown, Cell::Coordinate(-1,-1))));
    width = aWidth;
    height = aHeight;
    totalBlackCells = width * height;
}

void Grid::loadGrid(const string& numbers)
{
    auto i = numbers.cbegin();
    while (i != numbers.cend()) //TODO: Rewrite with for loop
    {
        // move forward until we find a character that is not a space
        while (*i == ' ')
        {
            auto coord = coordinateFromIterator(i, numbers);
            rows[coord.y][coord.x].coordinate = coord;
            unknownCellCoords.insert(coord);
            ++i;
            if (i == numbers.cend())
            {
                break;
            }
        }
        if (i == numbers.cend())
        {
            break;
        }
        
        // parse out the number - note that we support only one digit for the number
        auto number = *i-48;
        auto coord = coordinateFromIterator(i, numbers);
        
        totalBlackCells -= number;
        maxRegionSize = max(maxRegionSize, number);
        
        //TODO: Consider using move constructor
        rows[coord.y][coord.x].coordinate = coord;
        rows[coord.y][coord.x].type = Cell::Type::Numbered;
        rows[coord.y][coord.x].number = number;
        rows[coord.y][coord.x].region = shared_ptr<Region>(new Region(Region::Type::Numbered));
        rows[coord.y][coord.x].region->addCell(rows[coord.y][coord.x]);
        auto newAdjacentUnknownCells = cellCoordinatesAdjacentTo(Cell::CoordinateTypePair(coord, Cell::Type::Unknown));
        rows[coord.y][coord.x].region->adjacentUnknownCells.insert(newAdjacentUnknownCells.cbegin(), newAdjacentUnknownCells.cend());
        regions.insert(rows[coord.y][coord.x].region);
        
        numberOfKnownCells++;
        i++;
    }
}

//TODO: Switch to lambdas
vector<Grid::Cell::CoordinateTypePair> debugOutputHelper(vector<Grid::Cell::CoordinateTypePair>(Grid::*function)(), Grid& grid, const string& message)
{
    vector<Grid::Cell::CoordinateTypePair> changes = (grid.*function)();
#ifdef DEBUG
    if (!changes.empty())
    {
        cout << message << endl;
        for (auto change : changes)
        {
            cout << "(" << change.coord.x << "," << change.coord.y << ") ";
        }
        cout << endl;
        cout << "Known Cells: " << grid.numberOfKnownCells << endl << grid << endl;
    }
#endif
    return changes;
}

void Grid::solve(const vector<Grid::Cell::CoordinateTypePair>& cellsToMark = vector<Grid::Cell::CoordinateTypePair>())
{
    if (!cellsToMark.empty())
    {
        markCells(cellsToMark);
    }
    
    //TODO: Am I doing any undeed copies here?
    auto&& changes = debugOutputHelper(&Grid::applyRuleCompleteRegions, *this, "Complete Regions Rule Made Changes");
    if (!changes.empty()) solve(changes);

    changes = debugOutputHelper(&Grid::applyRuleMutipleAdjacency, *this, "Adjacency Rule Made Changes");
    if (!changes.empty()) solve(changes);

    changes = debugOutputHelper(&Grid::applyRuleElbow, *this, "Elbow Rule Made Changes");
    if (!changes.empty()) solve(changes);

    changes = debugOutputHelper(&Grid::applyRuleSinglePathwayBlack, *this, "Black Pathway Rule Made Changes");
    if (!changes.empty()) solve(changes);

    changes = debugOutputHelper(&Grid::applyRuleSinglePathwayWhite, *this, "White Pathway Rule Made Changes");
    if (!changes.empty()) solve(changes);
    
    changes = debugOutputHelper(&Grid::applyRuleN1, *this, "N-1 Rule Made Changes");
    if (!changes.empty()) solve(changes);
    
    changes = debugOutputHelper(&Grid::applyRuleUnreachable, *this, "Unreachable Rule Made Changes");
    if (!changes.empty()) solve(changes);
    
    changes = debugOutputHelper(&Grid::applyRuleGuessingUnreachable, *this, "Guessing Unreachable Rule Made Changes");
    if (!changes.empty()) solve(changes);
}

void Grid::solve()
{
    solve(vector<Grid::Cell::CoordinateTypePair>());
    #ifdef DEBUG
    cout << "Known Cells: " << this->numberOfKnownCells << endl << *this << endl;
    #endif
}

vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleCompleteRegions()
{
    // Set so that we don't mark any duplicates 
    auto coordsToMarkSet = set<Grid::Cell::CoordinateTypePair>();
    for (const auto& region : regions)
    {
        // TODO: If you guess one cell is black and then it makes an adjacent cell unreachable then the first cell is white
        // -> This is actually only for blocks of 4 cells and it is based on the no pools rule
        if (region->isComplete())
        {
            for (auto i = region->adjacentUnknownCells.cbegin(); i != region->adjacentUnknownCells.cend(); ++i)
            {
                coordsToMarkSet.insert(Cell::CoordinateTypePair(*i, Cell::Type::Black));
            }
        }
    }
    
    return vector<Grid::Cell::CoordinateTypePair>(coordsToMarkSet.cbegin(), coordsToMarkSet.cend());
}

vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleMutipleAdjacency()
{
    auto coordsToMark = vector<Cell::CoordinateTypePair>();
    for (auto i = unknownCellCoords.cbegin(); i != unknownCellCoords.cend(); ++i)
    {
        auto adjacentCellCoords = cellCoordinatesAdjacentTo(*i);
        
        int incompleteWhiteRegionCount = 0;
        for (auto i = adjacentCellCoords.cbegin(); i != adjacentCellCoords.cend(); ++i)
        {
            const auto& cellAtCoord = rows[i->y][i->x];
            
            if (cellAtCoord.region != nullptr &&
                !cellAtCoord.region->isComplete() &&
                cellAtCoord.region->type == Region::Type::Numbered)
            {
                incompleteWhiteRegionCount++;
            }
        }
        
        if (incompleteWhiteRegionCount >= 2)
        {
            coordsToMark.push_back(Cell::CoordinateTypePair(*i, Cell::Type::Black));
        }
    }
    
    return coordsToMark;
}

/// Returns the coordinate of the cell to change white based on the elbow rule
///
///     - Parameters:
///         - coord: The coordinate of black cell to use as the center of the elbow for testing the elbow rule
Grid::Cell::Coordinate Grid::coordinateToChangeWhiteBasedOnElbowRule(Cell::Coordinate coord) const
{
    Cell::Coordinate upCoord = Cell::Coordinate(coord.x, coord.y + 1);
    Cell::Coordinate leftCoord = Cell::Coordinate(coord.x - 1, coord.y);
    Cell::Coordinate downCoord = Cell::Coordinate(coord.x, coord.y - 1);
    Cell::Coordinate rightCoord = Cell::Coordinate(coord.x + 1, coord.y);
    
    auto checkCoords = [this] (Cell::Coordinate one, Cell::Coordinate two, Cell::Coordinate toMark) {
        return isCoordinateInBounds(one) &&
        isCoordinateInBounds(two) &&
        cellForCoordinate(one).type == Cell::Type::Black &&
        cellForCoordinate(two).type == Cell::Type::Black &&
        cellForCoordinate(toMark).type == Cell::Type::Unknown;
    };
    
    // check if we have an 'up left' opening elbow
    auto coordToMark = Cell::Coordinate(coord.x - 1, coord.y + 1);
    if (checkCoords(upCoord, leftCoord, coordToMark))
    {
        return coordToMark;
    }
    
    // check if we have an 'up right' opening elbow
    coordToMark = Cell::Coordinate(coord.x + 1, coord.y + 1);
    if (checkCoords(upCoord, rightCoord, coordToMark))
    {
        return coordToMark;
    }
    
    // check if we have a 'down left' opening elbow
    coordToMark = Cell::Coordinate(coord.x - 1, coord.y - 1);
    if (checkCoords(downCoord, leftCoord, coordToMark))
    {
        return coordToMark;
    }
    
    // check if we have a 'down right' opening elbow
    coordToMark = Cell::Coordinate(coord.x + 1, coord.y - 1);
    if (checkCoords(downCoord, rightCoord, coordToMark))
    {
        return coordToMark;
    }
    
    return Cell::Coordinate(UINT8_MAX, UINT8_MAX);
}

vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleElbow()
{
    auto coordsToMark = vector<Cell::CoordinateTypePair>();
    for (auto i = blackCellCoords.cbegin(); i != blackCellCoords.cend(); ++i)
    {
        auto coordToChangeWhite = coordinateToChangeWhiteBasedOnElbowRule(*i);
        if (isCoordinateInBounds(coordToChangeWhite))
        {
            coordsToMark.push_back(Cell::CoordinateTypePair(coordToChangeWhite, Cell::Type::White));
        }
    }
    return coordsToMark;
}

//TODO: These two rules are almost exactly the same we can combine them
vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleSinglePathwayWhite()
{
    // I need to look for all incomplete white regions and see if there is a single pathway out or not
    auto coordsToMarkSet = set<Grid::Cell::CoordinateTypePair>();
    for (auto i = regions.cbegin(); i != regions.cend(); ++i)
    {
        const auto& region = **i;
        // If the region is white it is by definition incomplete because it is not connected to it's numbered 'parent' region
        // We only want to apply this rule to incomplete regions
        if (region.type == Region::Type::Black) { continue; }
        if (region.type == Region::Type::Numbered && region.isComplete()) { continue; }
        
        if (region.adjacentUnknownCells.size() == 1)
        {
            coordsToMarkSet.insert(Cell::CoordinateTypePair(*region.adjacentUnknownCells.cbegin(), Cell::Type::White));
        }
    }
    
    return vector<Cell::CoordinateTypePair>(coordsToMarkSet.cbegin(), coordsToMarkSet.cend());
}

vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleSinglePathwayBlack()
{
    // I need to check all black regions and see if there is a single pathway out or not
    auto coordsToMarkSet = set<Cell::CoordinateTypePair>();
    for (auto i = regions.cbegin(); i != regions.cend(); ++i)
    {
        const auto& region = **i;
        if (region.type != Region::Type::Black) { continue; }
        
        // We have a black region, do we only have one possible path out of the black region?
        if (region.adjacentUnknownCells.size() == 1)
        {
            // We do! So we can mark this cell as black
            // Note that we can't mark the cell here because markCell modifies the regions set on Grid that we are iterating through
            coordsToMarkSet.insert(Cell::CoordinateTypePair(*region.adjacentUnknownCells.cbegin(), Cell::Type::Black));
        }
    }
    
    return vector<Cell::CoordinateTypePair>(coordsToMarkSet.cbegin(), coordsToMarkSet.cend());
}

bool Grid::areCoordinatesDiagonal(Cell::Coordinate one, Cell::Coordinate two) const
{
    return abs(one.x - two.x) == 1 && abs(one.y - two.y) == 1;
}

bool Grid::unreachable(Cell::Coordinate unknownCoord, set<Cell::Coordinate> visitedCoords = set<Cell::Coordinate>()) const
{
    if (rows[unknownCoord.y][unknownCoord.x].type != Cell::Type::Unknown) { abort(); }
    
    auto adjacentCells = cellCoordinatesAdjacentTo(unknownCoord);
    auto adjacentNumberedRegions = vector<shared_ptr<Region>>();
    auto adjacentWhiteRegions = vector<shared_ptr<Region>>();
    
    auto nodesToVisit = queue<pair<Cell::Coordinate, uint8_t>>();
    
    nodesToVisit.push(pair<Cell::Coordinate, uint8_t>(unknownCoord, 1));
    
    while (!nodesToVisit.empty())
    {
        // Grab the next node off of the queue
        pair<Cell::Coordinate, uint8_t> node = nodesToVisit.front();
        
        // Mark the node as visited
        nodesToVisit.pop();
        visitedCoords.insert(node.first);
        
        // We need to determine if we should visit this nodes adjacent cells
        auto adjacentCells = cellCoordinatesAdjacentTo(node.first);
        auto adjacentRegions = vector<shared_ptr<Region>>();
        transform(adjacentCells.cbegin(), adjacentCells.cend(), back_inserter(adjacentRegions), [this] (Cell::Coordinate coord) {
            return rows[coord.y][coord.x].region;
        });
        
        auto adjacentNumberedRegions = vector<shared_ptr<Region>>();
        copy_if(adjacentRegions.cbegin(), adjacentRegions.cend(), back_inserter(adjacentNumberedRegions), [] (shared_ptr<Region> regionPtr) -> bool {
            return regionPtr != nullptr && regionPtr->type == Region::Type::Numbered;
        });
        
        auto adjacentWhiteRegions = vector<shared_ptr<Region>>();
        copy_if(adjacentRegions.cbegin(), adjacentRegions.cend(), back_inserter(adjacentWhiteRegions), [] (shared_ptr<Region> regionPtr) -> bool {
            return regionPtr != nullptr && regionPtr->type == Region::Type::White;
        });
        
        uint8_t mergedWhiteRegionSize = 0;
        for (auto whiteRegion : adjacentWhiteRegions)
        {
            mergedWhiteRegionSize += whiteRegion->size;
        }
        
        for (auto numberedRegion : adjacentNumberedRegions)
        {
            mergedWhiteRegionSize += numberedRegion->size;
        }
        
        // If this cell is adjacent to two or more numbered regions we cannot add its adjacent cells to the list of cells to visit
        if (adjacentNumberedRegions.size() > 1) {
            continue;
        }
        
        if (adjacentNumberedRegions.size() == 1) {
            const int num = (*adjacentNumberedRegions.begin())->totalSize;
            
            if (node.second + mergedWhiteRegionSize <= num) {
                return false;
            } else {
                continue;
            }
        }
        
        // If we encounter a white region we need to check to see if the current size is less than the largest possible white region - 1
        // If it is then we have found a potentially valid path so return false, if visiting the cell would create a white region
        // that is larger than the largest possible white region then continue - we can't visit this node
        if (!adjacentWhiteRegions.empty()) {
            if (node.second + mergedWhiteRegionSize + 1 > maxRegionSize) {
                continue;
            } else {
                return false;
            }
        }
        
        // If we haven't hit any of the control flow breaks above then we need to add all unknown unvisited cells to the list of cells to visit
        for (auto unknownAdjacentCoord : cellCoordinatesAdjacentTo(Cell::CoordinateTypePair(node.first, Cell::Type::Unknown)))
        {
            if (visitedCoords.find(unknownAdjacentCoord) == visitedCoords.end())
            {
                nodesToVisit.push(pair<Cell::Coordinate, uint8_t>(unknownAdjacentCoord, node.second+1));
            }
        }
    }
    
    return true;
}

vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleUnreachable()
{
    auto coordsToMark = vector<Cell::CoordinateTypePair>();
    
    // For each unknown cell we need to do a breadth first search to find if a path exists from an unknown cell to a region
    // If there is no valid path from any numbered region to the unknown cell then that unknown cell is unreachable and must be black
    for (auto unknownCellCoord : unknownCellCoords)
    {
        if (unreachable(unknownCellCoord))
        {
            coordsToMark.push_back(Cell::CoordinateTypePair(unknownCellCoord, Cell::Type::Black));
        }
    }
    return coordsToMark;
}

// This is basically a variation on the pool rule, pools are not allowed so if marking one cell in a 4 cell block as black
// makes the only other cell in that 4 cell block black then it must be white because otherwise we would have a pool
vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleGuessingUnreachable()
{
    auto coordsToMark = vector<Cell::CoordinateTypePair>();
    
    for (int i = 0; i < width - 1; i++)
    {
        for (int j = 0; j < height - 1; j++)
        {
            // We need to sample squares of cells, and we are checking for the case that we have two black cells and two unknown cells
            vector<Cell::Coordinate> squareCoordinates =
            {
                Cell::Coordinate(i,     j),
                Cell::Coordinate(i + 1, j),
                Cell::Coordinate(i,     j + 1),
                Cell::Coordinate(i + 1, j + 1),
            };
            
            auto squareTypes = vector<Cell::Type>();
            auto unknownCoords = vector<Cell::Coordinate>();
            transform(squareCoordinates.cbegin(), squareCoordinates.cend(), back_inserter(squareTypes), [this] (Cell::Coordinate coord) {
                return rows[coord.y][coord.x].type;
            });
            copy_if(squareCoordinates.cbegin(), squareCoordinates.cend(), back_inserter(unknownCoords), [this] (Cell::Coordinate coord) {
                return rows[coord.y][coord.x].type == Cell::Type::Unknown;
            });
            
            int blackCount = 0;
            int unknownCount = 0;
            for (auto type : squareTypes)
            {
                if (type == Cell::Type::Black)
                {
                    blackCount++;
                }
                else if (type == Cell::Type::Unknown)
                {
                    unknownCount++;
                }
            }
            
            if (blackCount == 2 && unknownCount == 2)
            {
                // First try marking the first coordinate black and then test the second for unreachability
                auto firstUnknownCoord = unknownCoords.front();
                auto secondUnknownCoord = unknownCoords.back();
                if (unreachable(secondUnknownCoord, set<Cell::Coordinate>{ firstUnknownCoord }))
                {
                    // If setting the first coordinate as black made the second unreachable then we need to set the first white
                    coordsToMark.push_back(Cell::CoordinateTypePair(firstUnknownCoord, Cell::Type::White));
                }
                
                if (unreachable(firstUnknownCoord, set<Cell::Coordinate>{ secondUnknownCoord }))
                {
                    coordsToMark.push_back(Cell::CoordinateTypePair(secondUnknownCoord, Cell::Type::White));
                }
            }
        }
    }
    return coordsToMark;
}

vector<Grid::Cell::CoordinateTypePair> Grid::applyRuleN1()
{
    auto coordsToMark = vector<Cell::CoordinateTypePair>();
    for (auto i = regions.cbegin(); i != regions.cend(); ++i)
    {
        const auto& region = **i;
        if (region.type != Region::Type::Numbered) { continue; }
        
        // we have a numbered region, do we only have two possible pathways out and have we marked N-1 cells white?
        if (region.adjacentUnknownCells.size() == 2 && region.size == region.totalSize-1)
        {
            auto firstCoord = *region.adjacentUnknownCells.cbegin();
            auto secondCoord = *++region.adjacentUnknownCells.cbegin();
            
            if (areCoordinatesDiagonal(firstCoord, secondCoord))
            {
                // We have only two possible pathways and they are diagonal from eachother - we can mark the cell
                // We mark the cell that is adjacent to both unknown cells and is also unknown
                auto firstAdjacentCells = cellCoordinatesAdjacentTo(firstCoord);
                auto secondAdjacentCells = cellCoordinatesAdjacentTo(secondCoord);
                sort(firstAdjacentCells.begin(), firstAdjacentCells.end());
                sort(secondAdjacentCells.begin(), secondAdjacentCells.end());
                auto adjacentToBoth = vector<Cell::Coordinate>();
                set_intersection(firstAdjacentCells.cbegin(), firstAdjacentCells.cend(),
                                 secondAdjacentCells.cbegin(), secondAdjacentCells.cend(),
                                 back_inserter(adjacentToBoth));
                
                for (auto coord : adjacentToBoth)
                {
                    if (rows[coord.y][coord.x].type == Cell::Type::Unknown)
                    {
                        coordsToMark.push_back(Cell::CoordinateTypePair(coord, Cell::Type::Black));
                    }
                }
            }
        }
    }
    
    return coordsToMark;
}

void Grid::markCell(Cell::CoordinateTypePair pair)
{
    auto coord = pair.coord;
    auto type = pair.type;
    if (rows[coord.y][coord.x].type != Cell::Type::Unknown) { abort(); }
    
    // Grid state updates
    unknownCellCoords.erase(coord);
    if (type == Cell::Type::Black) { blackCellCoords.insert(coord); }
    numberOfKnownCells++;
    rows[coord.y][coord.x].type = type;
    
    // Region State Updates
        
    // First thing we need to do to keep the regions up to date is find all of the adjacent cells that are of the same type - we will need to merge all of these regions
    vector<Cell::Coordinate> adjacentCellCoords = cellCoordinatesAdjacentTo(coord);
    auto adjacentTypedCellCoords = vector<Cell::Coordinate>();
    
    copy_if(adjacentCellCoords.cbegin(), adjacentCellCoords.cend(), back_inserter(adjacentTypedCellCoords), [this, type] (Cell::Coordinate a) -> bool {
        if (type == Cell::Type::White)
        {
            return rows[a.y][a.x].type == Cell::Type::White || rows[a.y][a.x].type == Cell::Type::Numbered;
        }
        else
        {
            return rows[a.y][a.x].type == Cell::Type::Black;
        }
    });
    
    shared_ptr<Region> newRegionPtr = nullptr;
    if (adjacentTypedCellCoords.size() > 1)
    {
        // If there are more than 1 adjacent typed cell we need to merge regions
        auto regionsToMerge = vector<shared_ptr<Region>>();
        auto coords = adjacentTypedCellCoords;
        transform(coords.cbegin(), coords.cend(), back_inserter(regionsToMerge), [this] (Cell::Coordinate coord) {
            return rows[coord.y][coord.x].region;
        });
        
        newRegionPtr = mergeRegions(regionsToMerge);
    }
    else if (adjacentTypedCellCoords.size() == 1)
    {
        // If there is only one adjacent typed cell we can just adopt the region as our own
        auto coord = adjacentTypedCellCoords[0];
        newRegionPtr = rows[coord.y][coord.x].region;
    }
    else
    {
        // If there are no adjacent cells that are the same type as the cell that we are adding we need to create a new region
        Region::Type regionType;
        switch (type) {
            case Cell::Type::White:
                regionType = Region::Type::White;
                break;
            case Cell::Type::Black:
                regionType = Region::Type::Black;
                break;
            case Cell::Type::Numbered:
            case Cell::Type::Unknown:
                abort();
        }
        
        // If the black cell is isolated we need to make a new region
        newRegionPtr = make_shared<Region>(regionType);
        regions.insert(newRegionPtr);
    }
    
    // After we have the new region (which is either a newly created region or a merge of several regions we add the cell to it
    newRegionPtr->addCell(rows[coord.y][coord.x]);
    
    // Update the cell->shared_ptr<Region> pointer to point to the newly created/merged region
    rows[coord.y][coord.x].region = newRegionPtr;
    
    // Update the regions adjacent unknown cell list with the added cells adjacent cells
    auto newAdjacentUnknownCells = cellCoordinatesAdjacentTo(Cell::CoordinateTypePair(coord, Cell::Type::Unknown));
    newRegionPtr->adjacentUnknownCells.insert(newAdjacentUnknownCells.cbegin(), newAdjacentUnknownCells.cend());
    
    // erase the cell we just marked from every regions set of adjacent unknown cells
    for (auto i = regions.cbegin(); i != regions.cend(); ++i)
    {
        auto& region = **i;
        region.adjacentUnknownCells.erase(coord);
    }
}

void Grid::Region::mergeWith(const Region& region)
{
    if (region.type == Region::Type::Black && this->type != Region::Type::Black) { abort(); }
    if (region.type != Region::Type::Black && this->type == Region::Type::Black) { abort(); }
    
    size += region.size;
    totalSize = max(this->totalSize, region.totalSize);
    coordinates.insert(region.coordinates.cbegin(), region.coordinates.cend());
    adjacentUnknownCells.insert(region.adjacentUnknownCells.cbegin(), region.adjacentUnknownCells.cend());
}

shared_ptr<Grid::Region> Grid::mergeRegions(vector<shared_ptr<Region>>& regionsToMerge)
{
    if (regionsToMerge.size() < 2) { abort(); }
    
    // First sort regions on size
    sort(regionsToMerge.begin(), regionsToMerge.end());

    // Then iterate through the regions merging the next region with the last
    auto mergedRegion = *regionsToMerge.begin();
    for (auto i = regionsToMerge.begin()+1; i != regionsToMerge.end(); ++i)
    {
        shared_ptr<Region> regionToMergePtr = *i;
        mergedRegion->mergeWith(*regionToMergePtr);

        // For every cell that was part of region *regionToMergePtr we need to update it's region pointer
        for(auto cellCoordinateItr = (*i)->coordinates.cbegin(); cellCoordinateItr != (*i)->coordinates.cend(); ++cellCoordinateItr)
        {
            rows[cellCoordinateItr->y][cellCoordinateItr->x].region = mergedRegion;
        }
        
        // Finally we need to delete the regions that have been merged into a larger region
        regions.erase(*i);
    }
    
    // return the region that is now a merge of all the regions that were passed in
    return *regionsToMerge.begin();
}

void Grid::markCells(const std::vector<Cell::CoordinateTypePair>& cellCoordTypePairs) {
    for (auto i = cellCoordTypePairs.cbegin(); i != cellCoordTypePairs.cend(); ++i)
    {
        markCell(*i);
    }
}

bool Grid::isCoordinateInBounds(Grid::Cell::Coordinate coord) const
{
    if (coord.x < this->width &&
        coord.x >= 0 &&
        coord.y < this->height &&
        coord.y >= 0)
    {
        return true;
    }
    
    return false;
}

const Grid::Cell& Grid::cellForCoordinate(Cell::Coordinate coord) const
{
    return rows[coord.y][coord.x];
}

Grid::Cell::Coordinate Grid::coordinateFromIterator(const string::const_iterator& i, const string& string) const
{
    long index = (long)(i - string.cbegin()); //TODO: Verify that this is constant time or just track index
    unsigned short x = index % width;
    unsigned short y = index / width;
    
    return Cell::Coordinate(x, y);
}

//TODO: Convert this to use visitor pattern
//TODO: Add helpers to grab adjacent regions
vector<Grid::Cell::Coordinate> Grid::cellCoordinatesAdjacentTo(Cell::Coordinate coord) const
{
    return cellCoordinatesAdjacentTo(rows[coord.y][coord.x]);
}

vector<Grid::Cell::Coordinate> Grid::cellCoordinatesAdjacentTo(Cell::CoordinateTypePair coordTypePair) const
{
    auto adjacentCells = cellCoordinatesAdjacentTo(coordTypePair.coord);
    auto typedAdjacentCells = vector<Cell::Coordinate>();
    copy_if(adjacentCells.cbegin(), adjacentCells.cend(), back_inserter(typedAdjacentCells), [this, coordTypePair] (Cell::Coordinate coord) {
        return rows[coord.y][coord.x].type == coordTypePair.type;
    });
    return typedAdjacentCells;
}

vector<Grid::Cell::Coordinate> Grid::cellCoordinatesAdjacentTo(const Cell& cell) const
{
    auto up = Cell::Coordinate(cell.coordinate.x, cell.coordinate.y+1);
    auto down = Cell::Coordinate(cell.coordinate.x, cell.coordinate.y-1);
    auto left = Cell::Coordinate(cell.coordinate.x-1, cell.coordinate.y);
    auto right = Cell::Coordinate(cell.coordinate.x+1, cell.coordinate.y);
    
    auto cells = vector<Cell::Coordinate>();
    if (isCoordinateInBounds(up)) { cells.push_back( move(up) ); }
    if (isCoordinateInBounds(down)) { cells.push_back( move(down) ); }
    if (isCoordinateInBounds(left)) { cells.push_back( move(left) ); }
    if (isCoordinateInBounds(right)) { cells.push_back( move(right) ); }
    
    return cells;
}

void Grid::Region::addCell(const Grid::Cell& cell)
{
    // safety checks
    if (coordinates.count(cell.coordinate)) { abort(); }
    if (cell.type == Cell::Type::Black && this->type != Region::Type::Black) { abort(); }
    if (cell.type == Cell::Type::Numbered && this->type != Region::Type::Numbered) { abort(); }
    if (cell.type == Cell::Type::White && this->type == Region::Type::Black) { abort(); }
    // end safety checks
    
    coordinates.insert(cell.coordinate);
    size++;
    
    if (cell.type == Grid::Cell::Type::Numbered)
    {
        totalSize = cell.number;
    }
}

ostream& operator<<(ostream& o, const Grid::Cell& cell)
{
    switch (cell.type) {
        case Grid::Cell::Type::Numbered:
            o << cell.number;
            break;
            
        case Grid::Cell::Type::Unknown:
            o << 'U';
            break;
            
        case Grid::Cell::Type::White:
            o << 'W';
            break;
            
        case Grid::Cell::Type::Black:
            o << 'B';
            break;
            
        default:
            break;
    }
    
    return o;
}

ostream& operator<<(ostream& o, const Grid& grid)
{
    static int leftPadding = 3;
    
    // First print x axis
    o << string(leftPadding+1, ' ');
    for (int i = 0; i < grid.width; ++i) { o << i << " "; }
    o << endl << endl;
    
    // Then print the rest of the grid including y axis
    int currentRow = 0;
    const auto& constRows = grid.rows;
    for (auto&& row : constRows)
    {
        o << currentRow++ << string(leftPadding, ' ');
        const auto& constCells = row;
        for (auto&& cell : constCells)
        {
            o << cell << ' ';
        }
        o << endl;
    }
    
    return o;
}

Grid::Region::Region(Region::Type aType): type(aType) { }
Grid::Cell::Cell(Cell::Type aType, Cell::Coordinate aCoordinate): type(aType), coordinate(aCoordinate) { }
