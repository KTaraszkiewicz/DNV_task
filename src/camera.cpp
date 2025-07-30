#include "camera.h"
#include <QtMath>

// Default values that work well for most 3D scenes
const QVector3D Camera::DEFAULT_POSITION = QVector3D(0.0f, 0.0f, 5.0f);
const QVector3D Camera::DEFAULT_TARGET = QVector3D(0.0f, 0.0f, 0.0f);
const QVector3D Camera::DEFAULT_UP = QVector3D(0.0f, 1.0f, 0.0f);
const float Camera::DEFAULT_FOV = 45.0f;
const float Camera::DEFAULT_NEAR = 0.1f;
const float Camera::DEFAULT_FAR = 100.0f;

Camera::Camera()
    : position(DEFAULT_POSITION)
    , target(DEFAULT_TARGET)
    , up(DEFAULT_UP)
    , fov(DEFAULT_FOV)
    , aspect(1.0f)
    , nearPlane(DEFAULT_NEAR)
    , farPlane(DEFAULT_FAR)
    , dirty(true)  // Mark that matrices need to be recalculated
{
    updateVectors();
}

Camera::~Camera()
{
}

void Camera::setPosition(const QVector3D& pos)
{
    position = pos;
    updateVectors();  // Recalculate camera orientation vectors
    dirty = true;     // Mark matrices as needing update
}

void Camera::setTarget(const QVector3D& tgt)
{
    target = tgt;
    updateVectors();
    dirty = true;
}

void Camera::setUp(const QVector3D& upVector)
{
    up = upVector.normalized();  // Make sure the up vector has length 1
    updateVectors();
    dirty = true;
}

void Camera::lookAt(const QVector3D& eye, const QVector3D& center, const QVector3D& upVector)
{
    // Set all camera parameters at once - like positioning a real camera
    position = eye;
    target = center;
    up = upVector.normalized();
    updateVectors();
    dirty = true;
}

void Camera::setPerspective(float fieldOfView, float aspectRatio, float near, float far)
{
    // Configure how the 3D world is projected onto the 2D screen
    fov = fieldOfView;      // How wide the camera's view is (in degrees)
    aspect = aspectRatio;   // Width/height ratio of the screen
    nearPlane = near;       // Closest distance the camera can see
    farPlane = far;         // Furthest distance the camera can see
    dirty = true;
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float near, float far)
{
    // Set up orthographic projection (parallel lines stay parallel)
    // This is a basic implementation - mainly stores the clipping planes
    nearPlane = near;
    farPlane = far;
    dirty = true;
}

void Camera::translate(const QVector3D& delta)
{
    // Move both camera and target by the same amount to maintain view direction
    position += delta;
    target += delta;
    dirty = true;
}

void Camera::rotate(float pitch, float yaw, float roll)
{
    // Rotate the camera around its local axes
    QMatrix4x4 rotation;
    rotation.rotate(pitch, right);    // Up/down rotation
    rotation.rotate(yaw, up);         // Left/right rotation  
    rotation.rotate(roll, forward);   // Tilt rotation
    
    // Apply the rotation to the view direction
    QVector3D direction = target - position;
    direction = rotation * direction;
    target = position + direction;
    
    updateVectors();
    dirty = true;
}

void Camera::zoom(float factor)
{
    // Move the camera closer to or further from the target
    QVector3D direction = target - position;
    float distance = direction.length();
    
    // Don't let the camera get too close or too far away
    distance *= factor;
    distance = qMax(0.1f, qMin(100.0f, distance));
    
    direction.normalize();
    position = target - direction * distance;
    
    updateVectors();
    dirty = true;
}

void Camera::orbit(float horizontal, float vertical)
{
    // Rotate around the target point like a satellite orbiting Earth
    QVector3D direction = position - target;
    float radius = direction.length();  // Keep same distance from target
    
    // Convert to spherical coordinates (like longitude/latitude)
    float theta = atan2f(direction.x(), direction.z());  // Horizontal angle
    float phi = asinf(direction.y() / radius);           // Vertical angle
    
    // Apply the rotation
    theta += qDegreesToRadians(horizontal);
    phi += qDegreesToRadians(vertical);
    
    // Prevent the camera from flipping upside down
    phi = qMax(-M_PI/2.0f + 0.1f, qMin(M_PI/2.0f - 0.1f, phi));
    
    // Convert back to normal 3D coordinates
    direction.setX(radius * cosf(phi) * sinf(theta));
    direction.setY(radius * sinf(phi));
    direction.setZ(radius * cosf(phi) * cosf(theta));
    
    position = target + direction;
    updateVectors();
    dirty = true;
}

void Camera::pan(float x, float y)
{
    // Move both camera and target sideways/up-down relative to current view
    QVector3D offset = right * x + up * y;
    position += offset;
    target += offset;
    dirty = true;
}

void Camera::dolly(float distance)
{
    // Move the camera forward/backward along its viewing direction
    QVector3D direction = (target - position).normalized();
    position += direction * distance;
    dirty = true;
}

QMatrix4x4 Camera::getViewMatrix() const
{
    if (dirty) {
        updateMatrices();  // Recalculate if needed
    }
    return viewMatrix;
}

QMatrix4x4 Camera::getProjectionMatrix() const
{
    if (dirty) {
        updateMatrices();
    }
    return projectionMatrix;
}

QMatrix4x4 Camera::getViewProjectionMatrix() const
{
    // Combined matrix that transforms from world space to screen space
    return getProjectionMatrix() * getViewMatrix();
}

void Camera::reset()
{
    // Put everything back to starting values
    position = DEFAULT_POSITION;
    target = DEFAULT_TARGET;
    up = DEFAULT_UP;
    fov = DEFAULT_FOV;
    nearPlane = DEFAULT_NEAR;
    farPlane = DEFAULT_FAR;
    updateVectors();
    dirty = true;
}

void Camera::updateVectors()
{
    // Calculate the three vectors that define camera orientation
    
    // Forward vector points from camera toward target
    forward = (target - position).normalized();
    
    // Right vector is perpendicular to both forward and up
    right = QVector3D::crossProduct(forward, up).normalized();
    
    // Recalculate up vector to ensure it's perpendicular to the other two
    up = QVector3D::crossProduct(right, forward).normalized();
}

void Camera::updateMatrices() const
{
    // Create the view matrix (transforms world coordinates to camera coordinates)
    viewMatrix.setToIdentity();
    viewMatrix.lookAt(position, target, up);
    
    // Create the projection matrix (transforms camera coordinates to screen coordinates)
    projectionMatrix.setToIdentity();
    projectionMatrix.perspective(fov, aspect, nearPlane, farPlane);
    
    dirty = false;  // Mark as up-to-date
}