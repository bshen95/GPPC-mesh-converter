//
// Created by Bojie Shen on 15/5/2023.
//
#include <cstdio>
#include <ios>
#include <numeric>
#include <algorithm>
#include <string>
#include <unistd.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include "mesh2merged.h"
#include "grid2poly.h"
#include "poly2mesh.h"
#include "grid2rect.h"

std::string mapfile, outputfile, flag;
std::vector<bool> mapData;
int width, height;
bool grid2REC   = false;
bool grid2CDT   = false;
bool grid2MCDT = false;


std::string removeFileExtension(const std::string& filename) {
    // Find the position of the last dot
    size_t dotPos = filename.find_last_of(".");

    if (dotPos != std::string::npos) {
        // Extract the substring before the last dot
        return filename.substr(0, dotPos);
    }

    // If no dot is found, return the original filename
    return filename;
}

bool parse_argv(int argc, char **argv) {
    if (argc < 2) return false;
    flag = std::string(argv[1]);
    if (flag== "-rec") grid2REC =  true;
    else if (flag == "-cdt") grid2CDT = true;
    else if (flag == "-mcdt") grid2MCDT = true;


    if (argc < 3) return false;
    mapfile = std::string(argv[2]);

    outputfile = removeFileExtension(mapfile);

    return true;
}

void print_help(char **argv) {
    std::printf("Invalid Arguments\nUsage %s <flag> <map>\n", argv[0]);
    std::printf("Flags:\n");
    std::printf("\t-rec : Convert grid map to rectangle mesh\n");
    std::printf("\t-cdt : Convert grid map to CDT mesh\n");
    std::printf("\t-mcdt : Convert grid map to Merged CDT mesh\n");
}


std::string basename(const std::string& path) {
    std::size_t l = path.find_last_of('/');
    if (l == std::string::npos) l = 0;
    else l += 1;
    std::size_t r = path.find_last_of('.');
    if (r == std::string::npos) r = path.size()-1;
    return path.substr(l, r-l);
}


void LoadMap(const char *fname, std::vector<bool> &map, int &width, int &height)
{
    FILE *f;
    f = std::fopen(fname, "r");
    if (f)
    {
        std::fscanf(f, "type octile\nheight %d\nwidth %d\nmap\n", &height, &width);
        map.resize(height*width);
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                char c;
                do {
                    std::fscanf(f, "%c", &c);
                } while (std::isspace(c));
                map[y*width+x] = (c == '.' || c == 'G' || c == 'S');
            }
        }
        std::fclose(f);
    }
}

int main(int argc, char **argv)
{

    if (!parse_argv(argc, argv)) {
        print_help(argv);
        std::exit(1);
    }

    // in mapData, 1: traversable, 0: obstacle
    LoadMap(mapfile.c_str(), mapData, width, height);

    if(grid2REC){
       grid2rect::convertgrid2rect(mapData, width, height, outputfile+".rec");
    }
    if(grid2CDT){
        grid2poly::convertGrid2Poly(mapData, width, height, outputfile+".poly");
        poly2mesh::convertPoly2Mesh(outputfile+".poly",outputfile+".cdt",width);
    }
    if(grid2MCDT){
        grid2poly::convertGrid2Poly(mapData, width, height, outputfile+".poly");
        poly2mesh::convertPoly2Mesh(outputfile+".poly",outputfile+".cdt",width);
        mesh2merged::convertMesh2MergedMesh(outputfile+".cdt",outputfile+".merged-cdt");
    }


    return 0;
}