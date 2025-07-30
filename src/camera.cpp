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
    direction = rotation.map(direction);
    target = position + direction;
    
    updateVectors();
    dirty = true;
}

void Camera::zoom(float factor)
{
    qDebug() << "Camera::zoom: Factor" << factor << "Current position" << position << "Target" << target;
    
    QVector3D direction = target - position;
    float distance = direction.length();
    
    qDebug() << "Camera::zoom: Current distance" << distance;
    
    // Prevent division by zero and invalid operations
    if (distance < 0.001f) {
        qWarning() << "Camera::zoom: Distance too small, resetting to safe position";
        position = target + QVector3D(0.0f, 0.0f, 1.0f);
        updateVectors();
        dirty = true;
        return;
    }
    
    // Clamp zoom to reasonable limits
    distance *= factor;
    distance = qMax(0.1f, qMin(100.0f, distance));
    
    qDebug() << "Camera::zoom: New distance" << distance;
    
    // Normalize BEFORE checking for validity
    direction.normalize();
    
    // Validate the normalized direction
    if (!qIsFinite(direction.x()) || !qIsFinite(direction.y()) || !qIsFinite(direction.z()) || 
        direction.length() < 0.001f) {
        qWarning() << "Camera::zoom: Invalid direction after normalization, resetting";
        direction = QVector3D(0.0f, 0.0f, 1.0f);
    }
    
    position = target - direction * distance;
    
    // Validate final position
    if (!qIsFinite(position.x()) || !qIsFinite(position.y()) || !qIsFinite(position.z())) {
        qWarning() << "Camera::zoom: Invalid position calculated, resetting";
        position = target + QVector3D(0.0f, 0.0f, distance);
    }
    
    qDebug() << "Camera::zoom: New position" << position;
    
    updateVectors();
    dirty = true;
}

