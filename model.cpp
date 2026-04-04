#include <vector>
#include "geometry.h"
using namespace std;

class Model {
public:
    vector<vec3> vertices;
    vector<vec3> faces;

    Model(vector<vec3> vertices, vector<vec3> faces) {
        this->vertices = vertices;
        this->faces = faces;
    }

    int nverts() { return vertices.size(); }
    int nfaces() { return faces.size(); }

    vec3 vertex(int i) { return vertices[i]; }
    vec3 face(int i) { return faces[i]; }
};