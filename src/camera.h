#ifndef CAMERA_H
#define CAMERA_H

#include <QVector3D>
#include <QMatrix4x4>

class Camera
{

public:
    Camera();
    ~Camera();

    // Camera controls
    void setPosition(const QVector3D& position);
    void setTarget(const QVector3D& target);
    void setUp(const QVector3D& up);

    void lookAt(const QVector3D& eye, const QVector3D& center, const QVector3D& up);
    void setPerspective(float fov, float aspect, float near, float far);
    void setOrthographic(float left, float right, float bottom, float top, float near, float far);

    // Movement
    void translate(const QVector3D& delta);
    void rotate(float pitch, float yaw, float roll);
    void zoom(float factor);

    // Orbit controls
    void orbit(float horizontal, float vertical);
    void pan(float x, float y);
    void dolly(float distance);

    // Arcball rotation (quaternion-based)
    void arcballRotate(const QVector3D& va, const QVector3D& vb, float t = 1.0f);

    // Smooth interpolation
    void smoothMove(const QVector3D& targetPosition, const QVector3D& targetTarget, const QVector3D& targetUp, float t);
    void smoothRotate(const QVector3D& axis, float angle, float t);

    // Matrix getters
    QMatrix4x4 getViewMatrix() const;
    QMatrix4x4 getProjectionMatrix() const;
    QMatrix4x4 getViewProjectionMatrix() const;

    // Property getters
    QVector3D getPosition() const { return position; }
    QVector3D getTarget() const { return target; }
    QVector3D getUp() const { return up; }
    QVector3D getForward() const { return forward; }
    QVector3D getRight() const { return right; }

    float getFov() const { return fov; }
    float getAspect() const { return aspect; }
    float getNear() const { return nearPlane; }
    float getFar() const { return farPlane; }

    // State
    void reset();
    bool isDirty() const { return dirty; }
    void markClean() { dirty = false; }

private:
    void updateVectors();
    void updateMatrices() const;
    
    // Camera vectors
    QVector3D position;
    QVector3D target;
    QVector3D up;
    QVector3D forward;
    QVector3D right;
    
    // Projection parameters
    float fov;          // Field of view in degrees
    float aspect;       // Aspect ratio
    float nearPlane;    // Near clipping plane
    float farPlane;     // Far clipping plane
    
    // Matrices
    mutable QMatrix4x4 viewMatrix;
    mutable QMatrix4x4 projectionMatrix;
    mutable bool dirty;
    
    // Default values
    static const QVector3D DEFAULT_POSITION;
    static const QVector3D DEFAULT_TARGET;
    static const QVector3D DEFAULT_UP;
    static const float DEFAULT_FOV;
    static const float DEFAULT_NEAR;
    static const float DEFAULT_FAR;
};

#endif // CAMERA_H