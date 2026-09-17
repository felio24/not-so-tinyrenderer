#include <vector>
#include "vec3.h"

class Model {
    std::vector<vec3> verts = {};    // array of vertices
    std::vector<int> facet_vrt = {}; // per-triangle index in the above array (flattened from a vector of vector of vertex)
    std::vector<vec3> norms = {};
    std::vector<int> facet_norm = {};
public:
    Model(const std::string filename);
    int nverts() const; // number of vertices
    int nfaces() const; // number of triangles
    int nnorms() const;
    int nfacenorms() const;
    vec3 vert(const int i) const;                          // 0 <= i < nverts()
    vec3 vert(const int iface, const int nthvert) const;   // 0 <= iface <= nfaces(), 0 <= nthvert < 3
    vec3 norm(const int i) const;
    vec3 norm(const int iface, const int nthnorm) const;
}; 