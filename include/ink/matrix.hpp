#pragma once

/**
 * @file matrix.hpp
 * @brief 2D affine transformation matrix.
 */

#include "ink/types.hpp"
#include <algorithm>
#include <cmath>

namespace ink {

/**
 * @brief 3x3 affine transformation matrix (stored as 6 floats).
 *
 * Represents the matrix:
 *   | scaleX  skewX   transX |
 *   | skewY   scaleY  transY |
 *   | 0       0       1      |
 *
 * Points are transformed as:
 *   x' = scaleX * x + skewX * y + transX
 *   y' = skewY  * x + scaleY * y + transY
 */
struct Matrix {
    f32 scaleX = 1, skewY  = 0;
    f32 skewX  = 0, scaleY = 1;
    f32 transX = 0, transY = 0;

    /// @brief Create an identity matrix.
    static Matrix Identity() { return {}; }

    /// @brief Create a translation matrix.
    static Matrix Translate(f32 dx, f32 dy) {
        Matrix m;
        m.transX = dx;
        m.transY = dy;
        return m;
    }

    /// @brief Create a scaling matrix (from origin).
    static Matrix Scale(f32 sx, f32 sy) {
        Matrix m;
        m.scaleX = sx;
        m.scaleY = sy;
        return m;
    }

    /// @brief Create a rotation matrix (radians, counter-clockwise around origin).
    static Matrix Rotate(f32 radians) {
        f32 c = std::cos(radians);
        f32 s = std::sin(radians);
        Matrix m;
        m.scaleX = c;  m.skewX = -s;
        m.skewY  = s;  m.scaleY = c;
        return m;
    }

    /// @brief Create a rotation matrix around a pivot point.
    static Matrix RotateAround(f32 radians, f32 cx, f32 cy) {
        return Translate(cx, cy) * Rotate(radians) * Translate(-cx, -cy);
    }

    /// @brief Concatenate: this * other (apply other first, then this).
    Matrix operator*(const Matrix& o) const {
        Matrix r;
        r.scaleX = scaleX * o.scaleX + skewX  * o.skewY;
        r.skewX  = scaleX * o.skewX  + skewX  * o.scaleY;
        r.transX = scaleX * o.transX + skewX  * o.transY + transX;
        r.skewY  = skewY  * o.scaleX + scaleY * o.skewY;
        r.scaleY = skewY  * o.skewX  + scaleY * o.scaleY;
        r.transY = skewY  * o.transX + scaleY * o.transY + transY;
        return r;
    }

    Matrix& operator*=(const Matrix& o) { *this = *this * o; return *this; }

    /// @brief Transform a point through this matrix.
    Point mapPoint(Point p) const {
        return {scaleX * p.x + skewX * p.y + transX,
                skewY  * p.x + scaleY * p.y + transY};
    }

    /// @brief Transform a rectangle, returning the axis-aligned bounding box.
    Rect mapRect(Rect r) const {
        Point p0 = mapPoint({r.x, r.y});
        Point p1 = mapPoint({r.x + r.w, r.y});
        Point p2 = mapPoint({r.x, r.y + r.h});
        Point p3 = mapPoint({r.x + r.w, r.y + r.h});

        f32 minX = std::min({p0.x, p1.x, p2.x, p3.x});
        f32 minY = std::min({p0.y, p1.y, p2.y, p3.y});
        f32 maxX = std::max({p0.x, p1.x, p2.x, p3.x});
        f32 maxY = std::max({p0.y, p1.y, p2.y, p3.y});
        return {minX, minY, maxX - minX, maxY - minY};
    }

    /// @brief Compute the inverse matrix. Returns identity if not invertible.
    /// @param ok If non-null, set to true on success, false if not invertible.
    Matrix inverted(bool* ok = nullptr) const {
        f32 det = scaleX * scaleY - skewX * skewY;
        if (det == 0.0f || !std::isfinite(det)) {
            if (ok) *ok = false;
            return Identity();
        }
        if (ok) *ok = true;
        f32 invDet = 1.0f / det;
        Matrix r;
        r.scaleX =  scaleY * invDet;
        r.skewX  = -skewX  * invDet;
        r.transX = (skewX * transY - scaleY * transX) * invDet;
        r.skewY  = -skewY  * invDet;
        r.scaleY =  scaleX * invDet;
        r.transY = (skewY * transX - scaleX * transY) * invDet;
        return r;
    }

    /// @brief Check if this is the identity matrix.
    bool isIdentity() const {
        return scaleX == 1 && skewY == 0 && skewX == 0 &&
               scaleY == 1 && transX == 0 && transY == 0;
    }

    /// @brief Check if this is translate-only (no rotation/scale/skew).
    bool isTranslateOnly() const {
        return scaleX == 1 && skewY == 0 && skewX == 0 && scaleY == 1;
    }

    /// @brief Check if this is scale+translate only (no rotation/skew).
    bool isScaleTranslateOnly() const {
        return skewX == 0 && skewY == 0;
    }

    bool operator==(const Matrix& o) const {
        return scaleX == o.scaleX && skewY == o.skewY &&
               skewX  == o.skewX  && scaleY == o.scaleY &&
               transX == o.transX && transY == o.transY;
    }

    bool operator!=(const Matrix& o) const { return !(*this == o); }
};

} // namespace ink
