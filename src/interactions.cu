#include "interactions.h"

#include "utilities.h"

#include <thrust/random.h>

// is this in world coordinates?
__host__ __device__ glm::vec3 calculateRandomDirectionInHemisphere(
    glm::vec3 normal,
    thrust::default_random_engine &rng)
{
    thrust::uniform_real_distribution<float> u01(0, 1);

    float up = sqrt(u01(rng)); // cos(theta)
    float over = sqrt(1 - up * up); // sin(theta)
    float around = u01(rng) * TWO_PI;

    // Find a direction that is not the normal based off of whether or not the
    // normal's components are all equal to sqrt(1/3) or whether or not at
    // least one component is less than sqrt(1/3). Learned this trick from
    // Peter Kutz.

    glm::vec3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = glm::vec3(1, 0, 0);
    }
    else if (abs(normal.y) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = glm::vec3(0, 1, 0);
    }
    else
    {
        directionNotNormal = glm::vec3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    glm::vec3 perpendicularDirection1 =
        glm::normalize(glm::cross(normal, directionNotNormal));
    glm::vec3 perpendicularDirection2 =
        glm::normalize(glm::cross(normal, perpendicularDirection1));

    return up * normal
        + cos(around) * over * perpendicularDirection1
        + sin(around) * over * perpendicularDirection2;
}

__host__ __device__ glm::vec2 sampleUniformDiskConcentric(thrust::default_random_engine& rng)
{
    thrust::uniform_real_distribution<float> u01(-1, 1);

    float x = u01(rng);
    float y = u01(rng);
    float theta = 0;
    float r = 0;
    if (std::abs(x) > std::abs(y)) {
        r = x;
        theta = (PI / 4.0f) * (x / y);
    }
    else {
        r = y;
        theta = (PI / 2.0f) - ((PI / 4.0f) * (x / y));
    }

    return r * glm::vec2(std::cos(theta), std::sin(theta));
}

__host__ __device__ glm::vec3 sampleCosineHemisphere(
    glm::vec3 normal,
    thrust::default_random_engine& rng)
{
    glm::vec2 d = sampleUniformDiskConcentric(rng);
    float z = sqrt(1.0f - d.x * d.x - d.y * d.y);
    return glm::vec3(d.x, d.y, z);
}

__host__ __device__ void scatterRay(
    PathSegment & pathSegment,
    glm::vec3 intersect,
    glm::vec3 normal,
    const Material &m,
    thrust::default_random_engine &rng)
{
    // TODO: implement this.
    // A basic implementation of pure-diffuse shading will just call the
    // calculateRandomDirectionInHemisphere defined above.
    //pathSegment.ray.direction = glm::normalize(calculateRandomDirectionInHemisphere(normal, rng));

    // if reflective
    if (m.hasReflective > 0.0f)
    {
        pathSegment.ray.direction = glm::reflect(pathSegment.ray.direction, normal);
        pathSegment.pdf = 1.0f;
        pathSegment.color = m.color;
    }
    // if diffuse
    else {
        pathSegment.ray.direction = glm::normalize(calculateRandomDirectionInHemisphere(normal, rng));
        pathSegment.pdf = glm::dot(pathSegment.ray.direction, glm::normalize(normal)) / PI;
        pathSegment.color = m.color/PI;
    }

    pathSegment.ray.origin = EPSILON * normal + intersect;
}
