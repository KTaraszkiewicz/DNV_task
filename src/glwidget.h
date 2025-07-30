// OpenGL widget for rendering 3D models
#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QTimer>
#include <QVector>
#include <QVector3D>
#include "camera.h"

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLWidget(QWidget *parent = nullptr);
    ~GLWidget();

    // Public methods for external control
    void loadSTLFile(const QString &fileName);
    void resetCamera();
    void fitToWindow();
    void centerModel();
    void setWireframeMode(bool wireframe);
    void setLightingEnabled(bool enabled);
    void setZoom(float factor);
    void setRotationX(int degrees);
    void setRotationY(int degrees);
    void setRotationZ(int degrees);

signals:
    // Signals sent to parent window
    void frameRendered();                                                    // Emitted after each frame
    void fileLoaded(const QString &filename, int triangles, int vertices);   // Emitted when STL loads successfully

protected:
    // Qt OpenGL widget lifecycle methods
    void initializeGL() override;        // Called once to set up OpenGL
    void paintGL() override;             // Called every frame to draw
    void resizeGL(int width, int height) override;  // Called when widget resizes
    
    // Mouse and keyboard input handling
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // Setup and cleanup methods
    void cleanup();                                      // Clean up all OpenGL resources
    void cleanupModel();                                 // Clean up current model data
    bool setupShaders();                                 // Create and compile shaders
    void setupDefaultGeometry();                         // Create default cube geometry
    void setupVertexBuffer(const QVector<float>& vertexData);  // Upload vertex data to GPU
    void calculateBoundingBox(const QVector<float>& vertexData);  // Calculate model bounds
    
    // OpenGL objects (handles to GPU resources)
    QOpenGLShaderProgram *shaderProgram;    // Compiled shader program
    QOpenGLBuffer vertexBuffer;             // Vertex buffer object (VBO)
    QOpenGLBuffer indexBuffer;              // Element buffer object (EBO)
    QOpenGLVertexArrayObject vao;           // Vertex array object (VAO)
    
    // Transformation matrices (4x4 matrices for 3D math)
    QMatrix4x4 projectionMatrix;   // Camera projection (perspective/orthographic)
    QMatrix4x4 viewMatrix;         // Camera position and orientation
    QMatrix4x4 modelMatrix;        // Object position, rotation, scale
    
    // View control variables
    float zoomFactor;                     // Scale multiplier for model
    float rotationX, rotationY, rotationZ;  // Rotation angles in degrees
    bool wireframeMode;                   // Draw wireframe instead of filled triangles
    bool lightingEnabled;                 // Enable/disable lighting calculations
    
    // Mouse interaction state
    QPoint lastMousePos;        // Previous mouse position for delta calculation
    bool mousePressed;          // Is any mouse button currently pressed
    Qt::MouseButton mouseButton;  // Which mouse button is pressed
    Camera *camera;             // Camera object for view control
    
    // Current model data
    QVector<unsigned int> indices;  // Index buffer for vertex reuse
    int triangleCount;              // Number of triangles in current model
    bool hasModel;                  // Is a model currently loaded (vs default cube)
    
    // Model bounding box data (for camera positioning)
    QVector3D modelMin;       // Minimum coordinates of model
    QVector3D modelMax;       // Maximum coordinates of model  
    QVector3D modelCenter;    // Center point of model
    float modelRadius;        // Distance from center to furthest point
    bool boundingBoxValid;    // Is bounding box data valid
    
    // Rendering control
    QTimer renderTimer;       // Timer for continuous rendering (~60 FPS)
    
    // State tracking
    bool isInitialized;       // Has OpenGL been properly initialized

    // Default material color for rendered objects
    QVector3D defaultColor;
};

#endif // GLWIDGET_H