#include "host_app.h"

#include <GLFW/glfw3.h>

void Object::translate(float x, float y) {
    m_x = x;
    m_y = y;
}

void Object::colorize(float r, float g, float b) {
    m_r = r;
    m_g = g;
    m_b = b;
}

void Object::set_speed(float speed) { m_rot_speed = speed; }

void Object::draw() {
    m_rot += m_rot_speed;

    glPushMatrix();

    glTranslatef(m_x, m_y, 0);
    glRotatef(m_rot, 0, 0, 1);

    glBegin(GL_QUADS);

    glColor3f(m_r, m_g, m_b);

    glVertex2f(-1, -1);
    glVertex2f(-1, 1);
    glVertex2f(1, 1);
    glVertex2f(1, -1);

    glEnd();

    glPopMatrix();
}

std::vector<Object> g_objects;

std::vector<Object>& getObjects() { return g_objects; }

Object& addObject(float x, float y) {
    g_objects.push_back(Object());
    g_objects.back().translate(x, y);
    return g_objects.back();
}
