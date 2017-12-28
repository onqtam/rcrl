#include "host_app.h"

#include <iostream>
#include <GLFW/glfw3.h>

using namespace std;

void print() { cout << "forced print!" << endl; }

void draw() {
    glBegin(GL_TRIANGLES);

    glColor3f(0.5, 0, 0);

    glVertex2f(0.f, 1.f);
    glVertex2f(-1.f, -1.f);
    glVertex2f(1.f, -1.f);

    glEnd();
}
