#pragma once

// Add some text as a VisualModel

#ifndef USE_GLEW
#ifdef __OSX__
# include <OpenGL/gl3.h>
#else
# include <GL3/gl3.h>
#endif
#endif
#include <morph/vec.h>
#include <morph/tools.h>
#include <morph/VisualTextModel.h>
#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <stdexcept>

namespace morph {

    class TxtVisual : public VisualModel
    {
    public:
        TxtVisual (const std::string& _text,
                   const morph::vec<float, 3>& _offset,
                   const morph::TextFeatures& _tfeatures)
        {
            // Set up...
            this->mv_offset = _offset;
            this->viewmatrix.translate (this->mv_offset);

            this->text = _text;
            this->tfeatures = _tfeatures;
        }

        void initializeVertices()
        {
            // No op, but add text
            this->addLabel (this->text, morph::vec<float>({0,0,0}), this->tfeatures);
        }

        std::string text;
        morph::TextFeatures tfeatures;
    };

} // namespace morph
