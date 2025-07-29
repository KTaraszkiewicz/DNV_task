#include "camera.h"
#include <QtMath>

// Static constants
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
    , dirty(true)
{
    updateVectors();
}

Camera::~Camera()
{
}

void Camera::setPosition(const QVector3D& pos)
{
    position = pos;
    updateVectors();
    dirty = true;
}

void Camera::setTarget(const QVector3D& tgt)
{
    target = tgt;
    updateVectors();
    dirty = true;
}

void Camera::setUp(const QVector3D& upVector)
{
    up = upVector.normalized();
    updateVectors();
    dirty = true;
}

void Camera::lookAt(const QVector3D& eye, const QVector3D& center, const QVector3D& upVector)
{
    position = eye;
    target = center;
    up = upVector.normalized();
    updateVectors();
    dirty = true;
}

void Camera::setPerspective(float fieldOfView, float aspectRatio, float near, float far)
{
    fov = fieldOfView;
    aspect = aspectRatio;
    nearPlane = near;
    farPlane = far;
    dirty = true;
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float near, float far)
{
    // For orthographic projection, we'll store the parameters differently
    // This is a simplified implementation
    nearPlane = near;
    farPlane = far;
    dirty = true;
}

void Camera::translate(const QVector3D& delta)
{
    position += delta;
    target += delta;
    dirty = true;
}

void Camera::rotate(float pitch, float yaw, float roll)
{
    // Create rotation matrix
    QMatrix4x4 rotation;
    rotation.rotate(pitch, right);
    rotation.rotate(yaw, up);
    rotation.rotate(roll, forward);
    
    // Apply rotation to the camera vectors
    QVector3D direction = target - position;
    direction = rotation * direction;
    target = position + direction;
    
    updateVectors();
    dirty = true;
}

void Camera::zoom(float factor)
{
    QVector3D direction = target - position;
    float distance = direction.length();
    
    // Clamp zoom to reasonable limits
    distance *= factor;
    distance = qMax(0.1f, qMin(100.0f, distance));
    
    direction.normalize();
    position = target - direction * distance;
    
    updateVectors();
    dirty = true;
}

void Camera::orbit(float horizontal, float vertical)
{
    QVector3D direction = position - target;
    float radius = direction.length();
    
    // Convert to spherical coordinates
    float theta = atan2f(direction.x(), direction.z());
    float phi = asinf(direction.y() / radius);
    
    // Apply rotations
    theta += qDegreesToRadians(horizontal);
    phi += qDegreesToRadians(vertical);
    
    // Clamp phi to avoid gimbal lock
    phi = qMax(-M_PI/2.0f + 0.1f, qMin(M_PI/2.0f - 0.1f, phi));
    
    // Convert back to Cartesian
    direction.setX(radius * cosf(phi) * sinf(theta));
    direction.setY(radius * sinf(phi));
    direction.setZ(radius * cosf(phi) * cosf(theta));
    
    position = target + direction;
    updateVectors();
    dirty = true;
}

void Camera::pan(float x, float y)
{
    QVector3D offset = right * x + up * y;
    position += offset;
    target += offset;
    dirty = true;
}

void Camera::dolly(float distance)
{
    QVector3D direction = (target - position).normalized();
    position += direction * distance;
    dirty = true;
}

QMatrix4x4 Camera::getViewMatrix() const
{
    if (dirty) {
        updateMatrices();
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
    return getProjectionMatrix() * getViewMatrix();
}

void Camera::reset()
{
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
    // Calculate the forward vector
    forward = (target - position).normalized();
    
    // Calculate the right vector
    right = QVector3D::crossProduct(forward, up).normalized();
    
    // Recalculate the up vector
    up = QVector3D::crossProduct(right, forward).normalized();
}

void Camera::updateMatrices() const
{
    // Update view matrix
    viewMatrix.setToIdentity();
    viewMatrix.lookAt(position, target, up);
    
    // Update projection matrix
    projectionMatrix.setToIdentity();
    projectionMatrix.perspective(fov, aspect, nearPlane, farPlane);
    
    dirty = false;
}