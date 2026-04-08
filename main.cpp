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
constexpr vec3 eye{-1,0,2}; // camera position
constexpr vec3 center{0,0,0}; // camera direction
constexpr vec3 up{0,1,0}; // camera up vector

mat<4,4> ModelView, Viewport, Perspective;

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

vec3 rotate(vec3 v, double angle) {
    mat<3,3> Ry = {{{std::cos(angle), 0, std::sin(angle)}, {0,1,0}, {-std::sin(angle), 0, std::cos(angle)}}}; // Rotation matrix around y axis
    return Ry*v;
}

vec3 persp(vec3 v) {
    // We project from a camera at (0, 0, c) where c is the depth from z=0 onto a screen plane at z=0.
    // Note this means any +ve z are in front of the screen (so appear bigger), and any -ve z are behind the screen (so appear smaller)
    // To find x' and y' for this projection, we use the incercept theorem that states the ratios of x' to c == x to (c-z)
    double c = 3.;
    return v*(1/(1-(v.z/c)));
}

// Viewport = scales the scene to the screen size (from 1,1,1 coords to the actual size we want)
void viewport(const int x, const int y, const int w, const int h) {
    Viewport = {{{w/2., 0, 0, x+w/2}, {0, h/2, 0, y+h/2}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
}

// Perspective = perspective deformation via a focal point f instead of orthographic projection (closer things appear bigger, further appear smaller)
void perspective(const double f) {
    Perspective = {{{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0, -1/f,1}}};
}

// ModelView = renders based on the camera and its direction, rather than the world coordinates of the model. Transforms the world coordinates to camera coordinates
void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 n = normalized(eye-center); // Finds the vector from the eye to the center it is pointed at
    vec3 l = normalized(cross(up,n)); // Finds the vector orthoganal to the up direction and the eye-to-center vector
    vec3 m = normalized(cross(n, l)); // Since up may not be perfectly orthoganal, recalculates based on l and n
    ModelView = mat<4,4>{{{l.x,l.y,l.z,0}, {m.x,m.y,m.z,0}, {n.x,n.y,n.z,0}, {0,0,0,1}}} *
                mat<4,4>{{{1,0,0,-center.x}, {0,1,0,-center.y}, {0,0,1,-center.z}, {0,0,0,1}}}; // Matrix that gives the coordinates in screen-space based on the calculated camera vectors when multiplied with world coordinates
}

double signedTriangleArea(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5*((by-ay)*(bx+ax) + (cy-by)*(cx+bx) + (ay-cy)*(ax+cx));
}

void triangle(const vec4 clip[3], TGAImage &framebuffer, vector<double> &zbuffer, TGAColor color) {
    // Get ndc coordinates (unscaled) and screenspace coordinates (scaled)
    // a vec4 per coordinate
    vec4 ndc[3] = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w }; // due to perspective deformation, w encodes the factor we must divide by to get properly projecteds coordinates
    // a vec2 per coordinate - just x and y as z is irrelevant to display on screen
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() };

    // Get bounding box
    // loop thru pixels, check if in triangle, if so fill
    int bbMinX = std::min(std::min(screen[0].x, screen[1].x), screen[2].x);
    int bbMinY = std::min(std::min(screen[0].y, screen[1].y), screen[2].y);
    int bbMaxX = std::max(std::max(screen[0].x, screen[1].x), screen[2].x);
    int bbMaxY = std::max(std::max(screen[0].y, screen[1].y), screen[2].y);

    // float totalArea = signedTriangleArea(ax, ay, bx, by, cx, cy);
    // if (totalArea<1) return;
    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.0}, {screen[1].x, screen[1].y, 1.0}, {screen[2].x, screen[2].y, 1.0} }}; // converts screen to an array to allow for determinant
    if (ABC.det()<1) return; // determinant of ABC is the signed triangle area

    for (int i = bbMinX; i <= bbMaxX; i++) {
        for (int j = bbMinY; j <= bbMaxY; j++) {
            // float alpha = signedTriangleArea(i, j, bx, by, cx, cy) / totalArea;
            // float beta  = signedTriangleArea(i, j, cx, cy, ax, ay) / totalArea;
            // float gamma = signedTriangleArea(i, j, ax, ay, bx, by) / totalArea;
            // if (alpha < 0 || beta < 0 || gamma < 0) {
            //     continue;
            // }

            // double z = (alpha * az + beta * bz + gamma * cz); // computes the depth of that pixel by weighting the depth of points a b and c
            // if (z <= zbuffer[i + j*width]) continue;
            // zbuffer[i + j*width] = z;
            // framebuffer.set(i, j, color); //{alpha*255,beta*255,gamma*255});

            // With matrices for elegance and new model:
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(i), static_cast<double>(j), 1.0}; // barycentric coordinates of {x,y} - ABC.invert_transpose() gives 1/totalArea
            if (bc.x<0 || bc.y<0 || bc.z<0) continue; // negative barycentric coordinate => the pixel is outside the triangle
            
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[i+j*framebuffer.width()]) continue;
            zbuffer[i+j*framebuffer.width()] = z;
            framebuffer.set(i, j, color);
        }
    }
}

void render(Model model, TGAImage& framebuffer, vector<double> &zbuffer) {
#pragma omp parallel for
    for (vec3 f : model.faces) {
        // Transform with perspective and modelview to get the correct camera coordinates and projection of object. Mapping [-1,1] range to full coords is done last and later
        vec4 clip[3];
        for (int d : {0,1,2}) { // for 0,1,2
            vec3 v = model.vertex(f[d]);
            clip[d] = Perspective * ModelView * vec4{v.x, v.y, v.z, 1.0};
        }
        TGAColor rnd;
        for (int c=0; c<3; c++) rnd[c] = std::rand()%255;
        triangle(clip, framebuffer, zbuffer, rnd);
    }
}

void zbufferToImage(vector<double> zbuffer, TGAImage &zbufferImage) {
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

    lookat(eye, center, up); // build the ModelView matrix
    perspective(norm(eye-center)); // build the Perspective matrix
    viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport matrix

    // We need to convert to screenspace then render.
    // We have a model and we have a lookat function. we run through each face in the model, compute lookat for each point, then send to triangle.
    // So rewrite render to go thru faces and lookat etc. then rewrite triangle to take in the vectors instead of ax bx cx etc.

    render(model, framebuffer, zbuffer);
    framebuffer.write_tga_file("framebuffer.tga");

    zbufferToImage(zbuffer, zbufferImage);
    zbufferImage.write_tga_file("zbuffer.tga");

    return 0;
}

