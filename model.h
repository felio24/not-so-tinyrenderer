#include <vector>
#include "tgaimage.h"
#include "vec3.h"

class Model {
    std::vector<vec3> verts = {};    // array of vertices
    std::vector<int> facet_vrt = {}; // per-triangle index in the above array (flattened from a vector of vector of vertex)
    
    std::vector<vec3> norms = {};
    std::vector<int> facet_norm = {};

    std::vector<vec2> texcoords = {};
    std::vector<int> facet_texcoord = {};

    TGAImage normalmap;
    bool normalmap_loaded = false;

    TGAImage diffusemap;
    bool diffusemap_loaded = false;

    TGAImage specularmap;
    bool specularmap_loaded = false;
public:
    Model(const std::string filename);
    
    int nverts() const; // number of vertices
    int nfaces() const; // number of triangles
    int nnorms() const;
    int nfacenorms() const;
    int ntexcoords() const;
    int nfacetexcoords() const;
    
    vec3 vert(const int i) const;                          // 0 <= i < nverts()
    vec3 vert(const int iface, const int nthvert) const;   // 0 <= iface <= nfaces(), 0 <= nthvert < 3
    vec3 norm(const int i) const;
    vec3 norm(const int iface, const int nthnorm) const;
    vec2 uv(int iface, int nthvert) const;

    bool has_normalmap() const;
    TGAColor normalmap_sample(const vec2& uv) const;
    bool has_diffusemap() const;
    TGAColor diffusemap_sample(const vec2& uv) const;
    bool has_specularmap() const;
    TGAColor specularmap_sample(const vec2& uv) const;
}; 