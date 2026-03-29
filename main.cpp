#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
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

vector<tuple<int,int>> line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    vector<tuple<int,int>> lineCoords;
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
            lineCoords.push_back(make_tuple(y,x));
        } else {
            framebuffer.set(x, y, color);
            lineCoords.push_back(make_tuple(x,y));
        }
    }

    return lineCoords;
}

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color) {
    if (ay>by) { swap(ax, bx); swap(ay, by); }
    if (ay>cy) { swap(ax, cx); swap(ay, cy); }
    if (by>cy) { swap(bx, cx); swap(by, cy); }
    vector<tuple<int,int>> abCoords = line(ax, ay, bx, by, framebuffer, color);
    vector<tuple<int,int>> bcCoords = line(bx, by, cx, cy, framebuffer, color);
    vector<tuple<int,int>> acCoords = line(ax, ay, cx, cy, framebuffer, color);

    // Figure out triangle filling
    // WE HAVE OTDER SO A IS HIGHEST AND C IS LOWEST
    // draw lines between each segment of acCoords and aBCoords, then acCoords and bcCoords
    // NOT CORRECT AS AC != AB + BC HEIGHT WISE, NEED TO SOMEHOW SCAN DOWN THE Y OF AC AND MATCH WITH CORRECT PLACES IN AB AND BC
    for (size_t i = 0; i < acCoords.size(); i++) {
        line(get<0>(acCoords[i]), get<1>(acCoords[i]), get<0>(abCoords[i]), get<1>(abCoords[i]), framebuffer, color);
    }
    for (size_t i = 0; i < bcCoords.size(); i++) {
        line(get<0>(bcCoords[i]), get<1>(bcCoords[i]), get<0>(abCoords[i+acCoords.size()]), get<1>(abCoords[i+acCoords.size()]), framebuffer, color);
    }

    cout << "ac: " << acCoords.size() << endl;
    cout << "ab+bc: " << abCoords.size() + bcCoords.size() << endl;
    cout << "done" << endl;

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
                cout << "vertex" << endl;
                vector<string> lineData = splitString(line, " ");
                cout << lineData[1] << " " << lineData[2] << " " << lineData[3] << endl;
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
                cout << ++count << endl;
                cout << "face" << endl;
                vector<string> lineData = splitString(line, " ");
                cout << lineData[1] << endl;
                string f1 = splitString(lineData[1], "/")[0];
                string f2 = splitString(lineData[2], "/")[0];
                string f3 = splitString(lineData[3], "/")[0];
                cout << f1 << " " << f2 << " " << f3 << endl;
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
    for (Face f : faces) {
        auto [ax, ay] = project(vertices[f.v1]);
        auto [bx, by] = project(vertices[f.v2]);
        auto [cx, cy] = project(vertices[f.v3]);
        triangle(ax, ay, bx, by, cx, cy, framebuffer, red);
    }
    for (Vertex v : vertices) {
        auto [ax, ay] = project(v);
        framebuffer.set(ax, ay, white);
    }
}

int main(int argc, char** argv) {
    // TGAImage framebuffer(width, height, TGAImage::RGB);

    // vector<Vertex> vertices = readVertices("../obj/african_head/african_head.obj");
    // vector<Face> faces = readFaces("../obj/african_head/african_head.obj");

    // render(vertices, faces, framebuffer);

    // framebuffer.write_tga_file("framebuffer.tga");

    TGAImage framebuffer(width, height, TGAImage::RGB);
    triangle(  7, 45, 35, 100, 45,  60, framebuffer, red);
    triangle(120, 35, 90,   5, 45, 110, framebuffer, white);
    triangle(115, 83, 80,  90, 85, 120, framebuffer, green);
    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

