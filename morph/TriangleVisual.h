#pragma once

#include <morph/vec.h>
#include <morph/VisualModel.h>
#include <morph/mathconst.h>
#include <array>

namespace morph {

    //! This class creates the vertices for a simple triangle in a 3D scene.
    class TriangleVisual : public VisualModel
    {
    public:
        TriangleVisual() { this->mv_offset = {0.0, 0.0, 0.0}; }

        //! Initialise with offset, three coordinates and a single colour.
        TriangleVisual(const vec<float, 3> _offset,
                       const vec<float, 3> _coord1, const vec<float, 3> _coord2, const vec<float, 3> _coord3,
                       const std::array<float, 3> _col)
        {
            this->init (_offset, _coord1, _coord2, _coord3, _col);
        }

        virtual ~TriangleVisual () {}

        void init (const vec<float, 3> _offset,
                   const vec<float, 3> _coord1, const vec<float, 3> _coord2, const vec<float, 3> _coord3,
                   const std::array<float, 3> _col)
        {
            // Set up...
            this->mv_offset = _offset;
            this->viewmatrix.translate (this->mv_offset);
            this->coord1 = _coord1;
            this->coord2 = _coord2;
            this->coord3 = _coord3;
            this->col = _col;
        }

        //! Compute a triangle from 3 arbitrary corners
        void computeTriangle (GLuint& idx,
                              vec<float> c1, vec<float> c2, vec<float> c3,
                              std::array<float, 3> colr)
        {
            // v is the face normal
            vec<float> u1 = c1-c2;
            vec<float> u2 = c2-c3;
            vec<float> v = u1.cross(u2);
            v.renormalize();
            // Push corner vertices
            this->vertex_push (c1, this->vertexPositions);
            this->vertex_push (c2, this->vertexPositions);
            this->vertex_push (c3, this->vertexPositions);
            // Colours/normals
            for (size_t i = 0; i < 3; ++i) {
                this->vertex_push (colr, this->vertexColors);
                this->vertex_push (v, this->vertexNormals);
            }
            this->indices.push_back (idx++);
            this->indices.push_back (idx++);
            this->indices.push_back (idx++);
        }

        //! Initialize vertex buffer objects and vertex array object.
        void initializeVertices()
        {
            this->vertexPositions.clear();
            this->vertexNormals.clear();
            this->vertexColors.clear();
            this->indices.clear();

            // Draw a triangle. That's it.
            this->computeTriangle (this->idx, this->coord1, this->coord2, this->coord3, this->col);

            std::cout << "idx now has value: " << this->idx << std::endl;
            std::cout << "vertexPositions has size " <<  this->vertexPositions.size()<< std::endl;
        }

        //! The position of the vertices of the triangle
        vec<float, 3> coord1 = {0.0f, 0.0f, 0.0f};
        vec<float, 3> coord2 = {0.0f, 0.0f, 0.0f};
        vec<float, 3> coord3 = {0.0f, 0.0f, 0.0f};

        //! The colour of the triangle
        std::array<float, 3> col = {0.0f, 0.0f, 1.0f};
    };

} // namespace morph
