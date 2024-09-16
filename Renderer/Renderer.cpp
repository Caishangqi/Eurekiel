#include "Renderer.hpp"

#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
//  在包含 windows.h 和 gl/gl.h 时，顺序非常重要。需要先包含 windows.h，然后再包含 gl/gl.h。
#include <windows.h>			// #include this (massive, platform-specific) header in VERY few places (and .CPPs only)
#include <gl/gl.h>
#pragma comment( lib, "opengl32" )	// Link in the OpenGL32.lib static library
extern HDC g_displayDeviceContext;
extern HWND g_hWnd;

//------------------------------------------------------------------------------------------------
// Given an existing OS Window, create a Rendering Context (RC) for OpenGL or DirectX to draw to it.
// #SD1ToDo: Move this to become Renderer::CreateRenderingContext() in Engine/Renderer/Renderer.cpp
// #SD1ToDo: By the end of SD1-A1, this function will be called from the function Renderer::Startup
//
void Renderer::StartUp()
{
    CreateRenderingContext(); // #SD1ToDo: this will move to Renderer.cpp, called by Renderer::Startup
}

void Renderer::BeingFrame()
{
}

void Renderer::EndFrame()
{
    // "Present" the backbuffer by swapping the front (visible) and back (working) screen buffers
    SwapBuffers(g_displayDeviceContext); // Note: call this only once at the very end of each frame
}

void Renderer::Shutdown()
{
}

void Renderer::ClearScreen(const Rgba8& clearColor)
{
    glClearColor(clearColor.r / 255.f, clearColor.g / 255.f, clearColor.b / 255.f, clearColor.a);
    // Note; glClearColor takes colors as floats in [0,1], not bytes in [0,255]
    glClear(GL_COLOR_BUFFER_BIT); // ALWAYS clear the screen at the top of each frame's Render()!
}

void Renderer::BeingCamera(const Camera& camera)
{
    Vec2 bottom_left = camera.GetOrthoBottomLeft();
    Vec2 top_right = camera.GetOrthoTopRight();
    glLoadIdentity();
    glOrtho(bottom_left.x, top_right.x, bottom_left.y, top_right.y, 0.f, 1.f);
}

void Renderer::EndCamera(const Camera& camera)
{
}

void Renderer::DrawVertexArray(int numVertexes, const Vertex_PCU* vertexes)
{
    glBegin(GL_TRIANGLES);
    {
        for (int i = 0; i < numVertexes; i++)
        {
            glColor4ub(vertexes[i].m_color.r, vertexes[i].m_color.g, vertexes[i].m_color.b, vertexes[i].m_color.a);
            glTexCoord2f(vertexes[i].m_uvTextCoords.x, vertexes[i].m_uvTextCoords.y);
            glVertex2f(vertexes[i].m_position.x, vertexes[i].m_position.y);
        }
    }
    glEnd();
}

void Renderer::CreateRenderingContext()
{
    // Creates an OpenGL rendering context (RC) and binds it to the current window's device context (DC)
    PIXELFORMATDESCRIPTOR pixelFormatDescriptor;
    memset(&pixelFormatDescriptor, 0, sizeof(pixelFormatDescriptor));
    pixelFormatDescriptor.nSize = sizeof(pixelFormatDescriptor);
    pixelFormatDescriptor.nVersion = 1;
    pixelFormatDescriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormatDescriptor.iPixelType = PFD_TYPE_RGBA;
    pixelFormatDescriptor.cColorBits = 24;
    pixelFormatDescriptor.cDepthBits = 24;
    pixelFormatDescriptor.cAccumBits = 0;
    pixelFormatDescriptor.cStencilBits = 8;

    // These two OpenGL-like functions (wglCreateContext and wglMakeCurrent) will remain here for now.
    int pixelFormatCode = ChoosePixelFormat(g_displayDeviceContext, &pixelFormatDescriptor);
    SetPixelFormat(g_displayDeviceContext, pixelFormatCode, &pixelFormatDescriptor);
    m_apiRenderingContext = wglCreateContext(g_displayDeviceContext);
    wglMakeCurrent(g_displayDeviceContext, static_cast<HGLRC>(m_apiRenderingContext));

    // #SD1ToDo: move all OpenGL functions (including those below) to Renderer.cpp (only!)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
