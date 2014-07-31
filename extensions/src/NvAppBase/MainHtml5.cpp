//----------------------------------------------------------------------------------
// File:        NvAppBase/MainHtml5.cpp
// SDK Version: v1.2 
// Email:       gameworks@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------------

#ifdef LINUX

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <time.h>

#include <GL/glew.h>
#include <GL/glfw.h>
//#include <GL/glx.h>

#include <emscripten/emscripten.h>

#include "NvAppBase/NvAppBase.h"
#include "NV/NvStopWatch.h"
#include "NvAssetLoader/NvAssetLoader.h"

class NvHtml5StopWatch: public NvStopWatch
{
public:
    //! Constructor, default
    NvHtml5StopWatch() :
        start_time(), diff_time( 0.0)
    { };

    // Destructor
    ~NvHtml5StopWatch()
    { };

public:
    //! Start time measurement
    void start() {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        m_running = true;
    }

    //! Stop time measurement
    void stop() {
        diff_time = getDiffTime();
        m_running = false;
    }

    //! Reset time counters to zero
    void reset()
    {
        diff_time = 0;
        if( m_running )
            start();
    }

    const float getTime() const {
        if(m_running) {
            return getDiffTime();
        } else {
            // time difference in milli-seconds
            return  diff_time;
        }
    }

private:

    // helper functions
      
    //! Get difference between start time and current time
    float getDiffTime() const {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return  (float) (( now.tv_sec - start_time.tv_sec)
                    + (0.000000001 * (now.tv_nsec - start_time.tv_nsec)) );
    }

    // member variables

    //! Start of measurement
    struct timespec  start_time;

    //! Time difference between the last start and stop
    float  diff_time;
};


static NvAppBase *sApp = NULL;

// this needs to be global so inputcallbacksglfw can access...
NvInputCallbacks* sCallbacks = NULL;
extern void setInputCallbacksGLFW();

static bool sWindowIsFocused = true;
static bool sHasResized = true;
static int32_t sForcedRenderCount = 0;

class NvGLHtml5AppContext: public NvGLAppContext {
public:
    NvGLHtml5AppContext(NvEGLConfiguration& config) :
        NvGLAppContext(NvGLPlatformInfo(
            NvGLPlatformCategory::PLAT_MOBILE, 
            NvGLPlatformOS::OS_LINUX))
    {
        // Hack - we can't query most of this back from GLFW, so we assume it all "took"
        mConfig = config;

        //glfwWindowHint(GLFW_RED_BITS, config.redBits);
        //glfwWindowHint(GLFW_GREEN_BITS, config.greenBits);
        //glfwWindowHint(GLFW_BLUE_BITS, config.blueBits);
        //glfwWindowHint(GLFW_ALPHA_BITS, config.alphaBits);
        //glfwWindowHint(GLFW_DEPTH_BITS, config.depthBits);
        //glfwWindowHint(GLFW_STENCIL_BITS, config.stencilBits);
    }

    bool bindContext() {
        //glfwMakeContextCurrent(mWindow);
        return true;
    }

    bool unbindContext() {
        //glfwMakeContextCurrent(NULL);
        return true;
    }

    bool swap() {
        glfwSwapBuffers();
        return true;
    }

    bool setSwapInterval(int32_t interval) {
        glfwSwapInterval(interval);
        return true;
    }

    int32_t width() {
        int32_t w, h;
        glfwGetWindowSize(&w, &h);
        return w;
    }

    int32_t height() {
        int32_t w, h;
        glfwGetWindowSize(&w, &h);
        return h;
    }

    GLproc getGLProcAddress(const char* procname) {
        return (GLproc)glfwGetProcAddress(procname);
    }

    bool isExtensionSupported(const char* ext) {
        return glfwExtensionSupported(ext) ? true : false;
    }

    void setConfiguration(const NvEGLConfiguration& config) { mConfig = config; }

    virtual void* getCurrentPlatformContext() { 
        return NULL;
        //return (void*)glXGetCurrentContext(); 
    }

    virtual void* getCurrentPlatformDisplay() { 
        return NULL;
        //return (void*)glXGetCurrentDisplay(); 
    }

protected:
};

class NvHtml5PlatformContext : public NvPlatformContext {
public:
    NvHtml5PlatformContext() {}
    ~NvHtml5PlatformContext() {}

