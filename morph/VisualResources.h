/*!
 * \file
 *
 * Declares a VisualResource class to hold the information about any one-per-program
 * resources.
 *
 * \author Seb James
 * \date November 2020
 */

#pragma once

#ifndef OWNED_MODE
# define GLFW_INCLUDE_NONE
# include <GLFW/glfw3.h>
#endif
#include <iostream>
#include <tuple>
#include <set>
#include <stdexcept>
#include <morph/gl/util.h>

namespace morph {

    //! Singleton resource class for morph::Visual scenes.
    class VisualResources
    {
    private:
        VisualResources() { this->init(); }
        ~VisualResources()
        {
#ifndef OWNED_MODE
            // Shut down GLFW
            glfwTerminate();
#endif
        }

#ifndef OWNED_MODE
        void glfw_init()
        {
            if (!glfwInit()) { std::cerr << "GLFW initialization failed!\n"; }

            // Set up error callback
            glfwSetErrorCallback (morph::VisualResources::errorCallback);

            // See https://www.glfw.org/docs/latest/monitor_guide.html
            GLFWmonitor* primary = glfwGetPrimaryMonitor();
            float xscale, yscale;
            glfwGetMonitorContentScale(primary, &xscale, &yscale);

            glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 1);
#ifdef __OSX__
            glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
            glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
            // Tell glfw that we'd like to do anti-aliasing.
            glfwWindowHint (GLFW_SAMPLES, 4);
        }
#endif
        void init()
        {
#ifndef OWNED_MODE
            // VisualResources simply initialises GLFW
            this->glfw_init();
#endif
        }

        //! An error callback function for the GLFW windowing library
        static void errorCallback (int error, const char* description)
        {
            std::cerr << "Error: " << description << " (code "  << error << ")\n";
        }

    public:
        VisualResources(const VisualResources&) = delete;
        VisualResources& operator=(const VisualResources &) = delete;
        VisualResources(VisualResources &&) = delete;
        VisualResources & operator=(VisualResources &&) = delete;

        //! The instance public function. Uses the very short name 'i' to keep code tidy.
        //! This relies on C++11 magic statics (N2660).
        static auto& i()
        {
            static VisualResources instance;
            return instance;
        }

        //! A function to call to simply make sure the singleton instance exists
        void create() {}
    };

} // namespace morph
