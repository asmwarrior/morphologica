/*!
 * \file
 *
 * Declares a VisualModel class to hold the vertices that make up some individual
 * model object that can be part of an OpenGL scene.
 *
 * \author Seb James
 * \date May 2019
 */

#pragma once

#ifndef USE_GLEW
# ifdef __OSX__
#  include <OpenGL/gl3.h>
# else
#  include <GL3/gl3.h>
#  include <GL/glext.h>
# endif
#endif

#include <morph/TransformMatrix.h>
#include <morph/vec.h>
#include <morph/mathconst.h>
#include <morph/gl/util.h>
#include <morph/VisualCommon.h>
#include <morph/VisualTextModel.h>
#include <morph/VisualFace.h>
#include <morph/colour.h>
#include <morph/base64.h>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <iterator>
#include <string>
#include <memory>
#include <functional>

// Switches on some changes where I carefully unbind gl buffers after calling
// glBufferData() and rebind when changing the vertex model. Makes no difference on my
// Macbook Air, but should be more correct. Dotting my 'i's and 't's
#define CAREFULLY_UNBIND_AND_REBIND 1

namespace morph {

    union float_bytes
    {
        float f;
        uint8_t bytes[sizeof(float)];
    };

    //! Forward declaration of a Visual class
    class Visual;

    /*!
     * OpenGL model base class
     *
     * This class is a base 'OpenGL model' class. It has the common code to create the
     * vertices for some individual OpengGL model which is to be rendered in a 3-D
     * scene.
     *
     * Some OpenGL models are derived directly from VisualModel; see for example
     * morph::CoordArrows.
     *
     * Most of the models in morphologica are derived via morph::VisualDataModel,
     * which adds a common mechanism for managing the data which is to be visualised
     * by the final 'Visual' object (such as morph::HexGridVisual or
     * morph::ScatterVisual)
     *
     * This class contains some common 'object primitives' code, such as computeSphere
     * and computeCone, which compute the vertices that will make up sphere and cone,
     * respectively.
     */
    class VisualModel
    {
        //! Debug rendering process with cout messages
        static constexpr bool debug_render = false;

    public:
        VisualModel()
        {
            this->mv_offset = {0.0, 0.0, 0.0};
            this->model_scaling.setToIdentity();
        }

        VisualModel (const vec<float> _mv_offset)
        {
            this->mv_offset = _mv_offset;
            this->viewmatrix.translate (this->mv_offset);
            this->model_scaling.setToIdentity();
        }

        //! destroy gl buffers in the deconstructor
        virtual ~VisualModel()
        {
            if (this->vbos != nullptr) {
                glDeleteBuffers (numVBO, this->vbos);
                delete[] this->vbos;
                glDeleteVertexArrays (1, &this->vao);
            }
        }

        bool postVertexInitRequired = false;
        //! Common code to call after the vertices have been set up. GL has to have been initialised.
        void postVertexInit()
        {
            // Do gl memory allocation of vertex array once only
            if (this->vbos == nullptr) {
                // Create vertex array object
                glGenVertexArrays (1, &this->vao); // Safe for OpenGL 4.4-
                morph::gl::Util::checkError (__FILE__, __LINE__);
            }

            glBindVertexArray (this->vao);
            morph::gl::Util::checkError (__FILE__, __LINE__);

            // Create the vertex buffer objects (once only)
            if (this->vbos == nullptr) {
                this->vbos = new GLuint[numVBO];
                glGenBuffers (numVBO, this->vbos); // OpenGL 4.4- safe
            }
            morph::gl::Util::checkError (__FILE__, __LINE__);

            // Set up the indices buffer - bind and buffer the data in this->indices
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbos[idxVBO]);
            morph::gl::Util::checkError (__FILE__, __LINE__);

            //std::cout << "indices.size(): " << this->indices.size() << std::endl;
            int sz = this->indices.size() * sizeof(GLuint);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz, this->indices.data(), GL_STATIC_DRAW);
            morph::gl::Util::checkError (__FILE__, __LINE__);

            // Binds data from the "C++ world" to the OpenGL shader world for
            // "position", "normalin" and "color"
            // (bind, buffer and set vertex array object attribute)
            this->setupVBO (this->vbos[posnVBO], this->vertexPositions, visgl::posnLoc);
            this->setupVBO (this->vbos[normVBO], this->vertexNormals, visgl::normLoc);
            this->setupVBO (this->vbos[colVBO], this->vertexColors, visgl::colLoc);

#ifdef CAREFULLY_UNBIND_AND_REBIND
            // Unbind only the vertex array (not the buffers, that causes GL_INVALID_ENUM errors)
            glBindVertexArray(0);
            morph::gl::Util::checkError (__FILE__, __LINE__);
#endif
            this->postVertexInitRequired = false;
        }

        //! Initialize vertex buffer objects and vertex array object. Empty for 'text only' VisualModels.
        virtual void initializeVertices() {};

        //! Re-initialize the buffers. Client code might have appended to
        //! vertexPositions/Colors/Normals and indices before calling this method.
        void reinit_buffers()
        {
            if (this->postVertexInitRequired == true) { this->postVertexInit(); }
            morph::gl::Util::checkError (__FILE__, __LINE__);
            // Now re-set up the VBOs
#ifdef CAREFULLY_UNBIND_AND_REBIND // Experimenting with better buffer binding.
            glBindVertexArray (this->vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbos[idxVBO]);
#endif
            int sz = this->indices.size() * sizeof(GLuint);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz, this->indices.data(), GL_STATIC_DRAW);
            this->setupVBO (this->vbos[posnVBO], this->vertexPositions, visgl::posnLoc);
            this->setupVBO (this->vbos[normVBO], this->vertexNormals, visgl::normLoc);
            this->setupVBO (this->vbos[colVBO], this->vertexColors, visgl::colLoc);

#ifdef CAREFULLY_UNBIND_AND_REBIND
            glBindVertexArray(0);
            morph::gl::Util::checkError (__FILE__, __LINE__);