    virtual bool isAppRunning();
    virtual void requestExit() { }//glfwSetWindowShouldClose(mWindow, 1); }
    virtual bool pollEvents(NvInputCallbacks* callbacks);
    virtual bool isContextLost() { return false; }
    virtual bool isContextBound() { return true; }//glfwGetCurrentContext() != NULL; }
    virtual bool shouldRender();
    virtual bool hasWindowResized();
    virtual NvGamepad* getGamepad() { return NULL; }
    virtual void setAppTitle(const char* title) { glfwSetWindowTitle(title); }
    virtual const std::vector<std::string>& getCommandLine() { return m_commandLine; }

    std::vector<std::string> m_commandLine;
protected:
};

bool NvHtml5PlatformContext::isAppRunning() {
    return true;
}

bool NvHtml5PlatformContext::pollEvents(NvInputCallbacks* callbacks) {
    sCallbacks = callbacks;
    glfwPollEvents();
    sCallbacks = NULL;
    return true;
}

bool NvHtml5PlatformContext::shouldRender() {
    if (sWindowIsFocused || (sForcedRenderCount > 0)) {
        if (sForcedRenderCount > 0)
            sForcedRenderCount--;

        return true;
    }
    return false;
}

bool NvHtml5PlatformContext::hasWindowResized() {
    if (sHasResized) {
        sHasResized = false;
        return true;
    }
    return false;
}

static void reshape(int32_t width, int32_t height )
{
    glViewport(0, 0, width, height);
    sHasResized = true;
    sForcedRenderCount += 2;
}

static void focus(int32_t focused)
{
    sWindowIsFocused = (focused != 0);
    sApp->focusChanged(sWindowIsFocused);
    sForcedRenderCount += 2;
}

void glfwError(int,const char* err) {
    fprintf( stderr, "GLFW error = %s\n", err);
}

int32_t main(int32_t argc, char *argv[])
{
    int32_t width, height;
    sWindowIsFocused = true;
    sForcedRenderCount = 0;

    NvAssetLoaderInit(NULL);
    if (glfwInit() != GL_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(EXIT_FAILURE);
    }

    //glfwSetErrorCallback(glfwError);
    NvHtml5PlatformContext* platform = new NvHtml5PlatformContext;
    for (int i = 1; i < argc; i++) {
        platform->m_commandLine.push_back(argv[i]);
    }

    sApp = NvAppFactory(platform);
    NvEGLConfiguration config(NvGfxAPIVersionGL4(), 8,8,8,8, 24,0);
    sApp->configurationCallback(config);

    NvGLHtml5AppContext* context = new NvGLHtml5AppContext(config);
    if (glfwOpenWindow(1280, 720, 8,8,8,0, 24,0, GLFW_WINDOW) != GL_TRUE) {
        fprintf(stderr, "Failed to open GLFW window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    //context->setWindow(window);
    sApp->setGLContext(context);
    glfwSetWindowSizeCallback(reshape);
    //glfwSetWindowFocusCallback(window, focus);
    setInputCallbacksGLFW();
    context->bindContext();
    glfwSwapInterval(1);
    glfwGetWindowSize(&width, &height);

    //int32_t major = glfwGetWindowParam(GLFW_OPENGL_VERSION_MAJOR);
    //int32_t minor = glfwGetWindowParam(GLFW_OPENGL_VERSION_MINOR);
    config.apiVer = NvGfxAPIVersionES2(); //NvGfxAPIVersion(NvGfxAPI::GLES, major, minor);
    glGetIntegerv(GL_RED_BITS, (GLint*)&config.redBits);
    glGetIntegerv(GL_GREEN_BITS, (GLint*)&config.greenBits);
    glGetIntegerv(GL_BLUE_BITS, (GLint*)&config.blueBits);
    glGetIntegerv(GL_ALPHA_BITS, (GLint*)&config.alphaBits);
    glGetIntegerv(GL_DEPTH_BITS, (GLint*)&config.depthBits);
    glGetIntegerv(GL_STENCIL_BITS, (GLint*)&config.stencilBits);
    context->setConfiguration(config);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        exit(-1);
    }
    fprintf(stdout, "Using GLEW %s\n", glewGetString(GLEW_VERSION));

    reshape(width, height);
    sApp->mainLoop();
    delete sApp;
    glfwTerminate();
    NvAssetLoaderShutdown();
    exit(EXIT_SUCCESS);
}

NvStopWatch* NvAppBase::createStopWatch() {
    return new NvHtml5StopWatch;
}

bool NvAppBase::showDialog(const char*, const char *, bool exitApp) {
    return false;
}

bool NvAppBase::writeScreenShot(int32_t, int32_t, const uint8_t*, const std::string&) {
    return false;
}

bool NvAppBase::writeLogFile(const std::string&, bool, const char*, ...) {
    return false;
}

void NvAppBase::forceLinkHack() {
}

#endif
