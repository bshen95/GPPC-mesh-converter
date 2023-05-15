//
// Created by Bojie Shen on 20/5/2022.
//

#ifndef STARTKIT_GRID2POLY_H
#define STARTKIT_GRID2POLY_H
// vim: noet
/*
Takes a map file from stdin, and outputs a polygon map v1 file on stdout.
To do this, we do a floodfill from the edge of the map to assign a "elevation"
to all grid squares. To do this, we first assume that every square outside of
the map is traversable if we want an outside edge, or nontraversable if we do
not want an outside edge.
The elevation of a point is the minimum number of "traversability changes"
needed to get there. That is:
- any traversable area which is connected to a cell outside of the map has an
  elevation of 0
- any obstacle connected to a 0-elevation traversable cell has an elevation of 1
- any traversable area connected to a 1-elevation obstacle, but not connected to
  the outside of the map, has elevation 2.
- any obstacle connected to a 2-elevation traversable cell has an elevation of 3
and so on.
We then create the polygons from the "edges" where the elevation changes.
We do not create the first polygon as stated above, as that is trivial and
it brings in some edge cases.
Note that no "edge" is shared between two polygons, with the exception of the
edge of the map.

Can imagine the process as a Dijkstra though the graph of the grid, such that
whenever the traversability changes, it has a weight of 1, else, it has a
weight of 0.
*/
#include <iostream>
#include <string>
#include <utility>
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <stdlib.h>
#include <cassert>
#include <fstream>

#define FORMAT_VERSION 1
namespace grid2poly {
    const bool HAS_OUTSIDE = false;
    const bool DEBUG = false;

    typedef std::vector<bool> vbool;
    typedef std::vector<int> vint;

// Below is used for the data structure for
// {point on map : {polygon id : (point1, point2)}}
// for generating the polygons.
    typedef std::pair<int, int> point;
    typedef std::vector<point> vpoint;
    typedef std::map<int, vpoint> int_to_vpoint;
    typedef std::vector<int_to_vpoint> vint_to_vpoint;

// Search node used for the Dijkstra-like floodfill.
// We want to prioritise search nodes with a lower elevation, then the ones
// which have an ID (compared to the ones which have an ID of -1).
    struct search_node {
        int elevation;
        int id;
        point pos;

        bool operator<(const search_node &rhs) const {
            // We want the lowest elevations first.
            if (elevation != rhs.elevation) {
                return elevation < rhs.elevation;
            }
            // Then we want the HIGHEST IDs first to avoid -1s.
            return id > rhs.id;
        }


        bool operator>(const search_node &rhs) const {
            return rhs < *this;
        }
    };

    const int DX[] = {-1, 1, 0, 0};
    const int DY[] = {0, 0, -1, 1};

    const int DIAG_X[] = {-1, -1, 1, 1};
    const int DIAG_Y[] = {1, -1, 1, -1};

// Globals
// From the map
    std::vector<vbool> map_traversable;
    int map_width, map_height;

// Generated by program
    int next_id = 0;
    std::vector<vint> polygon_id;
    std::vector<int> id_to_elevation; // resize as necessary
    std::vector<point> id_to_first_cell; // resize with above
    std::vector<vint_to_vpoint> id_to_neighbours;

    std::vector<vpoint> id_to_polygon;

    void fail(std::string msg) {
        std::cerr << msg << std::endl;
        exit(1);
    }

    void read_map() {
        // Most of this code is from dharabor's warthog.
        // read in the whole map. ensure that it is valid.
        std::unordered_map<std::string, std::string> header;

        // header
        for (int i = 0; i < 3; i++) {
            std::string hfield, hvalue;
            if (std::cin >> hfield) {
                if (std::cin >> hvalue) {
                    header[hfield] = hvalue;
                } else {
                    fail("err; map has bad header");
                }
            } else {
                fail("err; map has bad header");
            }
        }

        if (header["type"] != "octile") {
            fail("err; map type is not octile");
        }

        // we'll assume that the width and height are less than INT_MAX
        map_width = atoi(header["width"].c_str());
        map_height = atoi(header["height"].c_str());

        if (map_width == 0 || map_height == 0) {
            fail("err; map has bad dimensions");
        }

        // we now expect "map"
        std::string temp_str;
        std::cin >> temp_str;
        if (temp_str != "map") {
            fail("err; map does not have 'map' keyword");
        }


        // basic checks passed. initialse the map
        map_traversable = std::vector<vbool>(map_height, vbool(map_width));
        // so to get (x, y), do map_traversable[y][x]
        // 0 is nontraversable, 1 is traversable

        // read in map_data
        int cur_y = 0;
        int cur_x = 0;

        char c;
        while (std::cin.get(c)) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // whitespace.
                // cannot put in the switch statement below as we need to check
                // "too many chars" before anything else
                continue;
            }

