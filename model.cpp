#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include "model.h"

Model::Model(const std::string filename) {
    std::ifstream in(filename);
    std::string s;
    while (std::getline(in, s)) {
        std::stringstream ss(s);

        std::string type;
        ss >> type;
        vec3 v;
        if (type == "v") {
            std::string x_str;
            ss >> x_str;

            std::string y_str;
            ss >> y_str;

            std::string z_str;
            ss >> z_str;

            verts.emplace_back(vec3(std::stod(x_str), std::stod(y_str), std::stod(z_str)));
        } else if (type == "f") {
            std::string face;
            for (int i = 0; i < 3; i++) {
                ss >> face;
                std::string vert;
                std::stringstream fs(face);
                std::getline(fs, vert, '/');
                facet_vrt.emplace_back(std::stoi(vert) - 1);
            }
        }
    }
}

int Model::nverts() const {
    return verts.size();
}

int Model::nfaces() const {
    return facet_vrt.size() / 3;
}

vec3 Model::vert(const int i) const {
    assert(nverts());
    return verts[i];
}

vec3 Model::vert(const int iface, const int nthvert) const {
    assert(iface < nfaces() && nthvert < 3);
    return verts[facet_vrt[iface * 3 + nthvert]];
}
