class Vertex {
public:
    Vertex(float x, float y, float z, int width, int height){
        this->x = (x + 1.0f) * (width / 2.0f);
        this->y = (y + 1.0f) * (height / 2.0f);
        this->z = z;
    }

    float x, y, z;
};