#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace SimCollision
{
    constexpr float kEps = 1e-6f;

    struct OBB
    {
        glm::vec3 center{ 0.0f };
        glm::mat3 orientation{ 1.0f }; // columns are local axes in world-space
        glm::vec3 halfExtents{ 0.5f };
    };

    struct Contact
    {
        bool hit = false;
        glm::vec3 normal{ 0.0f, 1.0f, 0.0f }; // points from A -> B
        float penetration = 0.0f;
    };

    inline glm::vec3 SafeNormalize(const glm::vec3& v, const glm::vec3& fallback = { 1,0,0 })
    {
        const float len2 = glm::dot(v, v);
        if (len2 <= kEps) return fallback;
        return v * (1.0f / std::sqrt(len2));
    }

    inline glm::mat3 OrthonormalizeColumns(const glm::mat3& m)
    {
        glm::vec3 x = glm::vec3(m[0]);
        glm::vec3 y = glm::vec3(m[1]);
        glm::vec3 z = glm::vec3(m[2]);

        x = SafeNormalize(x, { 1,0,0 });
        y = y - x * glm::dot(y, x);
        y = SafeNormalize(y, { 0,1,0 });
        z = glm::cross(x, y);
        z = SafeNormalize(z, { 0,0,1 });

        glm::mat3 out(1.0f);
        out[0] = x;
        out[1] = y;
        out[2] = z;
        return out;
    }

    inline glm::vec3 ClosestPointOnOBB(const glm::vec3& p, const OBB& obb)
    {
        // Transform point into OBB local space
        const glm::mat3 Rt = glm::transpose(obb.orientation);
        glm::vec3 d = p - obb.center;
        glm::vec3 local = Rt * d;

        local = glm::clamp(local, -obb.halfExtents, obb.halfExtents);

        // Back to world
        return obb.center + obb.orientation * local;
    }

    inline bool SphereVsOBB(const glm::vec3& sphereCenter, float radius, const OBB& obb, Contact& out)
    {
        out = {};
        const glm::vec3 closest = ClosestPointOnOBB(sphereCenter, obb);
        const glm::vec3 delta = sphereCenter - closest;
        const float dist2 = glm::dot(delta, delta);
        const float r2 = radius * radius;

        if (dist2 > r2) return false;

        const float dist = std::sqrt(std::max(dist2, kEps));
        out.hit = true;
        out.penetration = radius - dist;

        // If sphere center is inside / extremely close to closest point, choose a stable normal
        if (dist <= 1e-5f)
        {
            // pick axis of maximum penetration in OBB local space
            const glm::mat3 Rt = glm::transpose(obb.orientation);
            const glm::vec3 local = Rt * (sphereCenter - obb.center);
            const glm::vec3 absLocal = glm::abs(local);

            int axis = 0;
            if (absLocal.y > absLocal.x) axis = 1;
            if (absLocal.z > absLocal[axis]) axis = 2;

            glm::vec3 nLocal(0.0f);
            nLocal[axis] = (local[axis] >= 0.0f) ? 1.0f : -1.0f;
            out.normal = SafeNormalize(obb.orientation * nLocal, { 1,0,0 });
        }
        else
        {
            out.normal = delta / dist; // from box -> sphere
        }

        return true;
    }

    inline bool OBBVsOBB(const OBB& A, const OBB& B, Contact& out)
    {
        out = {};

        const glm::mat3 Au = OrthonormalizeColumns(A.orientation);
        const glm::mat3 Bu = OrthonormalizeColumns(B.orientation);

        const glm::vec3 aE = A.halfExtents;
        const glm::vec3 bE = B.halfExtents;

        // Rotation matrix expressing B in A’s frame
        glm::mat3 R(1.0f), AbsR(1.0f);
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                R[i][j] = glm::dot(Au[i], Bu[j]);
                AbsR[i][j] = std::abs(R[i][j]) + 1e-6f;
            }
        }

        // Translation vector t in A’s frame
        const glm::vec3 tWorld = B.center - A.center;
        glm::vec3 t(glm::dot(tWorld, Au[0]), glm::dot(tWorld, Au[1]), glm::dot(tWorld, Au[2]));

        float minOverlap = std::numeric_limits<float>::infinity();
        glm::vec3 bestAxisWorld = Au[0];

        auto considerAxis = [&](const glm::vec3& axisWorld, float overlap)
        {
            if (overlap < minOverlap)
            {
                minOverlap = overlap;
                bestAxisWorld = axisWorld;
            }
        };

        float ra = 0.0f, rb = 0.0f, dist = 0.0f, overlap = 0.0f;

        // Test A’s axes
        for (int i = 0; i < 3; ++i)
        {
            ra = aE[i];
            rb = bE[0] * AbsR[i][0] + bE[1] * AbsR[i][1] + bE[2] * AbsR[i][2];
            dist = std::abs(t[i]);
            if (dist > ra + rb) return false;
            overlap = (ra + rb) - dist;
            considerAxis(Au[i], overlap);
        }

        // Test B’s axes
        for (int j = 0; j < 3; ++j)
        {
            ra = aE[0] * AbsR[0][j] + aE[1] * AbsR[1][j] + aE[2] * AbsR[2][j];
            rb = bE[j];
            dist = std::abs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]);
            if (dist > ra + rb) return false;
            overlap = (ra + rb) - dist;
            considerAxis(Bu[j], overlap);
        }

        // Test cross products A[i] x B[j]
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                // Axis = A_i x B_j
                const glm::vec3 axisWorld = glm::cross(Au[i], Bu[j]);
                const float axisLen2 = glm::dot(axisWorld, axisWorld);
                if (axisLen2 <= 1e-10f) continue; // parallel -> skip

                const int i1 = (i + 1) % 3;
                const int i2 = (i + 2) % 3;
                const int j1 = (j + 1) % 3;
                const int j2 = (j + 2) % 3;

                ra = aE[i1] * AbsR[i2][j] + aE[i2] * AbsR[i1][j];
                rb = bE[j1] * AbsR[i][j2] + bE[j2] * AbsR[i][j1];

                dist = std::abs(t[i2] * R[i1][j] - t[i1] * R[i2][j]);
                if (dist > ra + rb) return false;

                overlap = (ra + rb) - dist;
                considerAxis(SafeNormalize(axisWorld), overlap);
            }
        }

        out.hit = true;
        out.penetration = std::max(0.0f, minOverlap);

        glm::vec3 n = SafeNormalize(bestAxisWorld, { 1,0,0 });
        if (glm::dot(n, B.center - A.center) < 0.0f) n = -n;
        out.normal = n;
        return true;
    }

    inline float SupportDistanceAlongNormal(const OBB& obb, const glm::vec3& nWorld)
    {
        const glm::vec3 nLocal = glm::transpose(obb.orientation) * nWorld;
        return glm::dot(glm::abs(nLocal), obb.halfExtents);
    }

    // ---------------------------------------------------------
    // ADD THIS TO THE BOTTOM OF CollisionUtil.h (Inside namespace SimCollision)
    // ---------------------------------------------------------

    inline bool SphereVsPlane(const glm::vec3& sphereCenter, float radius, const glm::vec3& planeNormal, const glm::vec3& planePoint, Contact& out)
    {
        out = {};
        float dist = glm::dot(sphereCenter - planePoint, planeNormal);

        if (dist > radius) return false;

        out.hit = true;
        out.normal = planeNormal; // Push away from the plane
        out.penetration = radius - dist;

        return true;
    }

    inline bool OBBVsPlane(const OBB& obb, const glm::vec3& planeNormal, const glm::vec3& planePoint, Contact& out)
    {
        out = {};
        // Use your existing function to find how far the OBB extends toward the plane
        float projectedRadius = SupportDistanceAlongNormal(obb, planeNormal);
        float dist = glm::dot(obb.center - planePoint, planeNormal);

        if (dist > projectedRadius) return false;

        out.hit = true;
        out.normal = planeNormal;
        out.penetration = projectedRadius - dist;

        return true;
    }

    // Helper for Capsule
    inline glm::vec3 ClosestPointOnLineSegment(const glm::vec3& a, const glm::vec3& b, const glm::vec3& p)
    {
        glm::vec3 ab = b - a;
        float t = glm::dot(p - a, ab) / glm::dot(ab, ab);
        t = glm::clamp(t, 0.0f, 1.0f);
        return a + t * ab;
    }

    inline bool SphereVsCapsule(const glm::vec3& sphereCenter, float sphereRadius,
        const glm::vec3& capA, const glm::vec3& capB, float capRadius,
        Contact& out)
    {
        out = {};
        glm::vec3 closestPt = ClosestPointOnLineSegment(capA, capB, sphereCenter);

        glm::vec3 delta = sphereCenter - closestPt;
        float dist2 = glm::dot(delta, delta);
        float radiusSum = sphereRadius + capRadius;

        if (dist2 > radiusSum * radiusSum) return false;

        float dist = std::sqrt(std::max(dist2, kEps));
        out.hit = true;
        out.normal = delta / dist;
        out.penetration = radiusSum - dist;

        return true;
    }
}