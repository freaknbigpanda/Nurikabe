//
//  main.cpp
//  Nurikabe
//
//  Created by Benjamin Luke on 8/26/18.
//  Copyright © 2018 Benjamin Luke. All rights reserved.
//

#include <iostream>
#include "Grid.hpp"
#include <CoreServices/CoreServices.h>
#include <chrono>
#include <sstream>

using namespace std;
using namespace std::chrono;

string formatTime(const steady_clock::time_point start, const steady_clock::time_point finish) {
    ostringstream oss;
    
    if (finish - start < 1ms) {
        oss << duration_cast<duration<double, micro>>(finish - start).count() << " microseconds";
    } else if (finish - start < 1s) {
        oss << duration_cast<duration<double, milli>>(finish - start).count() << " milliseconds";
    } else {
        oss << duration_cast<duration<double>>(finish - start).count() << " seconds";
    }
    
    return oss.str();
}

struct GridMetadata
{
    GridMetadata(const string& aGridString, int aWidth, int aHeight, const string& aName):
    gridString(aGridString),
    width(aWidth),
    height(aHeight),
    name(aName) { }
    
    string gridString;
    string name;
    int width;
    int height;
};

int main(int argc, const char * argv[]) {

    std::string easyWikipediaGrid =
    "1   4  4 2"
    "          "
    " 1   2    "
    "  1   1  2"
    "1    3    "
    "  6      5"
    "          "
    "     1   2"
    "    2  2  "
    "          ";
    
    auto easyWiki = GridMetadata(easyWikipediaGrid, 10, 10, "Easy Wikipedia Grid");

    std::string hardWikipediaGrid =
    "2        2"
    "      2   "
    " 2  7     "
    "          "
    "      3 3 "
    "  2    3  "
    "2  4      "
    "          "
    " 1    2 4 ";
    
    auto hardWiki = GridMetadata(hardWikipediaGrid, 10, 9, "Hard Wikipedia Grid");
    auto grids = vector<GridMetadata>{ easyWiki, hardWiki };
    
    for (const auto& gridMetdata : grids)
    {
        Grid grid(gridMetdata.width, gridMetdata.height);
        grid.loadGrid(gridMetdata.gridString);
        
        const auto start = chrono::steady_clock::now();
        
        grid.solve();
        
        const auto finish = chrono::steady_clock::now();
        
        cout << "Finished Grid " << gridMetdata.name << endl;
        cout << "Total execution time: " << formatTime(start, finish) << endl << endl;;
    }
    
    return 0;
}