            if (cur_y == map_height) {
                fail("err; map has too many characters");
            }

            switch (c) {
                case 'S':
                case 'W':
                case 'T':
                case '@':
                case 'O':
                    // obstacle
                    map_traversable[cur_y][cur_x] = 0;
                    break;
                default:
                    // traversable
                    map_traversable[cur_y][cur_x] = 1;
                    break;
            }

            cur_x++;
            if (cur_x == map_width) {
                cur_x = 0;
                cur_y++;
            }
        }

        if (cur_y != map_height || cur_x != 0) {
            fail("err; map has too few characters");
        }
    }


    void get_id_and_elevation() {
        // Initialise polygon_id with -1s.
        polygon_id = std::vector<vint>(map_height, vint(map_width, -1));
        // Initialise id_to_elevation as empty vint.
        id_to_elevation.clear();

        // Do a Dijkstra-like floodfill. Need an "open list".
        // We want to prioritise search nodes with a lower elevation, then the ones
        // which have an ID.
        // typedef std::pair<int, point> search_node;
        std::priority_queue<search_node,
                std::vector<search_node>,
                std::greater<search_node>> open_list;

        // Initialise open list.
        // Go around edge of map and add in points: elevation 0 if traversable,
        // 1 if not.

        // Do the top row and bottom row first.
#define INIT(x, y) open_list.push({HAS_OUTSIDE != map_traversable[(y)][(x)], -1, {(x), (y)}})
        const int bottom_row = map_height - 1;
        for (int i = 0; i < map_width; i++) {
            INIT(i, 0);
            INIT(i, bottom_row);
        }

        // Then do the left and right columns.
        // Omit the top row and bottom row.
        const int right_col = map_width - 1;
        for (int i = 1; i < bottom_row; i++) {
            INIT(0, i);
            INIT(right_col, i);
        }
#undef INIT

        while (!open_list.empty()) {
            search_node c = open_list.top();
            open_list.pop();
            const int x = c.pos.first, y = c.pos.second;
            if (polygon_id[y][x] != -1) {
                // Already seen before, skip.
                continue;
            }
            //std::cerr << x << " " << y << std::endl;
            if (c.id == -1) {
                // Give it a new ID.
                c.id = next_id++;
                id_to_elevation.push_back(c.elevation);
                id_to_first_cell.push_back(c.pos);
            }
            polygon_id[y][x] = c.id;

            // Go through all neighbours.
            if (map_traversable[y][x]) {
                for (int i = 0; i < 4; i++) {
                    const int next_x = x + DIAG_X[i], next_y = y + DIAG_Y[i];
                    if (next_x < 0 || next_x >= map_width ||
                        next_y < 0 || next_y >= map_height) {
                        continue;
                    }


                    if (polygon_id[next_y][next_x] != -1) {
                        // Already seen before, skip.
                        // Checking this here is optional, but speeds up run time.
                        continue;
                    }


                    if (map_traversable[y][x] == map_traversable[next_y][next_x]) {
                        // same elevation, same id
                        open_list.push({c.elevation, c.id, {next_x, next_y}});
                    } else {
                        // new elevation, new id
                        // may have been traversed before but that case is handled above
                        open_list.push({c.elevation + 1, -1, {next_x, next_y}});
                    }
                }
            }
            for (int i = 0; i < 4; i++) {
                const int next_x = x + DX[i], next_y = y + DY[i];
                if (next_x < 0 || next_x >= map_width ||
                    next_y < 0 || next_y >= map_height) {
                    continue;
                }


                if (polygon_id[next_y][next_x] != -1) {
                    // Already seen before, skip.
                    // Checking this here is optional, but speeds up run time.
                    continue;
                }


                if (map_traversable[y][x] == map_traversable[next_y][next_x]) {
                    // same elevation, same id
                    open_list.push({c.elevation, c.id, {next_x, next_y}});
                } else {
                    // new elevation, new id
                    // may have been traversed before but that case is handled above
                    open_list.push({c.elevation + 1, -1, {next_x, next_y}});
                }
            }
        }
    }

    void make_edges() {
        // Fill in id_to_neighbours, which, for each lattice point, is a mapping
        // from an ID to the two neighbouring lattice points where the polygon
        // is connected to.

        id_to_neighbours = std::vector<vint_to_vpoint>(
                map_height + 1, vint_to_vpoint(map_width + 1));

        // First, iterate over each "horizontal" edge made by two vertically
        // adjacent cells. This includes cells "outside" of the map which we will
        // assume to be traversable and have a elevation of 0.

        // First, iterate over the y position of the horizontal edge.
        for (int edge = 0; edge < map_height + 1; edge++) {
            // The interesting cells we are looking for have a y position of
            // edge-1 and edge respectively.
            // Then we can iterate over the x values of the cells as normal.
            const bool is_top = edge == 0;
            const bool is_bot = edge == map_height;
            for (int x = 0; x < map_width; x++) {
                const int top_id = (is_top ? -1 : polygon_id[edge - 1][x]);
                const int bot_id = (is_bot ? -1 : polygon_id[edge][x]);
                const int top_ele = (is_top ? 0 : id_to_elevation[top_id]);
                const int bot_ele = (is_bot ? 0 : id_to_elevation[bot_id]);

                if (top_ele == bot_ele) {
                    // Same elevation, therefore no edge will be made.
                    continue;
                }
                const int id_of_edge = (top_ele > bot_ele ? top_id : bot_id);
                assert(id_of_edge != -1);

                // Now we got an edge and the ID it's correlated to.
                // For both points, we add the other point to the neighbours.
                id_to_neighbours[edge][x][id_of_edge].push_back({x + 1, edge});
                id_to_neighbours[edge][x + 1][id_of_edge].push_back({x, edge});
            }
        }

        // Now we iterate over the "vertical" edges made by two horizontally
        // adjacent cells.

        for (int edge = 0; edge < map_width + 1; edge++) {
            const bool is_left = edge == 0;
            const bool is_right = edge == map_width;
            for (int y = 0; y < map_height; y++) {
                const int left_id = (is_left ? -1 : polygon_id[y][edge - 1]);
                const int right_id = (is_right ? -1 : polygon_id[y][edge]);
                const int left_ele = (is_left ? 0 : id_to_elevation[left_id]);
                const int right_ele = (is_right ? 0 : id_to_elevation[right_id]);

                if (left_ele == right_ele) {
                    continue;
                }
                const int id_of_edge = (left_ele > right_ele ? left_id : right_id);
                assert(id_of_edge != -1);

                id_to_neighbours[y][edge][id_of_edge].push_back({edge, y + 1});
                id_to_neighbours[y + 1][edge][id_of_edge].push_back({edge, y});
            }
        }
    }

    void generate_polygons() {
        using namespace std;
        // Don't forget to initialise id_to_polygon!
        id_to_polygon = std::vector<vpoint>(next_id);
        // For each ID...
        for (int id = 0; id < next_id; id++) {
            if (DEBUG) cout << "this id = " << id << endl;
            // we first want to check whether the elevation is zero.
            if (id_to_elevation[id] == 0) {
                // If so, we want to continue on: this should be covered by the
                // big "overall" rectangle.
                continue;
            }
            // Then, we get a cell on the "border" of the polygon.
            // We can use the first seen cell for this.
            const point first_cell = id_to_first_cell[id];
            const int cell_x = first_cell.first, cell_y = first_cell.second;
            point last;

            // We know that some corner of the cell must have an edge of the polygon.
            // Go through all of them.
            for (int dx = 0; dx < 2; dx++) {
                for (int dy = 0; dy < 2; dy++) {
                    if (id_to_neighbours[cell_y + dy][cell_x + dx].count(id) != 0) {
                        last = {cell_x + dx, cell_y + dy};
                        goto found_point;
                    }
                }
            }
            assert(false);
            found_point:
            vpoint *cur_neighbours = &id_to_neighbours[last.second][last.first][id];
            if (DEBUG)
                cout << "last x = " << last.first << ", y = " << last.second
                     << endl << cur_neighbours->size() << endl;
            // vpoint *cur_poly = &id_to_polygon[id];

            point first_last = {-100, -100};

            assert(cur_neighbours->size() == 2 || cur_neighbours->size() == 4);
            // We now start going an arbitrary direction.
            // To do this, we need to keep track of our "last" point.
            point cur = cur_neighbours->at(0);

            map<point, size_t> p_size;

            // Now we keep going, adding corners until we go on the first corner.
            // We know we've reached a corner when the neighbours' x AND y values
            // are different.
            while (id_to_polygon[id].empty() || cur != id_to_polygon[id].front() || last != first_last) {
                assert(abs(cur.first - last.first) == 1 || abs(cur.second - last.second) == 1);
                cur_neighbours = &id_to_neighbours[cur.second][cur.first][id];
                if (DEBUG)
                    cout << "cur x = " << cur.first << ", y = " << cur.second
                         << endl << cur_neighbours->size() << endl;
                assert(cur_neighbours->size() == 2 || cur_neighbours->size() == 4);
                const point temp = cur;

                if (cur_neighbours->size() == 4) {
                    if (id_to_polygon[id].empty()) {
                        first_last = last;
                    }
                    id_to_polygon[id].push_back(cur);
                    if (p_size.count(cur) != 0) {
                        vpoint cut_off(id_to_polygon[id].begin() + p_size[cur], id_to_polygon[id].end());
                        id_to_polygon.push_back(cut_off);
                        id_to_polygon[id].resize(p_size[cur]);
                    } else {
                        p_size[cur] = id_to_polygon[id].size();
                    }
                    // As we're walking around an obstacle, all we need to check is
                    // "this" one.
                    if ((polygon_id[cur.second][cur.first] == id) == (id_to_elevation[id] % 2 == 1)) {
                        // It goes like:
                        // .@
                        // @.
                        // If we came from the right, go up, and vice versa.
                        // If we came from the left, go down, and vice versa.

                        // Coming from the left/right.
                        if (cur.first != last.first) {
                            // If cur.first - last.first is positive, we came from
                            // left. Then go down (add).
                            // Also works for right/up.
                            cur.second += (cur.first - last.first);
                        } else {
                            // If cur.second - last.second is positive, we came from
                            // up. Go right (add).
                            cur.first += (cur.second - last.second);
                        }
                    } else {
                        // It goes like:
                        // @.
                        // .@
                        // If we came from the right, go down, and vice versa.
                        // If we came from the left, go up, and vice versa.
                        // Coming from the left/right.
                        if (cur.first != last.first) {
                            // If cur.first - last.first is positive, we came from
                            // left. Then go up (subtract).
                            // Also works for right/down.
                            cur.second -= (cur.first - last.first);
                        } else {
                            // If cur.second - last.second is positive, we came from
                            // up. Go left (subtract).
                            cur.first -= (cur.second - last.second);
                        }
                    }
                } else {
                    if (cur_neighbours->at(0).first != cur_neighbours->at(1).first &&
                        cur_neighbours->at(0).second != cur_neighbours->at(1).second) {
                        if (id_to_polygon[id].empty()) {
                            first_last = last;
                        }
                        id_to_polygon[id].push_back(cur);
                    }
                    if (cur_neighbours->at(0) == last) {
                        cur = cur_neighbours->at(1);
                    } else {
                        cur = cur_neighbours->at(0);
                    }
                }

                last = temp;
            }
        }
    }

    void print_polymap() {
        std::cout << "poly" << std::endl;
        std::cout << FORMAT_VERSION << std::endl;

        // Get the number of polygons to print.
        // Start with 1 if the border is included.
        int num_polys = HAS_OUTSIDE;
        for (int id = 0; id < next_id; id++) {
            // We know a polygon won't be valid if its elevation is 0.
            if (id_to_elevation[id] != 0) {
                num_polys++;
            }
        }
        num_polys += ((int) id_to_polygon.size()) - next_id;

        std::cout << num_polys << std::endl;

        if (HAS_OUTSIDE) {
            // Print the first polygon.
            const point first_poly[] = {
                    {0,         0},
                    {map_width, 0},
                    {map_width, map_height},
                    {0,         map_height}
            };

            std::cout << 4 << " ";
            for (int i = 0; i < 4; i++) {
                std::cout << first_poly[i].first << " " << first_poly[i].second;
                if (i == 3) {
                    std::cout << std::endl;
                } else {
                    std::cout << " ";
                }
            }
        }

        // Print the polygons.
        for (size_t id = 0; id < id_to_polygon.size(); id++) {
            const vpoint &points = id_to_polygon[id];
            const size_t m = points.size();
            if (m == 0) {
                continue;
            }
            std::cout << m << " ";
            for (size_t index = 0; index < m; index++) {
                const point &cur_point = points[index];
                std::cout << cur_point.first << " " << cur_point.second;
                if (index == m - 1) {
                    std::cout << std::endl;
                } else {
                    std::cout << " ";
                }
            }
        }
    }


    void output_polymap(string filename) {
        ofstream fout(filename);

        fout << "poly" << std::endl;
        fout << FORMAT_VERSION << std::endl;

        // Get the number of polygons to print.
        // Start with 1 if the border is included.
        int num_polys = HAS_OUTSIDE;
        for (int id = 0; id < next_id; id++) {
            // We know a polygon won't be valid if its elevation is 0.
            if (id_to_elevation[id] != 0) {
                num_polys++;
            }
        }
        num_polys += ((int) id_to_polygon.size()) - next_id;

        fout << num_polys << std::endl;

        if (HAS_OUTSIDE) {
            // Print the first polygon.
            const point first_poly[] = {
                    {0,         0},
                    {map_width, 0},
                    {map_width, map_height},
                    {0,         map_height}
            };

            fout << 4 << " ";
            for (int i = 0; i < 4; i++) {
                fout << first_poly[i].first << " " << first_poly[i].second;
                if (i == 3) {
                    fout << std::endl;
                } else {
                    fout << " ";
                }
            }
        }

        // Print the polygons.
        for (size_t id = 0; id < id_to_polygon.size(); id++) {
            const vpoint &points = id_to_polygon[id];
            const size_t m = points.size();
            if (m == 0) {
                continue;
            }
            fout << m << " ";
            for (size_t index = 0; index < m; index++) {
                const point &cur_point = points[index];
                fout << cur_point.first << " " << cur_point.second;
                if (index == m - 1) {
                    fout << std::endl;
                } else {
                    fout << " ";
                }
            }
        }
    }

    void print_map() {
        for (auto row: map_traversable) {
            for (auto t: row) {
                std::cout << "X."[t];
            }
            std::cout << std::endl;
        }
    }

    void print_elevation() {
        for (auto row: polygon_id) {
            for (int id: row) {
                std::cout << id_to_elevation[id];
            }
            std::cout << std::endl;
        }
    }

    void print_ids() {
        for (auto row: polygon_id) {
            for (int id: row) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }
    }

    void print_id_to_polygon() {
        for (int id = 0; id < next_id; id++) {
            std::cout << id << std::endl;
            if (id_to_polygon[id].empty()) {
                std::cout << "empty" << std::endl;
            } else {
                for (point p: id_to_polygon[id]) {
                    std::cout << "(" << p.first << ", " << p.second << "); ";
                }
                std::cout << std::endl;
            }
        }
    }

    void convertGrid2Poly(const std::vector<bool> &bits, int width, int height, const std::string filename) {
        map_height = height;
        map_width = width;
        map_traversable = std::vector<vbool>(map_height, vbool(map_width));
        for (unsigned i = 0; i < bits.size(); i++) {
            int y = i / width;
            int x = i % width;
            map_traversable[y][x] = bits[i];
        }

        get_id_and_elevation();
        make_edges();
        generate_polygons();
//    print_polymap();
        output_polymap(filename);

    }
}

#endif //STARTKIT_GRID2POLY_H
