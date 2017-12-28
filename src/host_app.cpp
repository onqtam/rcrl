#include "host_app.h"

#include <iostream>
#include <GLFW/glfw3.h>

using namespace std;

void print() { cout << "forced print!" << endl; }

void draw() {
    glBegin(GL_TRIANGLES);

    glColor3f(1, 0, 0);

    glVertex2f(-1, -1);
    glVertex2f(0, 1);
    glVertex2f(1, -1);

    glEnd();
}
