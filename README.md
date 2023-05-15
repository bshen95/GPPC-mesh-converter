# GPPC-mesh-converter
A quick fix for the mesh converter that converts GPPC grid maps into three types of meshes: [Rectangle Mesh], [CDT Mesh], and [Merged Mesh]. 
This fix builds upon Michael's previous implementation of [Polyanya](https://bitbucket.org/mlcui1/polyanya/src/master/utils/). 
However, we have addressed a bug caused by the fade2D library by replacing it with the more reliable [CDT library](https://github.com/artem-ogre/CDT).

The updated implementation ensures a more robust and accurate conversion process, 
allowing for seamless generation of the desired mesh types for [GPPC competition](https://gppc.search-conference.org).
If you are using our code, please carefully cite [1] - [2].
## Requirements
The implementation requires the external libraries: [CMake](https://cmake.org) 
If you are using Ubuntu, you can install them simply by:
```shell script
sudo apt-get install cmake
``` 
If you are using Mac, you can install them simply by:
```shell script
brew install cmake
```
If the above methods do not work, you can also follow the instructions
on the [CMake](https://cmake.org) and install it manually. No other dependency needed.



## Compiling and Running
The current implementation is compiled with CMake, you can compile it from the directory of the source code by:
```shell script
make fast
```
To run the mesh converter, you can simply do:
```shell script
./cbs -mcdt data/AcrosstheCape.map
```
This command simply converts the grid map "data/AcrosstheCape.map" into the merged CDT mesh, the output will be written 
into the same directory "data/AcrosstheCape.merged-cdt". Alternatively, 
you can swap the flag "-mcdt" to produce the other types of 
meshes:
- -rec: convert grid map to rectangle mesh. "data/AcrosstheCape.map" -> "data/AcrosstheCape.rec"
- -cdt: convert grid map to CDT mesh. "data/AcrosstheCape.map" -> "data/AcrosstheCape.cdt"
- -mcdt: convert grid map to merged CDT mesh. "data/AcrosstheCape.map" -> "data/AcrosstheCape.merged-cdt"

## Mesh file format


A summary of the format is shown below. For more detail, 
please refer to [Michael's repository]((https://bitbucket.org/mlcui1/polyanya/src/master/utils/). ).
```
mesh
2
[V: int = # of vertices in mesh] [P: int = # of polygons in mesh]
(for each vertex)
    [x: float] [y: float]
    [n: int = number of vertices this vertex is connected to]
    [p: int[n] = array of 0-indexed indices of the mesh's polygons connected to
                 this vertex in counterclockwise order. -1 if obstacle]
(for each polygon)
    [n: int = # of vertices in polygon]
    [v: int[n] = array of 0-indexed indices of the mesh's vertices defining
                 this polygon in counterclockwise order]
    [p: int[n] = array of 0-indexed indices of the mesh's polygons adjacent to
                 this polygon in counterclockwise order, such that the p[1]
                 shares the vertices v[0] and v[1]. -1 if obstacle]
```

The format is whitespace insensitive - tokens can be separated by any kind of
whitespace.
## Reference

[1] B. Shen, M. A. Cheema, D. Harabor, P. J. Stuckey,
Euclidean pathfinding with compressed path databases,
in: Proceedings of the Twenty-Ninth International Joint
Conference on Artificial Intelligence, IJCAI 2020, 2020,
pp. 4229–4235.

[2] M. Cui, D. D. Harabor, A. Grastien, Compromise-free pathfinding
on a navigation mesh, in: Proceedings of the Twenty-Sixth International
Joint Conference on Articial Intelligence, IJCAI 2017, 2017,
pp. 496-502.