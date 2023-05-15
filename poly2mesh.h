//
// Created by Bojie Shen on 20/5/2022.
//

#ifndef STARTKIT_POLY2MESH_H
#define STARTKIT_POLY2MESH_H
#include "CDT.h"
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <iomanip>

#define FORMAT_VERSION 2


using namespace std;
namespace poly2mesh {
    struct CustomPoint2D
    {
        CustomPoint2D(double i1, double i2) {
            x = i1;
            y = i2;
        }

        bool operator==(const CustomPoint2D & other) const {
            return (x == other.x) && (y == other.y);
        }

        bool operator!=(const CustomPoint2D& other) const {
            return !(*this == other);
        }

        double x;
        double y;
        int id = -1;
        int tri_id = -1;
        int center_id =-1;
    };

    struct CustomEdge
    {
        CustomEdge(size_t i1, size_t i2) {
            vertices.first = i1;
            vertices.second = i2;
        }
        std::pair<std::size_t, std::size_t> vertices;
    };

    struct CustomPoly
    {
        std::vector<CustomPoint2D> vertices;
        std::vector<CustomEdge> edges;
    };


    double polarAngle(const CustomPoint2D& p, const CustomPoint2D& center) {
        return std::atan2(p.y - center.y, p.x - center.x);
    }

// Define a custom comparison function to compare two points based on their polar angles
    bool compare(const CustomPoint2D& p1, const CustomPoint2D& p2, const CustomPoint2D& center) {
        return polarAngle(p1, center) < polarAngle(p2, center);
    }
    void fail(const string& message)
    {
        cerr << message << endl;
        exit(1);
    }

    vector<CustomPoly> *read_polys(istream& infile)
    {
        vector<CustomPoly> *polygons = new vector<CustomPoly>;
        string header;
        int version;
//    for(int i = 0; i < 35; i ++){
//        infile >> header;
//    }
        if (!(infile >> header))
        {
            fail("Error reading header");
        }
        if (header != "poly")
        {
            cerr << "Got header '" << header << "'" << endl;
            fail("Invalid header (expecting 'poly')");
        }

        if (!(infile >> version))
        {
            fail("Error getting version number");
        }
        if (version != 1)
        {
            cerr << "Got file with version " << version << endl;
            fail("Invalid version (expecting 1)");
        }

        int N;
        if (!(infile >> N))
        {
            fail("Error getting number of polys");
        }
        if (N < 1)
        {
            cerr << "Got " << N << "polys" << endl;
            fail("Invalid number of polys");
        }

        for (int i = 0; i < N; i++)
        {
            int M;
            if (!(infile >> M))
            {
                fail("Error parsing map (can't get number of points of poly)");
            }
            if (M < 3)
            {
                cerr << "Got " << N << "points" << endl;
                fail("Invalid number of points in poly");
            }
            CustomPoly cur_poly;
            for (int j = 0; j < M; j++)
            {
                double x, y;
                if (!(infile >> x >> y))
                {
                    fail("Error parsing map (can't get point)");
                }
//                cur_poly.push_back(Point2(x, y));
                cur_poly.vertices.push_back(CustomPoint2D(x,y));
            }
            for (int i = 1; i < cur_poly.vertices.size(); i++){
                cur_poly.edges.push_back( CustomEdge(i-1,i));
            }
            cur_poly.edges.push_back( CustomEdge(cur_poly.vertices.size()-1,0));
            polygons->push_back(cur_poly);
        }

        int temp;
        if (infile >> temp)
        {
            fail("Error parsing map (read too much)");
        }
        return polygons;
    }


