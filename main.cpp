#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "tgaimage.h"
#include "geometry.h"
#include "model.cpp"

using namespace std;

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

constexpr int width  = 2000;
constexpr int height = 2000;

// Returns a map of y: x coordinates that make up the line
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    // own attempt at steepness resolving
    bool transposed = false;
    if (abs(ay - by) > abs(ax - bx)) { // if more y changes than x
        swap(ax, ay);
        swap(bx,by);
        transposed = true;
    }
    if (ax>bx) { // make it left−to−right
        swap(ax, bx);
        swap(ay, by);
    }
    for (int x=ax; x<=bx; x++) {
        float t = (bx == ax) ? 0 : (x-ax) / static_cast<float>(bx-ax);
        int y = round( ay + (by-ay)*t );
        if (transposed) {
            framebuffer.set(y, x, color);
        } else {
            framebuffer.set(x, y, color);
        }
    }
}

vector<string> splitString(string s, string delimiter) {
    // https://stackoverflow.com/a/46931770
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

Model readModel(string filename) {
    ifstream file(filename);
    string line;
    vector<vec3> vertices;
    vector<vec3> faces;

    if (file.is_open()) {
        while (getline(file, line)) {
            if (line.substr(0,2) == "v ") {
                vector<string> lineData = splitString(line, " ");
                vertices.push_back(vec3(stof(lineData[1]),stof(lineData[2]),stof(lineData[3])));
            } else if (line.substr(0,2) == "f ") {
                vector<string> lineData = splitString(line, " ");
                string f1 = splitString(lineData[1], "/")[0];
                string f2 = splitString(lineData[2], "/")[0];
                string f3 = splitString(lineData[3], "/")[0];
                faces.push_back(vec3(stoi(f1)-1,stoi(f2)-1,stoi(f3)-1));
            }
        }
        file.close();
    } else {
        cerr << "unable to open file" << endl;
    }

    return Model(vertices, faces);
}

std::tuple<int,int,double> project(vec3 v) {
    return { (v.x + 1.0) *  width/2, (v.y + 1.0) * height/2, v.z };
}

vec3 rotate(vec3 v) {
    constexpr double angle = M_PI/6; // Angle to rotate
    constexpr mat<3,3> Ry = {{{std::cos(angle), 0, std::sin(angle)}, {0,1,0}, {-std::sin(angle), 0, std::cos(angle)}}}; // Rotation matrix around y axis
    return Ry*v;
}

vec3 perspective(vec3 v) {
    // We project from a camera at (0, 0, c) where c is the depth from z=0 onto a screen plane at z=0.
    // Note this means any +ve z are in front of the screen (so appear bigger), and any -ve z are behind the screen (so appear smaller)
    // To find x' and y' for this projection, we use the incercept theorem that states the ratios of x' to c == x to (c-z)
    double c = 3.;
    return v*(1/(1-(v.z/c)));
}


double signedTriangleArea(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5*((by-ay)*(bx+ax) + (cy-by)*(cx+bx) + (ay-cy)*(ax+cx));
}

void triangle(int ax, int ay, double az, int bx, int by, double bz, int cx, int cy, double cz, TGAImage &framebuffer, vector<double> &zbuffer, TGAColor color) {
    // Get bounding box
    // loop thru pixels, check if in triangle, if so fill
    int bbMinX = std::min(std::min(ax, bx), cx);
    int bbMinY = std::min(std::min(ay, by), cy);
    int bbMaxX = std::max(std::max(ax, bx), cx);
    int bbMaxY = std::max(std::max(ay, by), cy);
    float totalArea = signedTriangleArea(ax, ay, bx, by, cx, cy);
    if (totalArea<1) return;

    for (int i = bbMinX; i <= bbMaxX; i++) {
        for (int j = bbMinY; j <= bbMaxY; j++) {
            // My area calculations were flawed and caused some artifacting. still unsure the exact cause
            // float abpArea = (ax*(by-j) + bx*(j-ay) + i*(ay-by))/2;
            // float apcArea = (ax*(j-cy) + i*(cy-ay) + cx*(ay-j))/2;
            // float pbcArea = (i*(by-cy) + bx*(cy-j) + cx*(j-by))/2;
            float alpha = signedTriangleArea(i, j, bx, by, cx, cy) / totalArea;
            float beta  = signedTriangleArea(i, j, cx, cy, ax, ay) / totalArea;
            float gamma = signedTriangleArea(i, j, ax, ay, bx, by) / totalArea;
            if (alpha < 0 || beta < 0 || gamma < 0) {
                continue;
            }

            double z = (alpha * az + beta * bz + gamma * cz); // computes the depth of that pixel by weighting the depth of points a b and c
            if (z <= zbuffer[i + j*width]) continue;
            zbuffer[i + j*width] = z;
            framebuffer.set(i, j, color); //{alpha*255,beta*255,gamma*255});
        }
    }
}

void render(Model model, TGAImage& framebuffer, vector<double> &zbuffer) {
#pragma omp parallel for
    for (vec3 f : model.faces) {
        auto [ax, ay, az] = project(perspective(rotate(model.vertex(f[0]))));
        auto [bx, by, bz] = project(perspective(rotate(model.vertex(f[1]))));
        auto [cx, cy, cz] = project(perspective(rotate(model.vertex(f[2]))));
        TGAColor rnd;
        for (int c=0; c<3; c++) rnd[c] = std::rand()%255;
        triangle(ax, ay, az, bx, by, bz, cx, cy, cz, framebuffer, zbuffer, rnd);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " path/to/model.obj" << endl;
        return 1;
    }

    TGAImage framebuffer(width, height, TGAImage::RGB);
    vector<double> zbuffer(width*height, -std::numeric_limits<double>::max());
    TGAImage zbufferImage(width, height, TGAImage::GRAYSCALE);

    Model model = readModel(argv[1]);

    render(model, framebuffer, zbuffer);

    framebuffer.write_tga_file("framebuffer.tga");

    double minZ = std::numeric_limits<double>::max();
    double maxZ = -std::numeric_limits<double>::max();

    for (double z : zbuffer) {
        if (z == -std::numeric_limits<double>::max()) continue; // skip untouched pixels
        minZ = std::min(minZ, z);
        maxZ = std::max(maxZ, z);
    }
    for (int x=0; x<width; x++) {
        for (int y=0; y<height; y++) {
            double z = zbuffer[x + y*width];

            if (z == -std::numeric_limits<double>::max()) {
                zbufferImage.set(x, y, TGAColor{0,0,0,255});
                continue;
            }

            double normalized = (z - minZ) / (maxZ - minZ); // 0 -> 1
            int value = static_cast<int>(normalized * 255);

            zbufferImage.set(x, y, TGAColor{value, value, value, 255});
        }
    }

    zbufferImage.write_tga_file("zbuffer.tga");

    return 0;
}

