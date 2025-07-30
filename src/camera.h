#ifndef CAMERA_H
#define CAMERA_H

#include <QVector3D>
#include <QMatrix4x4>

// This class represents a 3D camera that can look around and move through 3D space
// Think of it like a real camera - it has a position, looks at something, and can move around
class Camera
{

public:
    Camera();
    ~Camera();

    // Basic camera setup functions
    void setPosition(const QVector3D& position);    // Where the camera is located
    void setTarget(const QVector3D& target);        // What the camera is looking at
    void setUp(const QVector3D& up);                // Which direction is "up" for the camera

    // All-in-one camera setup (like setting up a real camera on a tripod)
    void lookAt(const QVector3D& eye, const QVector3D& center, const QVector3D& up);
    
    // How the camera projects the 3D world onto a 2D screen
    void setPerspective(float fov, float aspect, float near, float far);
    void setOrthographic(float left, float right, float bottom, float top, float near, float far);

    // Movement functions
    void translate(const QVector3D& delta);          // Move camera and target together
    void rotate(float pitch, float yaw, float roll); // Rotate camera around its axes
    void zoom(float factor);                         // Move closer/further from target

    // Special movement modes (common in 3D applications)
    void orbit(float horizontal, float vertical);    // Rotate around the target point
    void pan(float x, float y);                     // Slide sideways/up-down
    void dolly(float distance);                     // Move forward/backward

    // Advanced rotation methods (not implemented yet)
    void arcballRotate(const QVector3D& va, const QVector3D& vb, float t = 1.0f);

    // Smooth animation methods (not implemented yet)
    void smoothMove(const QVector3D& targetPosition, const QVector3D& targetTarget, const QVector3D& targetUp, float t);
    void smoothRotate(const QVector3D& axis, float angle, float t);

    // Get the matrices needed for rendering
    QMatrix4x4 getViewMatrix() const;           // Transforms world to camera space
    QMatrix4x4 getProjectionMatrix() const;    // Transforms camera space to screen space  
    QMatrix4x4 getViewProjectionMatrix() const; // Combined transformation

    // Get current camera properties
    QVector3D getPosition() const { return position; }
    QVector3D getTarget() const { return target; }
    QVector3D getUp() const { return up; }
    QVector3D getForward() const { return forward; }
    QVector3D getRight() const { return right; }

    float getFov() const { return fov; }
    float getAspect() const { return aspect; }
    float getNear() const { return nearPlane; }
    float getFar() const { return farPlane; }

    // Utility functions
    void reset();                               // Return to default settings
    bool isDirty() const { return dirty; }      // Check if matrices need updating
    void markClean() { dirty = false; }         // Mark matrices as up-to-date

private:
    // Internal functions
    void updateVectors();      // Recalculate forward, right, up vectors
    void updateMatrices() const; // Recalculate view and projection matrices
    
    // Camera position and orientation
    QVector3D position;  // Where the camera is in 3D space
    QVector3D target;    // What point the camera is looking at
    QVector3D up;        // Which direction is "up" for the camera
    QVector3D forward;   // Direction the camera is facing
    QVector3D right;     // Direction to the right of the camera
    
    // Projection settings (how 3D gets flattened to 2D)
public:
    float aspect;        // Screen width/height ratio (public so GLWidget can set it)
private:
    float fov;           // Field of view angle in degrees (how wide the view is)
    float nearPlane;     // Closest distance the camera can see
    float farPlane;      // Furthest distance the camera can see
    
    // Transformation matrices (calculated from the above values)
    mutable QMatrix4x4 viewMatrix;       // World-to-camera transformation
    mutable QMatrix4x4 projectionMatrix; // Camera-to-screen transformation
    mutable bool dirty;                   // True when matrices need recalculating
    
    // Default starting values
    static const QVector3D DEFAULT_POSITION;
    static const QVector3D DEFAULT_TARGET;
    static const QVector3D DEFAULT_UP;
    static const float DEFAULT_FOV;
    static const float DEFAULT_NEAR;
    static const float DEFAULT_FAR;
};

#endif // CAMERA_H