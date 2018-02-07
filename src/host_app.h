#pragma once

#ifdef _WIN32
#define SYMBOL_EXPORT __declspec(dllexport)
#define SYMBOL_IMPORT __declspec(dllimport)
#else
#define SYMBOL_EXPORT __attribute__((visibility("default")))
#define SYMBOL_IMPORT
#endif

#ifdef HOST_APP
#define HOST_API SYMBOL_EXPORT
#else
#define HOST_API SYMBOL_IMPORT
#endif

// can also use WINDOWS_EXPORT_ALL_SYMBOLS in CMake for Windows
// instead of explicitly annotating each symbol in the host app

#include <vector>

class HOST_API Object
{
	friend HOST_API Object& addObject(float x, float y);

    float m_x = 0, m_y = 0;
    float m_r = 0.3f, m_g = 0.3f, m_b = 0.3f;

    float m_rot       = 0;
    float m_rot_speed = 1.f;

    Object() = default;

public:
    void translate(float x, float y);
    void colorize(float r, float g, float b);
    void set_speed(float speed);

    void draw();
};

HOST_API std::vector<Object>& getObjects();
HOST_API Object& addObject(float x, float y);
