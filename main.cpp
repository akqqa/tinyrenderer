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
                res.push_back(Vertex(stof(lineData[1]),stof(lineData[2]),stof(lineData[3]), width, height));
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

void render(vector<Vertex> vertices, vector<Face> faces, TGAImage& framebuffer) {
    for (Face face : faces) {
        Vertex v1 = vertices[face.v1];
        Vertex v2 = vertices[face.v2];
        Vertex v3 = vertices[face.v3];
        line(v1.x, v1.y, v2.x, v2.y, framebuffer, red);
        line(v1.x, v1.y, v3.x, v3.y, framebuffer, red);
        line(v2.x, v2.y, v3.x, v3.y, framebuffer, red);
    }

    framebuffer.write_tga_file("framebuffer.tga");
}

int main(int argc, char** argv) {
    TGAImage framebuffer(width, height, TGAImage::RGB);

    vector<Vertex> vertices = readVertices("../obj/diablo3_pose/diablo3_pose.obj");
    vector<Face> faces = readFaces("../obj/diablo3_pose/diablo3_pose.obj");

    render(vertices, faces, framebuffer);

    // int ax =  7, ay =  3;
    // int bx = 12, by = 37;
    // int cx = 62, cy = 53;

    // line(ax, ay, bx, by, framebuffer, blue);
    // line(cx, cy, bx, by, framebuffer, green);
    // line(cx, cy, ax, ay, framebuffer, yellow);
    // line(ax, ay, cx, cy, framebuffer, red);

    // framebuffer.set(ax, ay, white);
    // framebuffer.set(bx, by, white);
    // framebuffer.set(cx, cy, white);

    // framebuffer.write_tga_file("framebuffer.tga");
    // return 0;
}

