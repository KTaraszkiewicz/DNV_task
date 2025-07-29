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

    // Public interface methods that MainWindow expects
    void loadSTLFile(const QString &fileName);
    void resetCamera();
    void fitToWindow();
    void centerModel();  // New method to center on model
    void setWireframeMode(bool wireframe);
    void setLightingEnabled(bool enabled);
    void setZoom(float factor);
    void setRotationX(int degrees);
    void setRotationY(int degrees);
    void setRotationZ(int degrees);

signals:
    void frameRendered();
    void fileLoaded(const QString &filename, int triangles, int vertices);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void setupShaders();
    void setupDefaultGeometry();
    void setupVertexBuffer(const QVector<float>& vertexData);
    void calculateBoundingBox(const QVector<float>& vertexData);
    
    // OpenGL objects
    QOpenGLShaderProgram *shaderProgram;
    QOpenGLBuffer vertexBuffer;
    QOpenGLBuffer indexBuffer;
    QOpenGLVertexArrayObject vao;
    
    // Matrices
    QMatrix4x4 projectionMatrix;
    QMatrix4x4 viewMatrix;
    QMatrix4x4 modelMatrix;
    
    // Camera/view controls
    float zoomFactor;
    float rotationX, rotationY, rotationZ;
    bool wireframeMode;
    bool lightingEnabled;
    
    // Mouse interaction
    QPoint lastMousePos;
    bool mousePressed;
    Qt::MouseButton mouseButton;
    Camera *camera;
    
    // Model data
    QVector<unsigned int> indices;
    int triangleCount;
    bool hasModel;
    
    // Bounding box data
    QVector3D modelMin;
    QVector3D modelMax;
    QVector3D modelCenter;
    float modelRadius;
    bool boundingBoxValid;
    
    // Rendering
    QTimer renderTimer;
};

#endif // GLWIDGET_H