void Camera::orbit(float horizontal, float vertical)
{
    QVector3D direction = position - target;
    float radius = direction.length();
    
    // Prevent division by zero
    if (radius < 0.001f) {
        radius = 5.0f; // Default distance
        direction = QVector3D(0.0f, 0.0f, radius);
    }
    
    // Convert to spherical coordinates
    float theta = atan2f(direction.x(), direction.z());
    float phi = asinf(qBound(-1.0f, direction.y() / radius, 1.0f)); // Clamp to prevent NaN
    
    // Apply rotations
    theta += qDegreesToRadians(horizontal);
    phi += qDegreesToRadians(vertical);
    
    // Clamp phi to avoid gimbal lock and invalid values
    phi = qMax(-M_PI/2.0f + 0.1f, qMin(M_PI/2.0f - 0.1f, phi));
    
    // Convert back to Cartesian
    float cosTheta = cosf(theta);
    float sinTheta = sinf(theta);
    float cosPhi = cosf(phi);
    float sinPhi = sinf(phi);
    
    direction.setX(radius * cosPhi * sinTheta);
    direction.setY(radius * sinPhi);
    direction.setZ(radius * cosPhi * cosTheta);
    
    // Validate the new direction
    if (!qIsFinite(direction.x()) || !qIsFinite(direction.y()) || !qIsFinite(direction.z())) {
        direction = QVector3D(0.0f, 0.0f, radius); // Fallback
    }
    
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

void Camera::arcballRotate(const QVector3D& va, const QVector3D& vb, float t)
{
    // Arcball rotation using quaternions
    QVector3D axis = QVector3D::crossProduct(va, vb);
    float dot = QVector3D::dotProduct(va, vb);
    
    // Avoid division by zero
    if (axis.length() < 0.001f || qAbs(dot) > 0.999f) {
        return; // Vectors are too similar
    }
    
    float angle = acos(qBound(-1.0f, dot, 1.0f));
    
    // Apply rotation around the target
    QMatrix4x4 rotation;
    rotation.rotate(qRadiansToDegrees(angle * t), axis);
    
    QVector3D direction = position - target;
    direction = rotation.map(direction);
    position = target + direction;
    
    updateVectors();
    dirty = true;
}

void Camera::smoothMove(const QVector3D& targetPosition, const QVector3D& targetTarget, const QVector3D& targetUp, float t)
{
    // Linear interpolation between current and target values
    position = position * (1.0f - t) + targetPosition * t;
    target = target * (1.0f - t) + targetTarget * t;
    up = (up * (1.0f - t) + targetUp * t).normalized();
    
    updateVectors();
    dirty = true;
}

void Camera::smoothRotate(const QVector3D& axis, float angle, float t)
{
    // Smooth rotation around the target
    QMatrix4x4 rotation;
    rotation.rotate(qRadiansToDegrees(angle * t), axis);
    
    QVector3D direction = position - target;
    direction = rotation.map(direction);
    position = target + direction;
    
    updateVectors();
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
    // Validate input values first
    if (!qIsFinite(position.x()) || !qIsFinite(position.y()) || !qIsFinite(position.z()) ||
        !qIsFinite(target.x()) || !qIsFinite(target.y()) || !qIsFinite(target.z()) ||
        !qIsFinite(up.x()) || !qIsFinite(up.y()) || !qIsFinite(up.z())) {
        qWarning() << "Camera::updateVectors: Invalid input values detected, resetting to defaults";
        position = DEFAULT_POSITION;
        target = DEFAULT_TARGET;
        up = DEFAULT_UP;
    }
    
    // Calculate the forward vector
    forward = (target - position).normalized();
    
    // Validate forward vector
    if (forward.length() < 0.001f || !qIsFinite(forward.x()) || !qIsFinite(forward.y()) || !qIsFinite(forward.z())) {
        qWarning() << "Camera::updateVectors: Invalid forward vector, using default";
        forward = QVector3D(0.0f, 0.0f, -1.0f); // Default forward
    }
    
    // Ensure up vector is normalized and valid
    up = up.normalized();
    if (up.length() < 0.001f || !qIsFinite(up.x()) || !qIsFinite(up.y()) || !qIsFinite(up.z())) {
        up = DEFAULT_UP;
    }
    
    // Calculate the right vector
    right = QVector3D::crossProduct(forward, up).normalized();
    
    // If right vector is too small, up and forward are nearly parallel
    if (right.length() < 0.001f || !qIsFinite(right.x()) || !qIsFinite(right.y()) || !qIsFinite(right.z())) {
        // Choose a different up vector
        if (qAbs(forward.y()) < 0.9f) {
            right = QVector3D::crossProduct(forward, QVector3D(0.0f, 1.0f, 0.0f)).normalized();
        } else {
            right = QVector3D::crossProduct(forward, QVector3D(1.0f, 0.0f, 0.0f)).normalized();
        }
        
        // Final validation for right vector
        if (!qIsFinite(right.x()) || !qIsFinite(right.y()) || !qIsFinite(right.z()) || right.length() < 0.001f) {
            right = QVector3D(1.0f, 0.0f, 0.0f); // Default right
        }
    }
    
    // Recalculate the up vector to ensure orthogonality
    up = QVector3D::crossProduct(right, forward).normalized();
    
    // Final validation for up vector
    if (up.length() < 0.001f || !qIsFinite(up.x()) || !qIsFinite(up.y()) || !qIsFinite(up.z())) {
        up = QVector3D(0.0f, 1.0f, 0.0f); // Default up
    }
}

void Camera::updateMatrices() const
{
    // Update view matrix
    viewMatrix.setToIdentity();
    
    // Validate camera vectors before using them
    if (!qIsFinite(position.x()) || !qIsFinite(position.y()) || !qIsFinite(position.z()) ||
        !qIsFinite(target.x()) || !qIsFinite(target.y()) || !qIsFinite(target.z()) ||
        !qIsFinite(up.x()) || !qIsFinite(up.y()) || !qIsFinite(up.z())) {
        // Use default values if invalid
        viewMatrix.lookAt(DEFAULT_POSITION, DEFAULT_TARGET, DEFAULT_UP);
    } else {
        viewMatrix.lookAt(position, target, up);
    }
    
    // Update projection matrix
    projectionMatrix.setToIdentity();
    
    // Validate projection parameters
    if (fov <= 0.0f || fov >= 180.0f || aspect <= 0.0f || nearPlane <= 0.0f || farPlane <= nearPlane) {
        // Use default values if invalid
        projectionMatrix.perspective(DEFAULT_FOV, 1.0f, DEFAULT_NEAR, DEFAULT_FAR);
    } else {
        projectionMatrix.perspective(fov, aspect, nearPlane, farPlane);
    }
    
    dirty = false;
}