#endif
        }

        void clearTexts() { this->texts.clear(); }

        //! Clear out the model, *including text models*
        void clear()
        {
            this->vertexPositions.clear();
            this->vertexNormals.clear();
            this->vertexColors.clear();
            this->indices.clear();
            this->clearTexts();
            this->idx = 0;
            this->reinit_buffers();
        }

        //! Re-create the model - called after updating data
        void reinit()
        {
            // Fixme: Better not to clear, then repeatedly pushback here:
            this->vertexPositions.clear();
            this->vertexNormals.clear();
            this->vertexColors.clear();
            this->indices.clear();
            // NB: Do NOT call clearTexts() here! We're only updating the model itself.
            this->idx = 0;
            this->initializeVertices();
            this->reinit_buffers();
        }

        //! For some models it's important to clear the texts when reinitialising. This
        //! is NOT the same as VisualModel::clear() followed by
        //! initializeVertices(). For the same effect, you can call clearTexts() then
        //! reinit().
        void reinit_with_clearTexts()
        {
            this->vertexPositions.clear();
            this->vertexNormals.clear();
            this->vertexColors.clear();
            this->indices.clear();
            this->clearTexts();
            this->idx = 0;
            this->initializeVertices();
            this->reinit_buffers();
        }

        void reserve_vertices (size_t n_vertices)
        {
            this->vertexPositions.reserve (3*n_vertices);
            this->vertexNormals.reserve (3*n_vertices);
            this->vertexColors.reserve (3*n_vertices);
            this->indices.reserve (6*n_vertices);
        }

        //! A function to call initialiseVertices and postVertexInit after any necessary
        //! attributes have been set (see, for example, setting the colour maps up in
        //! VisualDataModel).
        void finalize()
        {
            this->initializeVertices();
            this->postVertexInitRequired = true;
        }

        //! Render the VisualModel
        virtual void render()
        {
            if (this->hide == true) { return; }

            // Execute post-vertex init at render, as GL should be available.
            if (this->postVertexInitRequired == true) { this->postVertexInit(); }

            GLint prev_shader;
            glGetIntegerv (GL_CURRENT_PROGRAM, &prev_shader);

            // Ensure the correct program is in play for this VisualModel
            glUseProgram (this->get_gprog(this->parentVis));

            if (!this->indices.empty()) {
                // It is only necessary to bind the vertex array object before rendering
                // (not the vertex buffer objects)
                glBindVertexArray (this->vao);

                // Pass this->float to GLSL so the model can have an alpha value.
                GLint loc_a = glGetUniformLocation (this->get_gprog(this->parentVis), static_cast<const GLchar*>("alpha"));
                if (loc_a != -1) { glUniform1f (loc_a, this->alpha); }

                GLint loc_v = glGetUniformLocation (this->get_gprog(this->parentVis), static_cast<const GLchar*>("v_matrix"));
                if (loc_v != -1) { glUniformMatrix4fv (loc_v, 1, GL_FALSE, this->scenematrix.mat.data()); }

                // Should be able to apply scaling to the model matrix
                GLint loc_m = glGetUniformLocation (this->get_gprog(this->parentVis), static_cast<const GLchar*>("m_matrix"));
                if (loc_m != -1) { glUniformMatrix4fv (loc_m, 1, GL_FALSE, (this->model_scaling * this->viewmatrix).mat.data()); }

                if constexpr (debug_render) {
                    std::cout << "VisualModel::render: scenematrix:\n" << scenematrix << std::endl;
                    std::cout << "VisualModel::render: model viewmatrix:\n" << viewmatrix << std::endl;
                }

                // Draw the triangles
                glDrawElements (GL_TRIANGLES, this->indices.size(), GL_UNSIGNED_INT, 0);

                // Unbind the VAO
                glBindVertexArray(0);
            }
            morph::gl::Util::checkError (__FILE__, __LINE__);

            // Now render any VisualTextModels
            auto ti = this->texts.begin();
            while (ti != this->texts.end()) { (*ti)->render(); ti++; }

            glUseProgram (prev_shader);

            morph::gl::Util::checkError (__FILE__, __LINE__);
        }

    public:

        //! Add a text label to the model at location (within the model coordinates) toffset. Return
        //! the text geometry of the added label so caller can place associated text correctly.
        morph::TextGeometry addLabel (const std::string& _text,
                                      const morph::vec<float, 3>& _toffset,
                                      const morph::TextFeatures& tfeatures)
        {
            if (this->get_shaderprogs(this->parentVis).tprog == 0) {
                throw std::runtime_error ("No text shader prog. Did your VisualModel-derived class set it up?");
            }

            auto tmup = std::make_unique<morph::VisualTextModel> (this->parentVis, this->get_shaderprogs(this->parentVis).tprog, tfeatures);

            if (tfeatures.centre_horz == true) {
                morph::TextGeometry tg = tmup->getTextGeometry(_text);
                morph::vec<float, 3> centred_locn = _toffset;
                centred_locn[0] = -tg.half_width();
                tmup->setupText (_text, centred_locn+this->mv_offset, tfeatures.colour);
            } else {
                tmup->setupText (_text, _toffset+this->mv_offset, tfeatures.colour);
            }

            this->texts.push_back (std::move(tmup));
            return this->texts.back()->getTextGeometry();
        }

        //! Add a text label, with given offset _toffset and the specified features. The reference
        //! to a pointer, tm, allows client code to change the text of the VisualTextModel as necessary.
        morph::TextGeometry addLabel (const std::string& _text,
                                      const morph::vec<float, 3>& _toffset,
                                      morph::VisualTextModel*& tm,
                                      const morph::TextFeatures& tfeatures)
        {
            if (this->get_shaderprogs(this->parentVis).tprog == 0) {
                throw std::runtime_error ("No text shader prog. Did your VisualModel-derived class set it up?");
            }

            auto tmup = std::make_unique<morph::VisualTextModel> (this->parentVis, this->get_shaderprogs(this->parentVis).tprog, tfeatures);

            if (tfeatures.centre_horz == true) {
                morph::TextGeometry tg = tmup->getTextGeometry(_text);
                morph::vec<float, 3> centred_locn = _toffset;
                centred_locn[0] = -tg.half_width();
                tmup->setupText (_text, centred_locn+this->mv_offset, tfeatures.colour);
            } else {
                tmup->setupText (_text, _toffset+this->mv_offset, tfeatures.colour);
            }

            this->texts.push_back (std::move(tmup));
            tm = this->texts.back().get();
            return this->texts.back()->getTextGeometry();
        }

        //! Deprecated argument format. Prefer the versions that take TextFeatures, rather than multiple args.
        morph::TextGeometry addLabel (const std::string& _text,
                                      const morph::vec<float, 3>& _toffset,
                                      const std::array<float, 3>& _tcolour = morph::colour::black,
                                      const morph::VisualFont _font = morph::VisualFont::DVSans,
                                      const float _fontsize = 0.05,
                                      const int _fontres = 24)
        {
            morph::TextFeatures tfeat (_fontsize, _fontres, false, _tcolour, _font);
            return addLabel (_text, _toffset, tfeat);
        }

        //! Deprecated argument format. Prefer the versions that take TextFeatures, rather than multiple args.
        morph::TextGeometry addLabel (const std::string& _text,
                                      const morph::vec<float, 3>& _toffset,
                                      morph::VisualTextModel*& tm,
                                      const std::array<float, 3>& _tcolour = morph::colour::black,
                                      const morph::VisualFont _font = morph::VisualFont::DVSans,
                                      const float _fontsize = 0.05,
                                      const int _fontres = 24)
        {
            morph::TextFeatures tfeat (_fontsize, _fontres, false, _tcolour, _font);
            return addLabel (_text, _toffset, tm, tfeat);
        }

        //! Setter for the viewmatrix
        void setViewMatrix (const TransformMatrix<float>& mv) { this->viewmatrix = mv; }

        //! When setting the scene matrix, also have to set the text's scene matrices.
        void setSceneMatrix (const TransformMatrix<float>& sv)
        {
            this->scenematrix = sv;
            // For each text model, also set scene matrix
            auto ti = this->texts.begin();
            while (ti != this->texts.end()) { (*ti)->setSceneMatrix (sv); ti++; }
        }

        //! Set a translation into the scene and into any child texts
        void setSceneTranslation (const vec<float>& v0)
        {
            this->scenematrix.setToIdentity();
            this->sv_offset = v0;
            this->scenematrix.translate (this->sv_offset);
            this->scenematrix.rotate (this->sv_rotation);
            auto ti = this->texts.begin();
            while (ti != this->texts.end()) { (*ti)->setSceneTranslation (v0); ti++; }
        }

        //! Set a translation (only) into the scene view matrix
        void addSceneTranslation (const vec<float>& v0)
        {
            this->sv_offset += v0;
            this->scenematrix.translate (v0);
        }

        //! Set a rotation (only) into the scene view matrix
        void setSceneRotation (const Quaternion<float>& r)
        {
            this->scenematrix.setToIdentity();
            this->sv_rotation = r;
            this->scenematrix.translate (this->sv_offset);
            this->scenematrix.rotate (this->sv_rotation);
        }

        //! Add a rotation to the scene view matrix
        void addSceneRotation (const Quaternion<float>& r)
        {
            this->sv_rotation.premultiply (r);
            this->scenematrix.rotate (r);
        }

        //! Set a translation to the model view matrix
        void setViewTranslation (const vec<float>& v0)
        {
            this->viewmatrix.setToIdentity();
            this->mv_offset = v0;
            this->viewmatrix.translate (this->mv_offset);
            this->viewmatrix.rotate (this->mv_rotation);
        }

        //! Add a translation to the model view matrix
        void addViewTranslation (const vec<float>& v0)
        {
            this->mv_offset += v0;
            this->viewmatrix.translate (v0);
        }

        void setViewRotationFixTexts (const Quaternion<float>& r)
        {
            this->viewmatrix.setToIdentity();
            this->mv_rotation = r;
            this->viewmatrix.translate (this->mv_offset);
            this->viewmatrix.rotate (this->mv_rotation);
        }

        //! Set a rotation (only) into the view
        void setViewRotation (const Quaternion<float>& r)
        {
            this->viewmatrix.setToIdentity();
            this->mv_rotation = r;
            this->viewmatrix.translate (this->mv_offset);
            this->viewmatrix.rotate (this->mv_rotation);

            // When rotating a model that contains texts, we need to rotate the scene
            // for the texts and also inverse-rotate the view of the texts.
            auto ti = this->texts.begin();
            while (ti != this->texts.end()) {
                // Rotate the scene. Note this won't work if the VisualModel has a
                // mv_offset that is away from the origin.
                (*ti)->setSceneRotation (r); // Need this to rotate about mv_offset. BUT the
                                             // translation is already there in the text,
                                             // but in the MODEL view.

                // Rotate the view of the text an opposite amount, to keep it facing forwards
                (*ti)->setViewRotation (r.invert());
                ti++;
            }
        }

        //! Apply a further rotation to the model view matrix
        void addViewRotation (const Quaternion<float>& r)
        {
            this->mv_rotation.premultiply (r);
            this->viewmatrix.rotate (r);
            std::cout << "VisualModel::addViewRotation: FIXME? or t->addSceneRotation(r)?\n";
            auto ti = this->texts.begin();
            while (ti != this->texts.end()) { (*ti)->addViewRotation (r); ti++; }
        }

        // The alpha attribute accessors
        void setAlpha (const float _a) { this->alpha = _a; }
        float getAlpha() const { return this->alpha; }
        void incAlpha()
        {
            this->alpha += 0.1f;
            this->alpha = this->alpha > 1.0f ? 1.0f : this->alpha;
        }
        void decAlpha()
        {
            this->alpha -= 0.1f;
            this->alpha = this->alpha < 0.0f ? 0.0f : this->alpha;
        }

        // The hide attribute accessors
        void setHide (const bool _h = true) { this->hide = _h; }
        void toggleHide() { this->hide = this->hide ? false : true; }
        float hidden() const { return this->hide; }

        /*
         * Methods used by Visual::savegltf()
         */

        // Get mv_offset in a json-friendly string
        std::string translation_str() { return this->mv_offset.str_mat(); }

        // Return the number of elements in this->indices
        size_t indices_size() { return this->indices.size(); }
        float indices_max() { return this->idx_max; }
        float indices_min() { return this->idx_min; }
        size_t indices_bytes() { return this->indices.size() * sizeof (GLuint); }
        // Return base64 encoded version of indices
        std::string indices_base64()
        {
            std::vector<std::uint8_t> idx_bytes (this->indices.size()<<2, 0);
            size_t b = 0;
            for (auto i : this->indices) {
                idx_bytes[b++] = i & 0xff;
                idx_bytes[b++] = i >> 8 & 0xff;
                idx_bytes[b++] = i >> 16 & 0xff;
                idx_bytes[b++] = i >> 24 & 0xff;
            }
            return base64::encode (idx_bytes);
        }

        // Compute the max and min values of indices and vertexPositions/Colors/Normals for use when saving gltf files
        void computeVertexMaxMins()
        {
            // Compute index maxmins
            for (size_t i = 0; i < this->indices.size(); ++i) {
                idx_max = this->indices[i] > idx_max ? this->indices[i] : idx_max;
                idx_min = this->indices[i] < idx_min ? this->indices[i] : idx_min;
            }
            // Check every 0th entry in vertex Positions, every 1st, etc for max in the

            if (this->vertexPositions.size() != this->vertexColors.size()
                ||this->vertexPositions.size() != this->vertexNormals.size()) {
                throw std::runtime_error ("Expect vertexPositions, Colors and Normals vectors all to have same size");
            }

            for (size_t i = 0; i < this->vertexPositions.size(); i+=3) {
                vpos_maxes[0] =  (vertexPositions[i] > vpos_maxes[0]) ? vertexPositions[i] : vpos_maxes[0];
                vpos_maxes[1] =  (vertexPositions[i+1] > vpos_maxes[1]) ? vertexPositions[i+1] : vpos_maxes[1];
                vpos_maxes[2] =  (vertexPositions[i+2] > vpos_maxes[2]) ? vertexPositions[i+2] : vpos_maxes[2];
                vcol_maxes[0] =  (vertexColors[i] > vcol_maxes[0]) ? vertexColors[i] : vcol_maxes[0];
                vcol_maxes[1] =  (vertexColors[i+1] > vcol_maxes[1]) ? vertexColors[i+1] : vcol_maxes[1];
                vcol_maxes[2] =  (vertexColors[i+2] > vcol_maxes[2]) ? vertexColors[i+2] : vcol_maxes[2];
                vnorm_maxes[0] =  (vertexNormals[i] > vnorm_maxes[0]) ? vertexNormals[i] : vnorm_maxes[0];
                vnorm_maxes[1] =  (vertexNormals[i+1] > vnorm_maxes[1]) ? vertexNormals[i+1] : vnorm_maxes[1];
                vnorm_maxes[2] =  (vertexNormals[i+2] > vnorm_maxes[2]) ? vertexNormals[i+2] : vnorm_maxes[2];

                vpos_mins[0] =  (vertexPositions[i] < vpos_mins[0]) ? vertexPositions[i] : vpos_mins[0];
                vpos_mins[1] =  (vertexPositions[i+1] < vpos_mins[1]) ? vertexPositions[i+1] : vpos_mins[1];
                vpos_mins[2] =  (vertexPositions[i+2] < vpos_mins[2]) ? vertexPositions[i+2] : vpos_mins[2];
                vcol_mins[0] =  (vertexColors[i] < vcol_mins[0]) ? vertexColors[i] : vcol_mins[0];
                vcol_mins[1] =  (vertexColors[i+1] < vcol_mins[1]) ? vertexColors[i+1] : vcol_mins[1];
                vcol_mins[2] =  (vertexColors[i+2] < vcol_mins[2]) ? vertexColors[i+2] : vcol_mins[2];
                vnorm_mins[0] =  (vertexNormals[i] < vnorm_mins[0]) ? vertexNormals[i] : vnorm_mins[0];
                vnorm_mins[1] =  (vertexNormals[i+1] < vnorm_mins[1]) ? vertexNormals[i+1] : vnorm_mins[1];
                vnorm_mins[2] =  (vertexNormals[i+2] < vnorm_mins[2]) ? vertexNormals[i+2] : vnorm_mins[2];
            }
        }

        size_t vpos_size() { return this->vertexPositions.size(); }
        std::string vpos_max() { return this->vpos_maxes.str_mat(); }
        std::string vpos_min() { return this->vpos_mins.str_mat(); }
        size_t vpos_bytes() { return this->vertexPositions.size() * sizeof (float); }
        std::string vpos_base64()
        {
            std::vector<std::uint8_t> _bytes (this->vertexPositions.size()<<2, 0);
            size_t b = 0;
            float_bytes fb;
            for (auto i : this->vertexPositions) {
                fb.f = i;
                _bytes[b++] = fb.bytes[0];
                _bytes[b++] = fb.bytes[1];
                _bytes[b++] = fb.bytes[2];
                _bytes[b++] = fb.bytes[3];
            }
            return base64::encode (_bytes);
        }
        size_t vcol_size() { return this->vertexColors.size(); }
        std::string vcol_max() { return this->vcol_maxes.str_mat(); }
        std::string vcol_min() { return this->vcol_mins.str_mat(); }
        size_t vcol_bytes() { return this->vertexColors.size() * sizeof (float); }
        std::string vcol_base64()
        {
            std::vector<std::uint8_t> _bytes (this->vertexColors.size()<<2, 0);
            size_t b = 0;
            float_bytes fb;
            for (auto i : this->vertexColors) {
                fb.f = i;
                _bytes[b++] = fb.bytes[0];
                _bytes[b++] = fb.bytes[1];
                _bytes[b++] = fb.bytes[2];
                _bytes[b++] = fb.bytes[3];
            }
            return base64::encode (_bytes);
        }
        size_t vnorm_size() { return this->vertexNormals.size(); }
        std::string vnorm_max() { return this->vnorm_maxes.str_mat(); }
        std::string vnorm_min() { return this->vnorm_mins.str_mat(); }
        size_t vnorm_bytes() { return this->vertexNormals.size() * sizeof (float); }
        std::string vnorm_base64()
        {
            std::vector<std::uint8_t> _bytes (this->vertexNormals.size()<<2, 0);
            size_t b = 0;
            float_bytes fb;
            for (auto i : this->vertexNormals) {
                fb.f = i;
                _bytes[b++] = fb.bytes[0];
                _bytes[b++] = fb.bytes[1];
                _bytes[b++] = fb.bytes[2];
                _bytes[b++] = fb.bytes[3];
            }
            return base64::encode (_bytes);
        }
        // end Visual::savegltf() methods

        //! If true, then this VisualModel should always be viewed in a plane - it's a 2D model
        bool twodimensional = false;

        //! The current indices index
        GLuint idx = 0;

        //! Set scaling in all dimensions
        void setSizeScale (const float scl)
        {
            this->model_scaling.setToIdentity();
            this->model_scaling[0] = scl;
            this->model_scaling[5] = scl;
            this->model_scaling[10] = scl;
        }
        //! Set scaling in xy only
        void setSizeScale (const float xscl, const float yscl)
        {
            this->model_scaling.setToIdentity();
            this->model_scaling[0] = xscl;
            this->model_scaling[5] = yscl;
        }

    public:
        // A function that will be runtime defined to get_shaderprogs from a pointer to
        // Visual (saving a boilerplate argument and avoiding that killer circular
        // dependency at the cost of one line of boilerplate in client programs)
        std::function<morph::visgl::visual_shaderprogs(morph::Visual*)> get_shaderprogs;
        // Get the graphics shader prog id
        std::function<GLuint(morph::Visual*)> get_gprog;
        // Get the text shader prog id
        std::function<GLuint(morph::Visual*)> get_tprog;

        // Setter for the parent pointer, parentVis
        void set_parent (morph::Visual* _vis)
        {
            if (this->parentVis != nullptr) { throw std::runtime_error ("VisualModel: Set the parent pointer once only!"); }
            this->parentVis = _vis;
        }

    protected:

        //! The model-specific view matrix.
        TransformMatrix<float> viewmatrix;
        //! The model-specific scene view matrix.
        TransformMatrix<float> scenematrix;
        //! An additional scaling applied to viewmatrix to scale the size of the model [see render()]
        TransformMatrix<float> model_scaling;

        /*!
         * The spatial offset of this VisualModel within the morph::Visual 'scene
         * view'. Note that this is not incorporated into the computation of the
         * vertices, but is instead applied when the object is rendered as part of the
         * model->world transformation - it's applied as a translation in
         * VisualModel::viewmatrix.
         */
        vec<float> mv_offset;
        //! Model view rotation
        Quaternion<float> mv_rotation;

        //! Scene view offset
        vec<float> sv_offset;
        //! Scene view rotation
        Quaternion<float> sv_rotation;

        //! A vector of pointers to text models that should be rendered.
        std::vector<std::unique_ptr<morph::VisualTextModel>> texts;

        //! This enum contains the positions within the vbo array of the different
        //! vertex buffer objects
        enum VBOPos { posnVBO, normVBO, colVBO, idxVBO, numVBO };

        //! A unit vector in the x direction
        morph::vec<float, 3> ux = {1,0,0};
        //! A unit vector in the y direction
        morph::vec<float, 3> uy = {0,1,0};
        //! A unit vector in the z direction
        morph::vec<float, 3> uz = {0,0,1};

        /*
         * Compute positions and colours of vertices for the hexes and store in these:
         */

        //! The OpenGL Vertex Array Object
        GLuint vao;

        //! Vertex Buffer Objects stored in an array
        GLuint* vbos = nullptr;

        //! CPU-side data for indices
        std::vector<GLuint> indices;
        //! CPU-side data for vertex positions
        std::vector<float> vertexPositions;
        //! CPU-side data for vertex normals
        std::vector<float> vertexNormals;
        //! CPU-side data for vertex colours
        std::vector<float> vertexColors;

        // The max and min values in the next 8 attriubutes are only computed if gltf files are going to be output by Visual::safegltf()

        //! Max values of 0th, 1st and 2nd coordinates in vertexPositions
        morph::vec<float, 3> vpos_maxes = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        //! Min values in vertexPositions
        morph::vec<float, 3> vpos_mins = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        morph::vec<float, 3> vcol_maxes = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        morph::vec<float, 3> vcol_mins = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        morph::vec<float, 3> vnorm_maxes = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        morph::vec<float, 3> vnorm_mins = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        //! Max value in indices
        GLuint idx_max = 0;
        //! Min value in indices.
        GLuint idx_min = std::numeric_limits<GLuint>::max();

        //! A model-wide alpha value for the shader
        float alpha = 1.0f;
        //! If true, then calls to VisualModel::render should return
        bool hide = false;

        // The morph::Visual in which this model exists.
        morph::Visual* parentVis = nullptr;

        //! Push three floats onto the vector of floats \a vp
        void vertex_push (const float& x, const float& y, const float& z, std::vector<float>& vp)
        {
            vec<float> vec = {x, y, z};
            std::copy (vec.begin(), vec.end(), std::back_inserter (vp));
        }
        //! Push array of 3 floats onto the vector of floats \a vp
        void vertex_push (const std::array<float, 3>& arr, std::vector<float>& vp)
        {
            std::copy (arr.begin(), arr.end(), std::back_inserter (vp));
        }
        //! Push morph::vec of 3 floats onto the vector of floats \a vp
        void vertex_push (const vec<float>& vec, std::vector<float>& vp)
        {
            std::copy (vec.begin(), vec.end(), std::back_inserter (vp));
        }

        //! Set up a vertex buffer object - bind, buffer and set vertex array object attribute
        void setupVBO (GLuint& buf, std::vector<float>& dat, unsigned int bufferAttribPosition)
        {
            int sz = dat.size() * sizeof(float);
            glBindBuffer (GL_ARRAY_BUFFER, buf);
            morph::gl::Util::checkError (__FILE__, __LINE__);
            glBufferData (GL_ARRAY_BUFFER, sz, dat.data(), GL_STATIC_DRAW);
            morph::gl::Util::checkError (__FILE__, __LINE__);
            glVertexAttribPointer (bufferAttribPosition, 3, GL_FLOAT, GL_FALSE, 0, (void*)(0));
            morph::gl::Util::checkError (__FILE__, __LINE__);
            glEnableVertexAttribArray (bufferAttribPosition);
            morph::gl::Util::checkError (__FILE__, __LINE__);
        }

        /*!
         * Create a tube from \a start to \a end, with radius \a r and a colour which
         * transitions from the colour \a colStart to \a colEnd.
         *
         * \param idx The index into the 'vertex array'
         * \param start The start of the tube
         * \param end The end of the tube
         * \param colStart The tube starting colour
         * \param colEnd The tube's ending colour
         * \param r Radius of the tube
         * \param segments Number of segments used to render the tube
         */
        void computeTube (GLuint& idx, vec<float> start, vec<float> end,
                          std::array<float, 3> colStart, std::array<float, 3> colEnd,
                          float r = 1.0f, int segments = 12)
        {
            // The vector from start to end defines a vector and a plane. Find a
            // 'circle' of points in that plane.
            vec<float> vstart = start;
            vec<float> vend = end;
            vec<float> v = vend - vstart;
            v.renormalize();

            // circle in a plane defined by a point (v0 = vstart or vend) and a normal
            // (v) can be found: Choose random vector vr. A vector inplane = vr ^ v. The
            // unit in-plane vector is inplane.normalise. Can now use that vector in the
            // plan to define a point on the circle. Note that this starting point on
            // the circle is at a random position, which means that this version of
            // computeTube is useful for tubes that have quite a few segments.
            vec<float> rand_vec;
            rand_vec.randomize();
            vec<float> inplane = rand_vec.cross(v);
            inplane.renormalize();

            // Now use parameterization of circle inplane = p1-x1 and
            // c1(t) = ( (p1-x1).normalized sin(t) + v.normalized cross (p1-x1).normalized * cos(t) )
            // c1(t) = ( inplane sin(t) + v * inplane * cos(t)
            vec<float> v_x_inplane = v.cross(inplane);

            // Push the central point of the start cap - this is at location vstart
            this->vertex_push (vstart, this->vertexPositions);
            this->vertex_push (-v, this->vertexNormals);
            this->vertex_push (colStart, this->vertexColors);

            // Start cap vertices. Draw as a triangle fan, but record indices so that we
            // only need a single call to glDrawElements.
            for (int j = 0; j < segments; j++) {
                // t is the angle of the segment
                float t = j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = inplane * sin(t) * r + v_x_inplane * cos(t) * r;
                this->vertex_push (vstart+c, this->vertexPositions);
                this->vertex_push (-v, this->vertexNormals);
                this->vertex_push (colStart, this->vertexColors);
            }

            // Intermediate, near start cap. Normals point in direction c
            for (int j = 0; j < segments; j++) {
                float t = j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = inplane * sin(t) * r + v_x_inplane * cos(t) * r;
                this->vertex_push (vstart+c, this->vertexPositions);
                c.renormalize();
                this->vertex_push (c, this->vertexNormals);
                this->vertex_push (colStart, this->vertexColors);
            }

            // Intermediate, near end cap. Normals point in direction c
            for (int j = 0; j < segments; j++) {
                float t = (float)j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = inplane * sin(t) * r + v_x_inplane * cos(t) * r;
                this->vertex_push (vend+c, this->vertexPositions);
                c.renormalize();
                this->vertex_push (c, this->vertexNormals);
                this->vertex_push (colEnd, this->vertexColors);
            }

            // Bottom cap vertices
            for (int j = 0; j < segments; j++) {
                float t = (float)j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = inplane * sin(t) * r + v_x_inplane * cos(t) * r;
                this->vertex_push (vend+c, this->vertexPositions);
                this->vertex_push (v, this->vertexNormals);
                this->vertex_push (colEnd, this->vertexColors);
            }

            // Bottom cap. Push centre vertex as the last vertex.
            this->vertex_push (vend, this->vertexPositions);
            this->vertex_push (v, this->vertexNormals);
            this->vertex_push (colEnd, this->vertexColors);

            // Note: number of vertices = segments * 4 + 2.
            int nverts = (segments * 4) + 2;

            // After creating vertices, push all the indices.
            GLuint capMiddle = idx;
            GLuint capStartIdx = idx + 1;
            GLuint endMiddle = idx + (GLuint)nverts - 1;
            GLuint endStartIdx = capStartIdx + (3*segments);

            //std::cout << "start cap" << std::endl;
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (capMiddle);
                //std::cout << "add " << capMiddle << " to indices\n";
                this->indices.push_back (capStartIdx + j);
                //std::cout << "add " << (capStartIdx+j) << " to indices\n";
                this->indices.push_back (capStartIdx + 1 + j);
                //std::cout << "add " << (capStartIdx+1+j) << " to indices\n";
            }
            // Last one
            this->indices.push_back (capMiddle);
            //std::cout << "add " << capMiddle << " to indices\n";
            this->indices.push_back (capStartIdx + segments - 1);
            //std::cout << "add " << (capStartIdx + segments - 1) << " to indices\n";
            this->indices.push_back (capStartIdx);
            //std::cout << "add " << (capStartIdx) << " to indices\n";

            // MIDDLE SECTIONS
            for (int lsection = 0; lsection < 3; ++lsection) {
                capStartIdx = idx + 1 + lsection*segments;
                endStartIdx = capStartIdx + segments;
                //std::cout << "For lsection " << lsection << " capStartIdx=" << capStartIdx
                //          << ", and endStartIdx=" << endStartIdx << std::endl;
                // This does sides between start and end. I want to do this three times.
                for (int j = 0; j < segments; j++) {
                    //std::cout << "Triangle 1\n";
                    this->indices.push_back (capStartIdx + j);
                    //std::cout << "1. add " << (capStartIdx + j) << " to indices\n";
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                        //std::cout << "1. add " << (capStartIdx) << " to indices\n";
                    } else {
                        this->indices.push_back (capStartIdx + 1 + j);
                        //std::cout << "1. add " << (capStartIdx + j + 1) << " to indices\n";
                    }
                    this->indices.push_back (endStartIdx + j);
                    //std::cout << "1. add " << (endStartIdx + j) << " to indices\n";
                    // 2:
                    //std::cout << "Triangle 2\n";
                    this->indices.push_back (endStartIdx + j);
                    //std::cout << "2. add " << (endStartIdx + j) << " to indices\n";
                    if (j == (segments-1)) {
                        this->indices.push_back (endStartIdx);
                        //std::cout << "2. add " << (endStartIdx) << " to indices\n";
                    } else {
                        this->indices.push_back (endStartIdx + 1 + j);
                        //std::cout << "2. add " << (endStartIdx + 1 + j) << " to indices\n";
                    }
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                        //std::cout << "2. add " << (capStartIdx) << " to indices\n";
                    } else {
                        this->indices.push_back (capStartIdx + j + 1);
                        //std::cout << "2. add " << (capStartIdx + j + 1) << " to indices\n";
                    }
                }
            }
            //std::cout << "endStartIdx after loop = " << endStartIdx << std::endl;

            // bottom cap
            //std::cout << "vend cap" << std::endl;
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (endMiddle);
                //std::cout << "add " << (endMiddle) << " to indices\n";
                this->indices.push_back (endStartIdx + j);
                //std::cout << "add " << (endStartIdx + j) << " to indices\n";
                this->indices.push_back (endStartIdx + 1 + j);
                //std::cout << "add " << (endStartIdx + 1 + j) << " to indices\n---\n";
            }
            // Last one
            this->indices.push_back (endMiddle);
            //std::cout << "add " << (endMiddle) << " to indices\n";
            this->indices.push_back (endStartIdx + segments - 1);
            //std::cout << "add " << (endStartIdx - 1 + segments) << " to indices\n";
            this->indices.push_back (endStartIdx);
            //std::cout << "add " << (endStartIdx) << " to indices\n";

            // Update idx
            idx += nverts;
        } // end computeTube with randomly initialized end vertices

        /*!
         * Compute a tube. This version requires unit vectors for orientation of the
         * tube end faces/vertices (useful for graph markers). The other version uses a
         * randomly chosen vector to do this.
         *
         * Create a tube from \a start to \a end, with radius \a r and a colour which
         * transitions from the colour \a colStart to \a colEnd.
         *
         * \param idx The index into the 'vertex array'
         * \param start The start of the tube
         * \param end The end of the tube
         * \param _ux a vector in the x axis direction for the end face
         * \param _uy a vector in the y axis direction
         * \param colStart The tube starting colour
         * \param colEnd The tube's ending colour
         * \param r Radius of the tube
         * \param segments Number of segments used to render the tube
         * \param rotation A rotation in the _ux/_uy plane to orient the vertices of the
         * tube. Useful if this is to be a short tube used as a graph marker.
         */
        void computeTube (GLuint& idx, vec<float> start, vec<float> end,
                          vec<float> _ux, vec<float> _uy,
                          std::array<float, 3> colStart, std::array<float, 3> colEnd,
                          float r = 1.0f, int segments = 12, float rotation = 0.0f)
        {
            // The vector from start to end defines direction of the tube
            vec<float> vstart = start;
            vec<float> vend = end;

            // v is a face normal
            vec<float> v = _uy.cross(_ux);
            v.renormalize();

            // Push the central point of the start cap - this is at location vstart
            this->vertex_push (vstart, this->vertexPositions);
            this->vertex_push (-v, this->vertexNormals);
            this->vertex_push (colStart, this->vertexColors);

            // Start cap vertices (a triangle fan)
            for (int j = 0; j < segments; j++) {
                // t is the angle of the segment
                float t = rotation + j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = _ux * sin(t) * r + _uy * cos(t) * r;
                this->vertex_push (vstart+c, this->vertexPositions);
                this->vertex_push (-v, this->vertexNormals);
                this->vertex_push (colStart, this->vertexColors);
            }

            // Intermediate, near start cap. Normals point in direction c
            for (int j = 0; j < segments; j++) {
                float t = rotation + j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = _ux * sin(t) * r + _uy * cos(t) * r;
                this->vertex_push (vstart+c, this->vertexPositions);
                c.renormalize();
                this->vertex_push (c, this->vertexNormals);
                this->vertex_push (colStart, this->vertexColors);
            }

            // Intermediate, near end cap. Normals point in direction c
            for (int j = 0; j < segments; j++) {
                float t = rotation + (float)j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = _ux * sin(t) * r + _uy * cos(t) * r;
                this->vertex_push (vend+c, this->vertexPositions);
                c.renormalize();
                this->vertex_push (c, this->vertexNormals);
                this->vertex_push (colEnd, this->vertexColors);
            }

            // Bottom cap vertices
            for (int j = 0; j < segments; j++) {
                float t = rotation + (float)j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = _ux * sin(t) * r + _uy * cos(t) * r;
                this->vertex_push (vend+c, this->vertexPositions);
                this->vertex_push (v, this->vertexNormals);
                this->vertex_push (colEnd, this->vertexColors);
            }

            // Bottom cap. Push centre vertex as the last vertex.
            this->vertex_push (vend, this->vertexPositions);
            this->vertex_push (v, this->vertexNormals);
            this->vertex_push (colEnd, this->vertexColors);

            // Number of vertices = segments * 4 + 2.
            int nverts = (segments * 4) + 2;

            // After creating vertices, push all the indices.
            GLuint capMiddle = idx;
            GLuint capStartIdx = idx + 1;
            GLuint endMiddle = idx + (GLuint)nverts - 1;
            GLuint endStartIdx = capStartIdx + (3*segments);

            // Start cap indices
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (capMiddle);
                this->indices.push_back (capStartIdx + j);
                this->indices.push_back (capStartIdx + 1 + j);
            }
            // Last one
            this->indices.push_back (capMiddle);
            this->indices.push_back (capStartIdx + segments - 1);
            this->indices.push_back (capStartIdx);

            // Middle sections
            for (int lsection = 0; lsection < 3; ++lsection) {
                capStartIdx = idx + 1 + lsection*segments;
                endStartIdx = capStartIdx + segments;
                for (int j = 0; j < segments; j++) {
                    this->indices.push_back (capStartIdx + j);
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                    } else {
                        this->indices.push_back (capStartIdx + 1 + j);
                    }
                    this->indices.push_back (endStartIdx + j);
                    this->indices.push_back (endStartIdx + j);
                    if (j == (segments-1)) {
                        this->indices.push_back (endStartIdx);
                    } else {
                        this->indices.push_back (endStartIdx + 1 + j);
                    }
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                    } else {
                        this->indices.push_back (capStartIdx + j + 1);
                    }
                }
            }

            // bottom cap
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (endMiddle);
                this->indices.push_back (endStartIdx + j);
                this->indices.push_back (endStartIdx + 1 + j);
            }
            this->indices.push_back (endMiddle);
            this->indices.push_back (endStartIdx + segments - 1);
            this->indices.push_back (endStartIdx);

            // Update idx
            idx += nverts;
        } // end computeTube with ux/uy vectors for faces

        //! Compute a Quad from 4 arbitrary corners which must be ordered clockwise around the quad.
        void computeFlatQuad (GLuint& idx,
                              vec<float> c1, vec<float> c2,
                              vec<float> c3, vec<float> c4,
                              std::array<float, 3> col)
        {
            // is the face normal
            vec<float> u1 = c1-c2;
            vec<float> u2 = c2-c3;
            vec<float> v = u1.cross(u2);
            v.renormalize();
            // Push corner vertices
            this->vertex_push (c1, this->vertexPositions);
            this->vertex_push (c2, this->vertexPositions);
            this->vertex_push (c3, this->vertexPositions);
            this->vertex_push (c4, this->vertexPositions);
            // Colours/normals
            for (size_t i = 0; i < 4; ++i) {
                this->vertex_push (col, this->vertexColors);
                this->vertex_push (v, this->vertexNormals);
            }
            GLuint botLeft = idx;
            this->indices.push_back (botLeft);
            this->indices.push_back (botLeft+1);
            this->indices.push_back (botLeft+2);
            this->indices.push_back (botLeft);
            this->indices.push_back (botLeft+2);
            this->indices.push_back (botLeft+3);
            idx += 4;
        }

        /*!
         * Compute a tube. This version requires unit vectors for orientation of the
         * tube end faces/vertices (useful for graph markers). The other version uses a
         * randomly chosen vector to do this.
         *
         * Create a tube from \a start to \a end, with radius \a r and a colour which
         * transitions from the colour \a colStart to \a colEnd.
         *
         * \param idx The index into the 'vertex array'
         * \param vstart The centre of the polygon
         * \param _ux a vector in the x axis direction for the end face
         * \param _uy a vector in the y axis direction
         * \param col The polygon colour
         * \param r Radius of the tube
         * \param segments Number of segments used to render the tube
         * \param rotation A rotation in the ux/uy plane to orient the vertices of the
         * tube. Useful if this is to be a short tube used as a graph marker.
         */
        void computeFlatPoly (GLuint& idx, vec<float> vstart,
                              vec<float> _ux, vec<float> _uy,
                              std::array<float, 3> col,
                              float r=1.0f, int segments=12, float rotation=0.0f)
        {
            // v is a face normal
            vec<float> v = _uy.cross(_ux);
            v.renormalize();

            // Push the central point of the start cap - this is at location vstart
            this->vertex_push (vstart, this->vertexPositions);
            this->vertex_push (-v, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            // Polygon vertices (a triangle fan)
            for (int j = 0; j < segments; j++) {
                // t is the angle of the segment
                float t = rotation + j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = _ux * sin(t) * r + _uy * cos(t) * r;
                this->vertex_push (vstart+c, this->vertexPositions);
                this->vertex_push (-v, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
            }

            // Number of vertices
            int nverts = segments + 1;

            // After creating vertices, push all the indices.
            GLuint capMiddle = idx;
            GLuint capStartIdx = idx + 1;
            //GLuint endMiddle = idx + (GLuint)nverts - 1;
            //GLuint endStartIdx = capStartIdx + (3*segments);

            // Start cap indices
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (capMiddle);
                this->indices.push_back (capStartIdx + j);
                this->indices.push_back (capStartIdx + 1 + j);
            }
            // Last one
            this->indices.push_back (capMiddle);
            this->indices.push_back (capStartIdx + segments - 1);
            this->indices.push_back (capStartIdx);

            // Update idx
            idx += nverts;
        } // end computeFlatPloy with ux/uy vectors for faces

        /*!
         * Make a ring of radius r, comprised of tubes(?)
         *
         * \param ro position of the centre of the ring
         * \param rc The ring colour.
         * \param r Radius of the ring
         * \param t Thickness of the ring
         * \param segments Number of tube segments used to render the ring
         */
        void computeRing (GLuint& idx, vec<float> ro, std::array<float, 3> rc, float r = 1.0f,
                          float t = 0.1f, int segments = 12)
        {
            for (int j = 0; j < segments; j++) {
                float segment = 2 * morph::mathconst<float>::pi * (float) (j) / segments;
                // x and y of inner point
                float xin = (r-(t*0.5f)) * cos(segment);
                float yin = (r-(t*0.5f)) * sin(segment);
                float xout = (r+(t*0.5f)) * cos(segment);
                float yout = (r+(t*0.5f)) * sin(segment);
                int segjnext = (j+1) % segments;
                float segnext = 2 * morph::mathconst<float>::pi * (float) (segjnext) / segments;
                float xin_n = (r-(t*0.5f)) * cos(segnext);
                float yin_n = (r-(t*0.5f)) * sin(segnext);
                float xout_n = (r+(t*0.5f)) * cos(segnext);
                float yout_n = (r+(t*0.5f)) * sin(segnext);

                // Now draw a quad
                vec<float> c1 = {xin, yin, 0};
                vec<float> c2 = {xout, yout, 0};
                vec<float> c3 = {xout_n, yout_n, 0};
                vec<float> c4 = {xin_n, yin_n, 0};
                this->computeFlatQuad (idx, ro+c1, ro+c2, ro+c3, ro+c4, rc);
            }
        }

        /*!
         * Sphere, 1 colour version.
         *
         * Code for creating a sphere as part of this model. I'll use a sphere at the centre of the arrows.
         *
         * \param idx The index into the 'vertex indices array'
         * \param so The sphere offset. Where to place this sphere...
         * \param sc The sphere colour.
         * \param r Radius of the sphere
         * \param rings Number of rings used to render the sphere
         * \param segments Number of segments used to render the sphere
         */
        void computeSphere (GLuint& idx, vec<float> so,
                            std::array<float, 3> sc,
                            float r = 1.0f, int rings = 10, int segments = 12)
        {
            // First cap, draw as a triangle fan, but record indices so that
            // we only need a single call to glDrawElements.
            float rings0 = morph::mathconst<float>::pi * -0.5;
            float _z0  = sin(rings0);
            float z0  = r * _z0;
            float r0 =  cos(rings0);
            float rings1 = morph::mathconst<float>::pi * (-0.5 + 1.0f / rings);
            float _z1 = sin(rings1);
            float z1 = r * _z1;
            float r1 = cos(rings1);
            // Push the central point
            this->vertex_push (so[0]+0.0f, so[1]+0.0f, so[2]+z0, this->vertexPositions);
            this->vertex_push (0.0f, 0.0f, -1.0f, this->vertexNormals);
            this->vertex_push (sc, this->vertexColors);

            GLuint capMiddle = idx++;
            GLuint ringStartIdx = idx;
            GLuint lastRingStartIdx = idx;

            bool firstseg = true;
            for (int j = 0; j < segments; j++) {
                float segment = 2 * morph::mathconst<float>::pi * (float) (j) / segments;
                float x = cos(segment);
                float y = sin(segment);

                float _x1 = x*r1;
                float x1 = _x1*r;
                float _y1 = y*r1;
                float y1 = _y1*r;

                this->vertex_push (so[0]+x1, so[1]+y1, so[2]+z1, this->vertexPositions);
                this->vertex_push (_x1, _y1, _z1, this->vertexNormals);
                this->vertex_push (sc, this->vertexColors);

                if (!firstseg) {
                    this->indices.push_back (capMiddle);
                    this->indices.push_back (idx-1);
                    this->indices.push_back (idx++);
                } else {
                    idx++;
                    firstseg = false;
                }
            }
            this->indices.push_back (capMiddle);
            this->indices.push_back (idx-1);
            this->indices.push_back (capMiddle+1);

            // Now add the triangles around the rings
            for (int i = 2; i < rings; i++) {

                rings0 = morph::mathconst<float>::pi * (-0.5 + (float) (i) / rings);
                _z0  = sin(rings0);
                z0  = r * _z0;
                r0 =  cos(rings0);

                for (int j = 0; j < segments; j++) {

                    // "current" segment
                    float segment = 2 * morph::mathconst<float>::pi * (float)j / segments;
                    float x = cos(segment);
                    float y = sin(segment);

                    // One vertex per segment
                    float _x0 = x*r0;
                    float x0 = _x0*r;
                    float _y0 = y*r0;
                    float y0 = _y0*r;

                    // NB: Only add ONE vertex per segment. ALREADY have the first ring!
                    this->vertex_push (so[0]+x0, so[1]+y0, so[2]+z0, this->vertexPositions);
                    // The vertex normal of a vertex that makes up a sphere is
                    // just a normal vector in the direction of the vertex.
                    this->vertex_push (_x0, _y0, _z0, this->vertexNormals);
                    this->vertex_push (sc, this->vertexColors);

                    if (j == segments - 1) {
                        // Last vertex is back to the start
                        this->indices.push_back (ringStartIdx++);
                        this->indices.push_back (idx);
                        this->indices.push_back (lastRingStartIdx);
                        this->indices.push_back (lastRingStartIdx);
                        this->indices.push_back (idx++);
                        this->indices.push_back (lastRingStartIdx+segments);
                    } else {
                        this->indices.push_back (ringStartIdx++);
                        this->indices.push_back (idx);
                        this->indices.push_back (ringStartIdx);
                        this->indices.push_back (ringStartIdx);
                        this->indices.push_back (idx++);
                        this->indices.push_back (idx);
                    }
                }
                lastRingStartIdx += segments;
            }

            // bottom cap
            rings0 = morph::mathconst<float>::pi * 0.5;
            _z0  = sin(rings0);
            z0  = r * _z0;
            r0 =  cos(rings0);
            // Push the central point of the bottom cap
            this->vertex_push (so[0]+0.0f, so[1]+0.0f, so[2]+z0, this->vertexPositions);
            this->vertex_push (0.0f, 0.0f, 1.0f, this->vertexNormals);
            this->vertex_push (sc, this->vertexColors);
            capMiddle = idx++;
            firstseg = true;
            // No more vertices to push, just do the indices for the bottom cap
            ringStartIdx = lastRingStartIdx;
            for (int j = 0; j < segments; j++) {
                if (j != segments - 1) {
                    this->indices.push_back (capMiddle);
                    this->indices.push_back (ringStartIdx++);
                    this->indices.push_back (ringStartIdx);
                } else {
                    // Last segment
                    this->indices.push_back (capMiddle);
                    this->indices.push_back (ringStartIdx);
                    this->indices.push_back (lastRingStartIdx);
                }
            }
        } // end of sphere calculation

        /*!
         * Sphere, two colour version.
         *
         * Code for creating a sphere as part of this model. I'll use a sphere at the
         * centre of the arrows.
         *
         * \param idx The index into the 'vertex indices array'
         * \param so The sphere offset. Where to place this sphere...
         * \param sc The sphere colour.
         * \param sc2 The sphere's second colour - used for cap and first ring
         * \param r Radius of the sphere
         * \param rings Number of rings used to render the sphere
         * \param segments Number of segments used to render the sphere
         */
        void computeSphere (GLuint& idx, vec<float> so,
                            std::array<float, 3> sc, std::array<float, 3> sc2,
                            float r = 1.0f, int rings = 10, int segments = 12)
        {
            // First cap, draw as a triangle fan, but record indices so that
            // we only need a single call to glDrawElements.
            float rings0 = morph::mathconst<float>::pi * -0.5;
            float _z0  = sin(rings0);
            float z0  = r * _z0;
            float r0 =  cos(rings0);
            float rings1 = morph::mathconst<float>::pi * (-0.5 + 1.0f / rings);
            float _z1 = sin(rings1);
            float z1 = r * _z1;
            float r1 = cos(rings1);
            // Push the central point
            this->vertex_push (so[0]+0.0f, so[1]+0.0f, so[2]+z0, this->vertexPositions);
            this->vertex_push (0.0f, 0.0f, -1.0f, this->vertexNormals);
            this->vertex_push (sc2, this->vertexColors);

            GLuint capMiddle = idx++;
            GLuint ringStartIdx = idx;
            GLuint lastRingStartIdx = idx;

            bool firstseg = true;
            for (int j = 0; j < segments; j++) {
                float segment = 2 * morph::mathconst<float>::pi * (float) (j) / segments;
                float x = cos(segment);
                float y = sin(segment);

                float _x1 = x*r1;
                float x1 = _x1*r;
                float _y1 = y*r1;
                float y1 = _y1*r;

                this->vertex_push (so[0]+x1, so[1]+y1, so[2]+z1, this->vertexPositions);
                this->vertex_push (_x1, _y1, _z1, this->vertexNormals);
                this->vertex_push (sc2, this->vertexColors);

                if (!firstseg) {
                    this->indices.push_back (capMiddle);
                    this->indices.push_back (idx-1);
                    this->indices.push_back (idx++);
                } else {
                    idx++;
                    firstseg = false;
                }
            }
            this->indices.push_back (capMiddle);
            this->indices.push_back (idx-1);
            this->indices.push_back (capMiddle+1);

            // Now add the triangles around the rings
            for (int i = 2; i < rings; i++) {

                rings0 = morph::mathconst<float>::pi * (-0.5 + (float) (i) / rings);
                _z0  = sin(rings0);
                z0  = r * _z0;
                r0 =  cos(rings0);

                for (int j = 0; j < segments; j++) {

                    // "current" segment
                    float segment = 2 * morph::mathconst<float>::pi * (float)j / segments;
                    float x = cos(segment);
                    float y = sin(segment);

                    // One vertex per segment
                    float _x0 = x*r0;
                    float x0 = _x0*r;
                    float _y0 = y*r0;
                    float y0 = _y0*r;

                    // NB: Only add ONE vertex per segment. ALREADY have the first ring!
                    this->vertex_push (so[0]+x0, so[1]+y0, so[2]+z0, this->vertexPositions);
                    // The vertex normal of a vertex that makes up a sphere is
                    // just a normal vector in the direction of the vertex.
                    this->vertex_push (_x0, _y0, _z0, this->vertexNormals);
                    if (i == 2 || i > (rings-2)) {
                        this->vertex_push (sc2, this->vertexColors);
                    } else {
                        this->vertex_push (sc, this->vertexColors);
                    }
                    if (j == segments - 1) {
                        // Last vertex is back to the start
                        this->indices.push_back (ringStartIdx++);
                        this->indices.push_back (idx);
                        this->indices.push_back (lastRingStartIdx);
                        this->indices.push_back (lastRingStartIdx);
                        this->indices.push_back (idx++);
                        this->indices.push_back (lastRingStartIdx+segments);
                    } else {
                        this->indices.push_back (ringStartIdx++);
                        this->indices.push_back (idx);
                        this->indices.push_back (ringStartIdx);
                        this->indices.push_back (ringStartIdx);
                        this->indices.push_back (idx++);
                        this->indices.push_back (idx);
                    }
                }
                lastRingStartIdx += segments;
            }

            // bottom cap
            rings0 = morph::mathconst<float>::pi * 0.5;
            _z0  = sin(rings0);
            z0  = r * _z0;
            r0 =  cos(rings0);
            // Push the central point of the bottom cap
            this->vertex_push (so[0]+0.0f, so[1]+0.0f, so[2]+z0, this->vertexPositions);
            this->vertex_push (0.0f, 0.0f, 1.0f, this->vertexNormals);
            this->vertex_push (sc2, this->vertexColors);
            capMiddle = idx++;
            firstseg = true;
            // No more vertices to push, just do the indices for the bottom cap
            ringStartIdx = lastRingStartIdx;
            for (int j = 0; j < segments; j++) {
                if (j != segments - 1) {
                    this->indices.push_back (capMiddle);
                    this->indices.push_back (ringStartIdx++);
                    this->indices.push_back (ringStartIdx);
                } else {
                    // Last segment
                    this->indices.push_back (capMiddle);
                    this->indices.push_back (ringStartIdx);
                    this->indices.push_back (lastRingStartIdx);
                }
            }
        }

        /*!
         * Create a cone.
         *
         * \param idx The index into the 'vertex array'
         *
         * \param centre The centre of the cone - would be the end of the line
         *
         * \param tip The tip of the cone
         *
         * \param ringoffset Move the ring forwards or backwards along the vector from
         * \a centre to \a tip. This is positive or negative proportion of tip - centre.
         *
         * \param col The cone colour
         *
         * \param r Radius of the ring
         *
         * \param segments Number of segments used to render the tube
         */
        void computeCone (GLuint& idx,
                          vec<float> centre,
                          vec<float> tip,
                          float ringoffset,
                          std::array<float, 3> col,
                          float r = 1.0f, int segments = 12)
        {
            // Cone is drawn as a base ring around a centre-of-the-base vertex, an
            // intermediate ring which is on the base ring, but has different normals, a
            // 'ring' around the tip (with suitable normals) and a 'tip' vertex

            vec<float> vbase = centre;
            vec<float> vtip = tip;
            vec<float> v = vtip - vbase;
            v.renormalize();

            // circle in a plane defined by a point and a normal
            vec<float> rand_vec;
            rand_vec.randomize();
            vec<float> inplane = rand_vec.cross(v);
            inplane.renormalize();
            vec<float> v_x_inplane = v.cross(inplane);

            // Push the central point of the start cap - this is at location vstart
            this->vertex_push (vbase, this->vertexPositions);
            this->vertex_push (-v, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            // Base ring with normals in direction -v
            for (int j = 0; j < segments; j++) {
                float t = j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = inplane * sin(t) * r + v_x_inplane * cos(t) * r;
                // Subtract the vector which makes this circle
                c = c + (c * ringoffset);
                this->vertex_push (vbase+c, this->vertexPositions);
                this->vertex_push (-v, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
            }

            // Intermediate ring of vertices around/aligned with the base ring with normals in direction c
            for (int j = 0; j < segments; j++) {
                float t = j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = inplane * sin(t) * r + v_x_inplane * cos(t) * r;
                c = c + (c * ringoffset);
                this->vertex_push (vbase+c, this->vertexPositions);
                c.renormalize();
                this->vertex_push (c, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
            }

            // Intermediate ring of vertices around the tip with normals direction c
            for (int j = 0; j < segments; j++) {
                float t = j * morph::mathconst<float>::two_pi/(float)segments;
                vec<float> c = inplane * sin(t) * r + v_x_inplane * cos(t) * r;
                c = c + (c * ringoffset);
                this->vertex_push (vtip, this->vertexPositions);
                c.renormalize();
                this->vertex_push (c, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
            }

            // Push tip vertex as the last vertex, normal is in direction v
            this->vertex_push (vtip, this->vertexPositions);
            this->vertex_push (v, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            // Number of vertices = segments*3 + 2.
            int nverts = segments*3 + 2;

            // After creating vertices, push all the indices.
            GLuint capMiddle = idx;
            GLuint capStartIdx = idx + 1;
            GLuint endMiddle = idx + (GLuint)nverts - 1;
            GLuint endStartIdx = capStartIdx /*+ segments*/;

            // Base of the cone
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (capMiddle);
                this->indices.push_back (capStartIdx + j);
                this->indices.push_back (capStartIdx + 1 + j);
            }
            // Last tri of base
            this->indices.push_back (capMiddle);
            this->indices.push_back (capStartIdx + segments - 1);
            this->indices.push_back (capStartIdx);

            // Middle sections
            for (int lsection = 0; lsection < 2; ++lsection) {
                capStartIdx = idx + 1 + lsection*segments;
                endStartIdx = capStartIdx + segments;
                for (int j = 0; j < segments; j++) {
                    // Triangle 1:
                    this->indices.push_back (capStartIdx + j);
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                    } else {
                        this->indices.push_back (capStartIdx + 1 + j);
                    }
                    this->indices.push_back (endStartIdx + j);
                    // Triangle 2:
                    this->indices.push_back (endStartIdx + j);
                    if (j == (segments-1)) {
                        this->indices.push_back (endStartIdx);
                    } else {
                        this->indices.push_back (endStartIdx + 1 + j);
                    }
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                    } else {
                        this->indices.push_back (capStartIdx + j + 1);
                    }
                }
            }

            // tip
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (endMiddle);
                this->indices.push_back (endStartIdx + j);
                this->indices.push_back (endStartIdx + 1 + j);
            }
            // Last triangle of tip
            this->indices.push_back (endMiddle);
            this->indices.push_back (endStartIdx + segments - 1);
            this->indices.push_back (endStartIdx);

            // Update idx
            idx += nverts;
        } // end of cone calculation

        //! Compute a line with a single colour
        void computeLine (GLuint& idx, vec<float> start, vec<float> end,
                          vec<float> _uz,
                          std::array<float, 3> col,
                          float w = 0.1f, float thickness = 0.01f, float shorten = 0.0f)
        {
            this->computeLine (idx, start, end, _uz, col, col, w, thickness, shorten);
        }

        /*!
         * Create a line from \a start to \a end, with width \a w and a colour which
         * transitions from the colour \a colStart to \a colEnd. The thickness of the
         * line in the z direction is \a thickness
         *
         * \param idx The index into the 'vertex array'
         * \param start The start of the tube
         * \param end The end of the tube
         * \param _uz Dirn of z (up) axis for end face of line. Should be normalized.
         * \param colStart The tube staring colour
         * \param colEnd The tube's ending colour
         * \param w width of line
         * \param thickness The thickness/depth of the line in uy direction
         * \param shorten An amount by which to shorten the length of the line at each end.
         */
        void computeLine (GLuint& idx, vec<float> start, vec<float> end,
                          vec<float> _uz,
                          std::array<float, 3> colStart, std::array<float, 3> colEnd,
                          float w = 0.1f, float thickness = 0.01f, float shorten = 0.0f)
        {
            // There are always 8 segments for this line object, 2 at each of 4 corners
            const int segments = 8;

            // The vector from start to end defines direction of the tube
            vec<float> vstart = start;
            vec<float> vend = end;
            vec<float> v = vend - vstart;
            v.renormalize();

            // If shorten is not 0, then modify vstart and vend
            if (shorten > 0.0f) {
                vstart = start + v * shorten;
                vend = end - v * shorten;
            }

            // vv is normal to v and uz
            vec<float> vv = v.cross(_uz);
            vv.renormalize();

            // Push the central point of the start cap - this is at location vstart
            this->vertex_push (vstart, this->vertexPositions);
            this->vertex_push (-v, this->vertexNormals);
            this->vertex_push (colStart, this->vertexColors);

            // Compute the 'face angles' that will give the correct width and thickness for the line
            std::array<float, 8> angles;
            float w_ = w * 0.5f;
            float d_ = thickness * 0.5f;
            float r = std::sqrt (w_ * w_ + d_ * d_);
            angles[0] = std::acos (w_ / r);
            angles[1] = angles[0];
            angles[2] = morph::mathconst<float>::pi - angles[0];
            angles[3] = angles[2];
            angles[4] = morph::mathconst<float>::pi + angles[0];
            angles[5] = angles[4];
            angles[6] = morph::mathconst<float>::two_pi - angles[0];
            angles[7] = angles[6];
            // The normals for the vertices around the line
            std::array<vec<float>, 8> norms = {vv, uz, uz, -vv, -vv, -uz, -uz, vv};

            // Start cap vertices (a triangle fan)
            for (int j = 0; j < segments; j++) {
                vec<float> c = uz * sin(angles[j]) * r + vv * cos(angles[j]) * r;
                this->vertex_push (vstart+c, this->vertexPositions);
                this->vertex_push (-v, this->vertexNormals);
                this->vertex_push (colStart, this->vertexColors);
            }

            // Intermediate, near start cap. Normals point outwards. Need Additional vertices
            for (int j = 0; j < segments; j++) {
                vec<float> c = uz * sin(angles[j]) * r + vv * cos(angles[j]) * r;
                this->vertex_push (vstart+c, this->vertexPositions);
                this->vertex_push (norms[j], this->vertexNormals);
                this->vertex_push (colStart, this->vertexColors);
            }

            // Intermediate, near end cap. Normals point in direction c
            for (int j = 0; j < segments; j++) {
                vec<float> c = uz * sin(angles[j]) * r + vv * cos(angles[j]) * r;
                this->vertex_push (vend+c, this->vertexPositions);
                this->vertex_push (norms[j], this->vertexNormals);
                this->vertex_push (colEnd, this->vertexColors);
            }

            // Bottom cap vertices
            for (int j = 0; j < segments; j++) {
                vec<float> c = uz * sin(angles[j]) * r + vv * cos(angles[j]) * r;
                this->vertex_push (vend+c, this->vertexPositions);
                this->vertex_push (v, this->vertexNormals);
                this->vertex_push (colEnd, this->vertexColors);
            }

            // Bottom cap. Push centre vertex as the last vertex.
            this->vertex_push (vend, this->vertexPositions);
            this->vertex_push (v, this->vertexNormals);
            this->vertex_push (colEnd, this->vertexColors);

            // Number of vertices = segments * 4 + 2.
            int nverts = (segments * 4) + 2;

            // After creating vertices, push all the indices.
            GLuint capMiddle = idx;
            GLuint capStartIdx = idx + 1;
            GLuint endMiddle = idx + (GLuint)nverts - 1;
            GLuint endStartIdx = capStartIdx + (3*segments);

            // Start cap indices
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (capMiddle);
                this->indices.push_back (capStartIdx + j);
                this->indices.push_back (capStartIdx + 1 + j);
            }
            // Last one
            this->indices.push_back (capMiddle);
            this->indices.push_back (capStartIdx + segments - 1);
            this->indices.push_back (capStartIdx);

            // Middle sections
            for (int lsection = 0; lsection < 3; ++lsection) {
                capStartIdx = idx + 1 + lsection*segments;
                endStartIdx = capStartIdx + segments;
                for (int j = 0; j < segments; j++) {
                    this->indices.push_back (capStartIdx + j);
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                    } else {
                        this->indices.push_back (capStartIdx + 1 + j);
                    }
                    this->indices.push_back (endStartIdx + j);
                    this->indices.push_back (endStartIdx + j);
                    if (j == (segments-1)) {
                        this->indices.push_back (endStartIdx);
                    } else {
                        this->indices.push_back (endStartIdx + 1 + j);
                    }
                    if (j == (segments-1)) {
                        this->indices.push_back (capStartIdx);
                    } else {
                        this->indices.push_back (capStartIdx + j + 1);
                    }
                }
            }

            // bottom cap
            for (int j = 0; j < segments-1; j++) {
                this->indices.push_back (endMiddle);
                this->indices.push_back (endStartIdx + j);
                this->indices.push_back (endStartIdx + 1 + j);
            }
            this->indices.push_back (endMiddle);
            this->indices.push_back (endStartIdx + segments - 1);
            this->indices.push_back (endStartIdx);

            // Update idx
            idx += nverts;
        } // end computeLine

        // Like computeLine, but this line has no thickness.
        void computeFlatLine (GLuint& idx, vec<float> start, vec<float> end,
                              vec<float> uz,
                              std::array<float, 3> col,
                              float w = 0.1f, float shorten = 0.0f)
        {
            // The vector from start to end defines direction of the tube
            vec<float> vstart = start;
            vec<float> vend = end;
            vec<float> v = vend - vstart;
            v.renormalize();

            // If shorten is not 0, then modify vstart and vend
            if (shorten > 0.0f) {
                vstart = start + v * shorten;
                vend = end - v * shorten;
            }

            // vv is normal to v and uz
            vec<float> vv = v.cross(uz);
            vv.renormalize();

            // corners of the line, and the start angle is determined from vv and w
            vec<float> ww = (vv*w*0.5f);
            vec<float> c1 = vstart + ww;
            vec<float> c2 = vstart - ww;
            vec<float> c3 = vend - ww;
            vec<float> c4 = vend + ww;

            this->vertex_push (c1, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c2, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c3, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c4, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            // Number of vertices = segments * 4 + 2.
            int nverts = 4;

            // After creating vertices, push all the indices.
            this->indices.push_back (idx);
            this->indices.push_back (idx+1);
            this->indices.push_back (idx+2);

            this->indices.push_back (idx);
            this->indices.push_back (idx+2);
            this->indices.push_back (idx+3);

            // Update idx
            idx += nverts;

        } // end computeFlatLine

        // Like computeFlatLine but with option to add rounded start/end caps (I lazily
        // draw a whole circle around start/end to achieve this, rather than figuring
        // out a semi-circle).
        void computeFlatLineRnd (GLuint& idx, vec<float> start, vec<float> end,
                                 vec<float> uz,
                                 std::array<float, 3> col,
                                 float w = 0.1f, float shorten = 0.0f, bool startcaps = true, bool endcaps = true)
        {
            // The vector from start to end defines direction of the tube
            vec<float> vstart = start;
            vec<float> vend = end;
            vec<float> v = vend - vstart;
            v.renormalize();

            // If shorten is not 0, then modify vstart and vend
            if (shorten > 0.0f) {
                vstart = start + v * shorten;
                vend = end - v * shorten;
            }

            // vv is normal to v and uz
            vec<float> vv = v.cross(uz);
            vv.renormalize();

            // corners of the line, and the start angle is determined from vv and w
            vec<float> ww = (vv*w*0.5f);
            vec<float> c1 = vstart + ww;
            vec<float> c2 = vstart - ww;
            vec<float> c3 = vend - ww;
            vec<float> c4 = vend + ww;

            int segments = 12;
            float r = 0.5f * w;
            size_t startvertices = 0;
            if (startcaps) {
                // Push the central point of the start cap - this is at location vstart
                this->vertex_push (vstart, this->vertexPositions);
                this->vertex_push (uz, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
                //std::cout << "centre point is index " << (idx + startvertices) << std::endl;
                ++startvertices;

                // Start cap vertices (a triangle fan)
                for (int j = 0; j < segments; j++) {
                    float t = j * morph::mathconst<float>::two_pi/(float)segments;
                    morph::vec<float> c = { sin(t) * r, cos(t) * r, 0 };
                    this->vertex_push (vstart+c, this->vertexPositions);
                    this->vertex_push (uz, this->vertexNormals);
                    this->vertex_push (col, this->vertexColors);
                    //std::cout << "capvertex is index " << (idx + startvertices) << std::endl;
                    ++startvertices;
                }
                //std::cout << "startvertices = " << startvertices << std::endl;
            }

            this->vertex_push (c1, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c2, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c3, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c4, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            size_t endvertices = 0;
            if (endcaps) {
                // Push the central point of the end cap - this is at location vend
                this->vertex_push (vend, this->vertexPositions);
                this->vertex_push (uz, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
                //std::cout << "centre point is index " << (idx + 4 + startvertices + endvertices) << std::endl;
                ++endvertices;

                // Start cap vertices (a triangle fan)
                for (int j = 0; j < segments; j++) {
                    float t = j * morph::mathconst<float>::two_pi/(float)segments;
                    morph::vec<float> c = { sin(t) * r, cos(t) * r, 0 };
                    this->vertex_push (vend+c, this->vertexPositions);
                    this->vertex_push (uz, this->vertexNormals);
                    this->vertex_push (col, this->vertexColors);
                    //std::cout << "capvertex is index " << (idx +  4 + startvertices + endvertices) << std::endl;
                    ++endvertices;
                }
                //std::cout << "endvertices = " << endvertices << std::endl;
            }

            // After creating vertices, push all the indices.

            if (startcaps) { // prolly startcaps, for flexibility
                GLuint topcap = idx;
                for (int j = 0; j < segments; j++) {
                    int inc1 = 1+j;
                    int inc2 = 1+((j+1)%segments);
                    //std::cout << "tri: " << topcap << "," << topcap+inc1 << "," << topcap+inc2 << std::endl;
                    this->indices.push_back (topcap);
                    this->indices.push_back (topcap+inc1);
                    this->indices.push_back (topcap+inc2);
                }
                idx += startvertices;
            }

            //std::cout << "Line tri idxs: " << idx << "," << idx+1 << "," << idx+2 << std::endl;
            //std::cout << "Line tri idxs: " << idx << "," << idx+2 << "," << idx+3 << std::endl;
            this->indices.push_back (idx);
            this->indices.push_back (idx+1);
            this->indices.push_back (idx+2);
            this->indices.push_back (idx);
            this->indices.push_back (idx+2);
            this->indices.push_back (idx+3);
            // Update idx
            idx += 4;

            if (endcaps) {
                GLuint botcap = idx;
                for (int j = 0; j < segments; j++) {
                    int inc1 = 1+j;
                    int inc2 = 1+((j+1)%segments);
                    //std::cout << "tri: " << botcap << "," << botcap+inc1 << "," << botcap+inc2 << std::endl;
                    this->indices.push_back (botcap);
                    this->indices.push_back (botcap+inc1);
                    this->indices.push_back (botcap+inc2);
                }
                idx += endvertices;
            }
            //std::cout << "end computeFlatLine(Caps): idx = " << idx << std::endl;
        } // end computeFlatLine

        //! Like computeFlatLine, but this line has no thickness and you can provide the
        //! previous and next data points so that this line, the previous line and the
        //! next line can line up perfectly without drawing a circular rounded 'end cap'!
        void computeFlatLine (GLuint& idx, vec<float> start, vec<float> end,
                              vec<float> prev, vec<float> next,
                              vec<float> uz,
                              std::array<float, 3> col,
                              float w = 0.1f)
        {
            // The vector from start to end defines direction of the tube
            vec<float> vstart = start;
            vec<float> vend = end;

            // line segment vectors
            vec<float> v = vend - vstart;
            v.renormalize();
            vec<float> vp = vstart - prev;
            vp.renormalize();
            vec<float> vn = next - vend;
            vn.renormalize();

            // vv is normal to v and uz
            vec<float> vv = v.cross(uz);
            vv.renormalize();
            vec<float> vvp = vp.cross(uz);
            vvp.renormalize();
            vec<float> vvn = vn.cross(uz);
            vvn.renormalize();

            // corners of the line, and the start angle is determined from vv and w
            vec<float> ww = ( (vv+vvp)*0.5f * w*0.5f );

            vec<float> c1 = vstart + ww;
            vec<float> c2 = vstart - ww;

            ww = ( (vv+vvn)*0.5f * w*0.5f );

            vec<float> c3 = vend - ww;
            vec<float> c4 = vend + ww;

            this->vertex_push (c1, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c2, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c3, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c4, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->indices.push_back (idx);
            this->indices.push_back (idx+1);
            this->indices.push_back (idx+2);

            this->indices.push_back (idx);
            this->indices.push_back (idx+2);
            this->indices.push_back (idx+3);

            // Update idx
            idx += 4;
        } // end computeFlatLine that joins perfectly

        //! Make a joined up line with previous.
        void computeFlatLineP (GLuint& idx, vec<float> start, vec<float> end,
                               vec<float> prev,
                               vec<float> uz,
                               std::array<float, 3> col,
                               float w = 0.1f)
        {
            // The vector from start to end defines direction of the tube
            vec<float> vstart = start;
            vec<float> vend = end;

            // line segment vectors
            vec<float> v = vend - vstart;
            v.renormalize();
            vec<float> vp = vstart - prev;
            vp.renormalize();

            // vv is normal to v and uz
            vec<float> vv = v.cross(uz);
            vv.renormalize();
            vec<float> vvp = vp.cross(uz);
            vvp.renormalize();

            // corners of the line, and the start angle is determined from vv and w
            vec<float> ww = ( (vv+vvp)*0.5f * w*0.5f );

            vec<float> c1 = vstart + ww;
            vec<float> c2 = vstart - ww;

            ww = (vv*w*0.5f);

            vec<float> c3 = vend - ww;
            vec<float> c4 = vend + ww;

            this->vertex_push (c1, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c2, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c3, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c4, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->indices.push_back (idx);
            this->indices.push_back (idx+1);
            this->indices.push_back (idx+2);

            this->indices.push_back (idx);
            this->indices.push_back (idx+2);
            this->indices.push_back (idx+3);

            // Update idx
            idx += 4;
        } // end computeFlatLine that joins perfectly with prev

        //! Flat line, joining up with next
        void computeFlatLineN (GLuint& idx, vec<float> start, vec<float> end,
                               vec<float> next,
                               vec<float> uz,
                               std::array<float, 3> col,
                               float w = 0.1f)
        {
            // The vector from start to end defines direction of the tube
            vec<float> vstart = start;
            vec<float> vend = end;

            // line segment vectors
            vec<float> v = vend - vstart;
            v.renormalize();
            vec<float> vn = next - vend;
            vn.renormalize();

            // vv is normal to v and uz
            vec<float> vv = v.cross(uz);
            vv.renormalize();
            vec<float> vvn = vn.cross(uz);
            vvn.renormalize();

            // corners of the line, and the start angle is determined from vv and w
            vec<float> ww = (vv*w*0.5f);

            vec<float> c1 = vstart + ww;
            vec<float> c2 = vstart - ww;

            ww = ( (vv+vvn)*0.5f * w*0.5f );

            vec<float> c3 = vend - ww;
            vec<float> c4 = vend + ww;

            this->vertex_push (c1, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c2, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c3, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->vertex_push (c4, this->vertexPositions);
            this->vertex_push (uz, this->vertexNormals);
            this->vertex_push (col, this->vertexColors);

            this->indices.push_back (idx);
            this->indices.push_back (idx+1);
            this->indices.push_back (idx+2);

            this->indices.push_back (idx);
            this->indices.push_back (idx+2);
            this->indices.push_back (idx+3);

            // Update idx
            idx += 4;
        } // end computeFlatLine that joins perfectly with next line

        // Like computeLine, but this line has no thickness and it's dashed.
        // dashlen: the length of dashes
        // gap prop: The proportion of dash length used for the gap
        void computeFlatDashedLine (GLuint& idx, vec<float> start, vec<float> end,
                                    vec<float> uz,
                                    std::array<float, 3> col,
                                    float w = 0.1f, float shorten = 0.0f,
                                    float dashlen = 0.1f, float gapprop = 0.3f)
        {
            if (dashlen == 0.0f) { return; }

            // The vector from start to end defines direction of the line
            vec<float> vstart = start;
            vec<float> vend = end;

            vec<float> v = vend - vstart;
            float linelen = v.length();
            v.renormalize();

            // If shorten is not 0, then modify vstart and vend
            if (shorten > 0.0f) {
                vstart = start + v * shorten;
                vend = end - v * shorten;
                linelen = v.length() - shorten*2.0f;
            }

            // vv is normal to v and uz
            vec<float> vv = v.cross(uz);
            vv.renormalize();

            // Loop, creating the dashes
            vec<float> dash_s = vstart;
            vec<float> dash_e = dash_s + v * dashlen;
            vec<float> dashes = dash_e - vstart;

            while (dashes.length() < linelen) {

                // corners of the line, and the start angle is determined from vv and w
                vec<float> ww = (vv*w*0.5f);
                vec<float> c1 = dash_s + ww;
                vec<float> c2 = dash_s - ww;
                vec<float> c3 = dash_e - ww;
                vec<float> c4 = dash_e + ww;

                this->vertex_push (c1, this->vertexPositions);
                this->vertex_push (uz, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);

                this->vertex_push (c2, this->vertexPositions);
                this->vertex_push (uz, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);

                this->vertex_push (c3, this->vertexPositions);
                this->vertex_push (uz, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);

                this->vertex_push (c4, this->vertexPositions);
                this->vertex_push (uz, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);

                // Number of vertices = segments * 4 + 2.
                int nverts = 4;

                // After creating vertices, push all the indices.
                this->indices.push_back (idx);
                this->indices.push_back (idx+1);
                this->indices.push_back (idx+2);

                this->indices.push_back (idx);
                this->indices.push_back (idx+2);
                this->indices.push_back (idx+3);

                // Update idx
                idx += nverts;

                // Next dash
                dash_s = dash_e + v * dashlen * gapprop;
                dash_e = dash_s + v * dashlen;
                dashes = dash_e - vstart;
            }

        } // end computeFlatDashedLine

        // Compute a flat line circle outline
        void computeFlatCircleLine (GLuint& idx, vec<float> centre, vec<float> norm, float radius,
                                    float linewidth, std::array<float, 3> col, int segments = 128)
        {
            // circle in a plane defined by a point (v0 = vstart or vend) and a normal
            // (v) can be found: Choose random vector vr. A vector inplane = vr ^ v. The
            // unit in-plane vector is inplane.normalise. Can now use that vector in the
            // plan to define a point on the circle. Note that this starting point on
            // the circle is at a random position, which means that this version of
            // computeTube is useful for tubes that have quite a few segments.
            vec<float> rand_vec;
            rand_vec.randomize();
            vec<float> inplane = rand_vec.cross(norm);
            inplane.renormalize();
            vec<float> norm_x_inplane = norm.cross(inplane);

            float half_lw = linewidth / 2.0f;
            float r_in = radius-half_lw;
            float r_out = radius+half_lw;
            // Inner ring at radius radius-linewidth/2 with normals in direction norm;
            // Outer ring at radius radius+linewidth/2 with normals also in direction norm
            for (int j = 0; j < segments; j++) {
                float t = j * morph::mathconst<float>::two_pi/static_cast<float>(segments);
                vec<float> c_in = inplane * sin(t) * r_in + norm_x_inplane * cos(t) * r_in;
                this->vertex_push (centre+c_in, this->vertexPositions);
                this->vertex_push (norm, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
                vec<float> c_out = inplane * sin(t) * r_out + norm_x_inplane * cos(t) * r_out;
                this->vertex_push (centre+c_out, this->vertexPositions);
                this->vertex_push (norm, this->vertexNormals);
                this->vertex_push (col, this->vertexColors);
            }
            // Added 2*segments vertices to vertexPositions

            // After creating vertices, push all the indices.
            for (int j = 0; j < segments; j++) {
                int jn = (segments + ((j+1) % segments)) % segments;
                this->indices.push_back (idx+(2*j));
                this->indices.push_back (idx+(2*jn));
                this->indices.push_back (idx+(2*jn+1));
                this->indices.push_back (idx+(2*j));
                this->indices.push_back (idx+(2*jn+1));
                this->indices.push_back (idx+(2*j+1));
            }
            idx += 2 * segments; // nverts

        } // end computeFlatCircle
    };

} // namespace morph