    void convertPoly2Mesh(const std::string input_file,const std::string output_file, int width){

        ifstream fin(input_file);
        vector<CustomPoly>* polygons =  read_polys(fin);
        vector<CustomPoint2D> vertices;
        vector<CustomEdge> edges;
        width = width + 1;
        std::unordered_map<unsigned int, unsigned int> vertex_map;
        for (auto& poly : *polygons){
            for(auto& v : poly.vertices){
                auto it = vertex_map.find(v.y * width + v.x);
                if(it != vertex_map.end()){
                    // found
                    v.id = it->second;
                }else{
                    // not found
                    vertex_map.insert(make_pair(v.y * width + v.x, vertices.size()));
                    v.id = vertices.size();
                    vertices.push_back(v);
                }
            }
        }

        for (auto poly : *polygons) {
            for (auto v: poly.edges) {
                CustomPoint2D v1 = poly.vertices[v.vertices.first];
                CustomPoint2D v2 = poly.vertices[v.vertices.second];
                if(v1.id == -1 || v2.id == -1){
                    cerr<< "edge_id can not match"<< endl;
                }
                edges.push_back(CustomEdge(v1.id,v2.id));
            }
        }


        CDT::Triangulation<double> cdt;
        cdt.insertVertices(
                vertices.begin(),
                vertices.end(),
                [](const CustomPoint2D& p){ return p.x; },
                [](const CustomPoint2D& p){ return p.y; }
        );
        cdt.insertEdges(
                edges.begin(),
                edges.end(),
                [](const CustomEdge& e){ return e.vertices.first; },
                [](const CustomEdge& e){ return e.vertices.second; }
        );
        cdt.eraseOuterTrianglesAndHoles();
        auto triangles = cdt.triangles;
        if(triangles.empty()){
            cerr<<"Error: generating CDT failed "<<endl;
        };

        // output the mesh format;
        vector<vector<int>> vertex2tri_mapper(vertices.size());
        for( int i = 0; i < triangles.size(); i ++ ){
            const auto& tri  = triangles[i];
            for(int j = 0; j < tri.vertices.size(); j++){
                int v_id = tri.vertices[j];
                if(v_id<0 || v_id > vertices.size()-1){
                    cerr<<"Error: vertices index out of range"<<endl;
                }
                vertex2tri_mapper[v_id].push_back(i);
            }
        }

        vector<vector<int>>vertices_index_list = vector<vector<int>>(vertex2tri_mapper.size());
        for(int i = 0; i < vertex2tri_mapper.size(); i ++){
            const auto & tri_list = vertex2tri_mapper[i];
            vector<CustomPoint2D> sort_list;
            for( const auto& tri : tri_list){
                const auto& triangle = triangles[tri];
                int index = 0;
                bool found = false;
                for(const auto& v_id :triangle.vertices){
                    if(v_id == i){
                        found =true;
                        break;
                    }
                    index ++;
                }
                if(!found){
                    cerr<< "Error: vertex not found"<<endl;
                }

                int center_id = index;
                index = index + 2;
                index = index % 3;
                // get represented vertices;
                CustomPoint2D cur = vertices[triangle.vertices[index]];
                cur.tri_id = tri;
                cur.center_id = center_id;
                sort_list.push_back(cur);
            }
            CustomPoint2D center = vertices[i];
            std::sort(sort_list.begin(), sort_list.end(), [&center](const CustomPoint2D& p1, const CustomPoint2D& p2) {
                return compare(p1, p2, center);
            });

            vector<int> index_list ;
            index_list.push_back(sort_list[0].tri_id);
            if(sort_list.size() >= 2) {
                const  auto&  prev_tri = triangles[sort_list[0].tri_id];
                CustomPoint2D pre_v =  vertices[prev_tri.vertices[(sort_list[0].center_id + 2) % 3]];
                for (int j = 1; j < sort_list.size(); j++) {
                    const auto & curr_tri = triangles[sort_list[j].tri_id];
                    const CustomPoint2D & curr_v = vertices[curr_tri.vertices[(sort_list[j].center_id + 1) % 3]];
                    if (curr_v == pre_v) {
                        // continue triangle;
                        index_list.push_back(sort_list[j].tri_id);
                    } else {
                        index_list.push_back(-1);
                        index_list.push_back(sort_list[j].tri_id);
                    }
                    pre_v = vertices[curr_tri.vertices[(sort_list[j].center_id + 2) % 3]];
                }
                //check last tri with first_tri.
                const  auto&  first_tri = triangles[sort_list[0].tri_id];
                CustomPoint2D& first_v =  vertices[first_tri.vertices[(sort_list[0].center_id + 1) %3]];
                if (first_v != pre_v) {
                    index_list.push_back(-1);
                }
            }else{
                index_list.push_back(-1);
            }
            vertices_index_list[i] = index_list;
        }


        ofstream fout(output_file);
        fout << "mesh" << endl;
        fout << FORMAT_VERSION << endl;
        // Assume that all the vertices in the triangulation are interesting.
        fout << vertices.size() << " " << triangles.size() << endl;
        fout << fixed << setprecision(10);
        for (const auto& vertex: vertices) {
            double x, y;
            x = vertex.x;
            y = vertex.y;
            if (x == (int) x) {
                fout << (int) x;
            } else {
                fout << x;
            }
            fout << " ";
            if (y == (int) y) {
                fout << (int) y;
            } else {
                fout << y;
            }
            fout << " " << vertices_index_list[vertex.id].size();

            for (auto index: vertices_index_list[vertex.id]) {
                fout << " " << index;
            }
            fout << endl;
        }
        for (const auto& triangle: triangles) {
            fout << 3;

            for (int i: triangle.vertices) {
                fout << " " << i;
            }
            vector<unsigned int> neighbour = vector<unsigned int> (3);
            neighbour[0] = triangle.neighbors[2];
            neighbour[1] = triangle.neighbors[0];
            neighbour[2] = triangle.neighbors[1];


            for (unsigned int i: neighbour) {
                fout << " ";
                if(i == numeric_limits<unsigned int>::max()){
                    fout << -1;
                }else{
                    fout << i;
                }
            }
            fout << endl;
        }
    }

}

#endif //STARTKIT_POLY2MESH_H
