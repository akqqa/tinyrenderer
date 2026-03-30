#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include "tgaimage.h"
#include "vertex.cpp"
#include "face.cpp"

using namespace std;

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

constexpr int width  = 1000;
constexpr int height = 1000;

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

// void scanlineTriangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color) {
//     if (ay>by) { swap(ax, bx); swap(ay, by); }
//     if (ay>cy) { swap(ax, cx); swap(ay, cy); }
//     if (by>cy) { swap(bx, cx); swap(by, cy); }
//     map<int,int> abCoords = line(ax, ay, bx, by, framebuffer, color);
//     map<int,int> bcCoords = line(bx, by, cx, cy, framebuffer, color);
//     map<int,int> acCoords = line(ax, ay, cx, cy, framebuffer, color);

//     // scan down every y value and make lines using maps to get the x value for each line at that y
//     for (int i = ay; i <= cy; i++) {
//         if (i <= by) {
//             line(acCoords[i], i, abCoords[i], i, framebuffer, color);
//         } else {
//             line(acCoords[i], i, bcCoords[i], i, framebuffer, color);
//         }
//     }
// }

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color) {
    // Get bounding box
    // loop thru pixels, check if in triangle, if so fill
    int bbMinX = std::min(std::min(ax, bx), cx);
    int bbMinY = std::min(std::min(ay, by), cy);
    int bbMaxX = std::max(std::max(ax, bx), cx);
    int bbMaxY = std::max(std::max(ay, by), cy);
    float abcArea = (ax*(by-cy) + bx*(cy-ay) + cx*(ay-by))/2;
    if (abcArea<1) return;

    for (int i = bbMinX; i <= bbMaxX; i++) {
        for (int j = bbMinY; j <= bbMaxY; j++) {
            float abpArea = (ax*(by-j) + bx*(j-ay) + i*(ay-by))/2;
            float apcArea = (ax*(j-cy) + i*(cy-ay) + cx*(ay-j))/2;
            float pbcArea = (i*(by-cy) + bx*(cy-j) + cx*(j-by))/2;
            float alpha = abpArea / abcArea;
            float beta  = apcArea / abcArea;
            float gamma = pbcArea / abcArea;
            if (alpha < 0 || beta < 0 || gamma < 0) {
                continue;
            }
            if (alpha > 0.1 && beta > 0.1 && gamma > 0.1) { // if all barycentric parameters are above 0.1, it must be within the middle, so discard
                continue;
            }
            cout << alpha << endl;
            cout << beta << endl;
            cout << gamma << endl;
            framebuffer.set(i, j, {alpha*255,beta*255,gamma*255});
            
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

// Read in vertices from file into a vector of objects
vector<Vertex> readVertices(string filename) {
    ifstream file(filename);
    string line;
    vector<Vertex> res;

    if (file.is_open()) {
        while (getline(file, line)) {
            if (line.substr(0,2) == "v ") {
                vector<string> lineData = splitString(line, " ");
                res.push_back(Vertex(stof(lineData[1]),stof(lineData[2]),stof(lineData[3])));
            }
        }
        file.close();
    } else {
        cerr << "unable to open file" << endl;
    }

    return res;
}

// Read in faces from file into a vector of objects
vector<Face> readFaces(string filename) {
    ifstream file(filename);
    string line;
    vector<Face> res;
    int count = 0;

    if (file.is_open()) {
        while (getline(file, line)) {
            if (line.substr(0,2) == "f ") {
                vector<string> lineData = splitString(line, " ");
                string f1 = splitString(lineData[1], "/")[0];
                string f2 = splitString(lineData[2], "/")[0];
                string f3 = splitString(lineData[3], "/")[0];
                res.push_back(Face(stoi(f1)-1,stoi(f2)-1,stoi(f3)-1));
            }
        }
        file.close();
    } else {
        cerr << "unable to open file" << endl;
    }

    return res;
}

tuple<int,int> project(Vertex v) {
    return { (v.x + 1.0) *  width/2, (v.y + 1.0) * height/2 };
}

void render(vector<Vertex> vertices, vector<Face> faces, TGAImage& framebuffer) {
#pragma omp parallel for
    for (Face f : faces) {
        auto [ax, ay] = project(vertices[f.v1]);
        auto [bx, by] = project(vertices[f.v2]);
        auto [cx, cy] = project(vertices[f.v3]);
        TGAColor rnd;
        for (int c=0; c<3; c++) rnd[c] = std::rand()%255;
        triangle(ax, ay, bx, by, cx, cy, framebuffer, rnd);
    }
    // for (Vertex v : vertices) {
    //     auto [ax, ay] = project(v);
    //     framebuffer.set(ax, ay, white);
    // }
}

int main(int argc, char** argv) {
    TGAImage framebuffer(width, height, TGAImage::RGB);

    vector<Vertex> vertices = readVertices("../obj/diablo3_pose/diablo3_pose.obj");
    vector<Face> faces = readFaces("../obj/diablo3_pose/diablo3_pose.obj");

    render(vertices, faces, framebuffer);

    framebuffer.write_tga_file("framebuffer.tga");

    // TGAImage framebuffer(width, height, TGAImage::RGB);

    // int ax = 17, ay =  4, az =  255;
    // int bx = 55, by = 39, bz = 128;
    // int cx = 23, cy = 59, cz = 13;

    // triangle(ax, ay, az, bx, by, bz, cx, cy, cz, framebuffer, red);

    // framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

