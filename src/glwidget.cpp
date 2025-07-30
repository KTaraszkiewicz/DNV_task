#include "glwidget.h"
#include "camera.h"
#include <QCursor>
#include <QFileInfo>
#include "stlloader.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QtMath>
#include <QDebug>
#include <QApplication>
#include <iostream>

// Convert mouse coordinates to 3D sphere coordinates (used for smooth rotation)
static QVector3D mapToArcball(int x, int y, int w, int h) {
    float nx = (2.0f * x - w) / w;
    float ny = (h - 2.0f * y) / h;
    float length2 = nx * nx + ny * ny;
    float z = length2 > 1.0f ? 0.0f : sqrtf(1.0f - length2);
    return QVector3D(nx, ny, z).normalized();
}

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , shaderProgram(nullptr)
    , zoomFactor(1.0f)
    , rotationX(0.0f)
    , rotationY(0.0f)
    , rotationZ(0.0f)
    , wireframeMode(false)
    , lightingEnabled(true)
    , mousePressed(false)
    , mouseButton(Qt::NoButton)
    , triangleCount(0)
    , hasModel(false)
    , boundingBoxValid(false)
    , camera(nullptr)
    , isInitialized(false)
{
    // Set OpenGL format before creating the widget
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling for better quality
    setFormat(format);
    
    // Set focus policy to receive key events
    setFocusPolicy(Qt::StrongFocus);
    
    // Camera will be created later when OpenGL context is ready
    
    // Set default material color to light gray
    defaultColor = QVector3D(0.8f, 0.8f, 0.8f);
}

GLWidget::~GLWidget()
{
    std::cout << "GLWidget: Destructor called" << std::endl;
    qDebug() << "GLWidget: Destructor called";
    
    // Stop timers and free up graphics card memory
    cleanup();
    
    std::cout << "GLWidget: Destructor complete" << std::endl;
    qDebug() << "GLWidget: Destructor complete";
}

void GLWidget::cleanup()
{
    std::cout << "GLWidget: Starting cleanup..." << std::endl;
    qDebug() << "GLWidget: Starting cleanup...";
    
    // Stop any timers first to prevent further updates
    renderTimer.stop();
    renderTimer.disconnect();
    
    // Make sure we have a valid context before cleanup
    if (context() && context()->isValid()) {
        makeCurrent();
        
        qDebug() << "GLWidget: Cleaning up OpenGL resources...";
        
        // Free up graphics card memory in the right order
        if (vao.isCreated()) {
            vao.destroy();
        }
        
        if (indexBuffer.isCreated()) {
            indexBuffer.destroy();
        }
        
        if (vertexBuffer.isCreated()) {
            vertexBuffer.destroy();
        }
        
        if (shaderProgram) {
            delete shaderProgram;
            shaderProgram = nullptr;
        }
        
        doneCurrent();
        qDebug() << "GLWidget: OpenGL resources cleaned up";
    }
    
    // Clean up camera
    if (camera) {
        delete camera;
        camera = nullptr;
        qDebug() << "GLWidget: Camera cleaned up";
    }
    
    isInitialized = false;
    qDebug() << "GLWidget: Cleanup complete";
}

void GLWidget::initializeGL()
{
    // Load OpenGL function pointers that we'll need
    initializeOpenGLFunctions();

    // Check OpenGL version
    QString glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    qDebug() << "OpenGL Version:" << glVersion;

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable back-face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Enable multisampling if available
    if (format().samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Set clear color
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    // Set up shaders
    if (!setupShaders()) {
        qCritical() << "Failed to setup shaders";
        return;
    }

    // Create camera now that we have OpenGL context
    camera = new Camera();
    camera->setPerspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);

    // Set up transformation matrices
    modelMatrix.setToIdentity();
    viewMatrix.setToIdentity();
    viewMatrix.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    // We're ready to start drawing
    isInitialized = true;
    
    // Create a default cube to show when no 3D model is loaded
    setupDefaultGeometry();

    // We don't use continuous rendering to save CPU/GPU resources
    // The display only updates when something changes (mouse, new model, etc.)
    // connect(&renderTimer, &QTimer::timeout, this, QOverload<>::of(&GLWidget::update));
    // renderTimer.start(33); // This would update 30 times per second - disabled
    
    qDebug() << "OpenGL initialized successfully";
}

void GLWidget::paintGL()
{
    // Make sure OpenGL is ready before we try to draw anything
    if (!isInitialized || !context() || !context()->isValid()) {
        qDebug() << "GLWidget::paintGL: Not initialized or invalid context";
        return;
    }
    
    qDebug() << "GLWidget::paintGL: Starting paint operation";

    try {
        static int frameCounter = 0;
        frameCounter++;
        
        // Print debug info every 100 frames so we can see what's happening
        if (frameCounter % 100 == 0) {
            qDebug() << "GLWidget::paintGL: Frame" << frameCounter << "- hasModel:" << hasModel << "triangleCount:" << triangleCount;
        }
        
        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if (!shaderProgram || !vao.isCreated()) {
            qDebug() << "Shader program or VAO not ready";
            return;
        }
        
        // Set wireframe mode
        if (wireframeMode) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.0f);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    
    // Use shader program
    shaderProgram->bind();

    // Update camera matrices with validation
    if (camera) {
        // Validate camera pointer and state before any operations
        try {
            // Test if camera object is accessible by calling a simple method
            camera->aspect = float(width()) / float(height() ? height() : 1);
            
            // Validate camera state before getting matrices
            QVector3D camPos = camera->getPosition();
            QVector3D camTarget = camera->getTarget();
            QVector3D camUp = camera->getUp();
            
            qDebug() << "Camera state - Pos:" << camPos << "Target:" << camTarget << "Up:" << camUp;
            
            // Check for invalid values
            bool validCamera = qIsFinite(camPos.x()) && qIsFinite(camPos.y()) && qIsFinite(camPos.z()) &&
                              qIsFinite(camTarget.x()) && qIsFinite(camTarget.y()) && qIsFinite(camTarget.z()) &&
                              qIsFinite(camUp.x()) && qIsFinite(camUp.y()) && qIsFinite(camUp.z());
            
            if (validCamera) {
                qDebug() << "Camera state is valid, getting matrices";
                viewMatrix = camera->getViewMatrix();
                projectionMatrix = camera->getProjectionMatrix();
                qDebug() << "Camera matrices obtained successfully";
            } else {
                qWarning() << "Invalid camera state detected, using fallback matrices";
                camera->reset(); // Reset to safe state
                viewMatrix = camera->getViewMatrix();
                projectionMatrix = camera->getProjectionMatrix();
            }
        } catch (const std::exception& e) {
            qCritical() << "Exception while accessing camera:" << e.what();
            // Use fallback matrices
            viewMatrix.setToIdentity();
            viewMatrix.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
            projectionMatrix.setToIdentity();
            projectionMatrix.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);
        } catch (...) {
            qCritical() << "Unknown exception while accessing camera";
            // Use fallback matrices
            viewMatrix.setToIdentity();
            viewMatrix.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
            projectionMatrix.setToIdentity();
            projectionMatrix.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);
        }
    }
    
    modelMatrix.setToIdentity();
    
    // Make sure zoom and rotation values are valid numbers before using them
    if (!qIsFinite(zoomFactor) || zoomFactor <= 0.0f) {
        qWarning() << "Invalid zoom factor detected:" << zoomFactor << "- resetting to 1.0";
        zoomFactor = 1.0f;
    }
    
    if (!qIsFinite(rotationX) || !qIsFinite(rotationY) || !qIsFinite(rotationZ)) {
        qWarning() << "Invalid rotation values detected - resetting";
        rotationX = rotationY = rotationZ = 0.0f;
    }
    
    modelMatrix.scale(zoomFactor);
    modelMatrix.rotate(rotationX, 1, 0, 0);
    modelMatrix.rotate(rotationY, 0, 1, 0);
    modelMatrix.rotate(rotationZ, 0, 0, 1);

    QMatrix4x4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    QMatrix4x4 normalMatrix = modelMatrix.inverted().transposed();

    // Check that our transformation matrices contain valid numbers
    bool validMatrices = true;
    for (int i = 0; i < 16; i++) {
        if (!qIsFinite(mvpMatrix.data()[i]) || !qIsFinite(normalMatrix.data()[i])) {
            validMatrices = false;
            break;
        }
    }
    
    if (!validMatrices) {
        qWarning() << "Invalid matrices detected, skipping frame";
        shaderProgram->release();
        return;
    }

    // Set shader uniforms
    try {
        shaderProgram->setUniformValue("u_mvpMatrix", mvpMatrix);
        shaderProgram->setUniformValue("u_modelMatrix", modelMatrix);
        shaderProgram->setUniformValue("u_color", defaultColor);
        shaderProgram->setUniformValue("u_viewMatrix", viewMatrix);
        shaderProgram->setUniformValue("u_normalMatrix", normalMatrix);

        // Lighting uniforms
        shaderProgram->setUniformValue("u_lightDir", QVector3D(-0.5f, -1.0f, -0.3f));
        shaderProgram->setUniformValue("u_lightColor", QVector3D(1.0f, 1.0f, 1.0f));
        
        if (camera) {
            try {
                QVector3D camPos = camera->getPosition();
                if (qIsFinite(camPos.x()) && qIsFinite(camPos.y()) && qIsFinite(camPos.z())) {
                    shaderProgram->setUniformValue("u_lightPos", camPos);
                    shaderProgram->setUniformValue("u_viewPos", camPos);
                } else {
                    qWarning() << "Invalid camera position, using default lighting position";
                    shaderProgram->setUniformValue("u_lightPos", QVector3D(0.0f, 0.0f, 5.0f));
                    shaderProgram->setUniformValue("u_viewPos", QVector3D(0.0f, 0.0f, 5.0f));
                }
            } catch (const std::exception& e) {
                qWarning() << "Exception accessing camera position:" << e.what();
                shaderProgram->setUniformValue("u_lightPos", QVector3D(0.0f, 0.0f, 5.0f));
                shaderProgram->setUniformValue("u_viewPos", QVector3D(0.0f, 0.0f, 5.0f));
            } catch (...) {
                qWarning() << "Unknown exception accessing camera position";
                shaderProgram->setUniformValue("u_lightPos", QVector3D(0.0f, 0.0f, 5.0f));
                shaderProgram->setUniformValue("u_viewPos", QVector3D(0.0f, 0.0f, 5.0f));
            }
        } else {
            shaderProgram->setUniformValue("u_lightPos", QVector3D(0.0f, 0.0f, 5.0f));
            shaderProgram->setUniformValue("u_viewPos", QVector3D(0.0f, 0.0f, 5.0f));
        }
        
        shaderProgram->setUniformValue("u_ambientStrength", 0.2f);
        shaderProgram->setUniformValue("u_specularStrength", 0.5f);
        shaderProgram->setUniformValue("u_shininess", 32.0f);

        // Choose colors: blue-gray for STL models, red for default cube
        QVector3D materialColor = hasModel ? QVector3D(0.8f, 0.8f, 0.9f) : QVector3D(0.7f, 0.3f, 0.3f);
        shaderProgram->setUniformValue("u_materialColor", materialColor);
        shaderProgram->setUniformValue("u_wireframe", wireframeMode);
        shaderProgram->setUniformValue("u_lightingEnabled", lightingEnabled);
        
        // Set up realistic lighting values for nice visual appearance
        shaderProgram->setUniformValue("u_diffuseStrength", 0.7f);
        shaderProgram->setUniformValue("u_lightConstant", 1.0f);
        shaderProgram->setUniformValue("u_lightLinear", 0.09f);
        shaderProgram->setUniformValue("u_lightQuadratic", 0.032f);
        shaderProgram->setUniformValue("u_metallic", 0.1f);
        shaderProgram->setUniformValue("u_roughness", 0.5f);
        shaderProgram->setUniformValue("u_ao", 1.0f);
    } catch (...) {
        qWarning() << "Error setting shader uniforms, skipping frame";
        shaderProgram->release();
        return;
    }
    
    // Bind VAO and draw
    vao.bind();
    
    // Check for OpenGL errors before drawing
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        qWarning() << "OpenGL error before drawing:" << error;
    }
    
    // Choose how to draw based on whether we have a loaded 3D model or default cube
    if (hasModel && !indices.isEmpty()) {
        // Draw STL models using indexed triangles (more efficient)
        int indexCount = indices.size();
        qDebug() << "Drawing STL with" << indexCount << "indices";
        
        // Make sure we have a reasonable number of triangles to draw
        if (indexCount > 0 && indexCount < 1000000) { // Sanity check
            qDebug() << "About to call glDrawElements with" << indexCount << "indices";

            // Tell OpenGL to draw triangles using our index list
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.bufferId());
            
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, 0);
            
            qDebug() << "glDrawElements call completed successfully";
            
            // Check for errors immediately after draw call
            GLenum drawError = glGetError();
            if (drawError != GL_NO_ERROR) {
                qWarning() << "OpenGL error during glDrawElements:" << drawError;
            } else {
                qDebug() << "No OpenGL errors after glDrawElements";
            }
        } else {
            qWarning() << "Invalid index count for drawing:" << indexCount;
        }
    } else {
        // Draw the default cube using simple triangle vertices
        int vertexCount = triangleCount * 3;
        static bool logged = false;
        if (!logged) {
            qDebug() << "Drawing cube with" << vertexCount << "vertices";
            logged = true;
        }
        
        if (vertexCount > 0 && vertexCount < 1000000) { // Sanity check
            glDrawArrays(GL_TRIANGLES, 0, vertexCount);
            
            // Check for errors immediately after draw call
            GLenum drawError = glGetError();
            if (drawError != GL_NO_ERROR) {
                qWarning() << "OpenGL error during glDrawArrays:" << drawError;
            }
        } else {
            qWarning() << "Invalid vertex count for drawing:" << vertexCount;
        }
    }
    
    // Check for OpenGL errors after drawing
    error = glGetError();
    if (error != GL_NO_ERROR) {
        qWarning() << "OpenGL error after drawing:" << error;
    } else {
        qDebug() << "No OpenGL errors after drawing";
    }
    
    qDebug() << "About to release VAO";
    vao.release();
    qDebug() << "VAO released successfully";
    
    qDebug() << "About to release shader program";
    shaderProgram->release();
    qDebug() << "Shader program released successfully";
    
    // Reset polygon mode
    qDebug() << "About to reset polygon mode";
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qDebug() << "Polygon mode reset successfully";
    
    // Track frame completion
    qDebug() << "About to emit frameRendered signal";
    emit frameRendered();
    qDebug() << "frameRendered signal emitted successfully";
    
    qDebug() << "paintGL method completing normally";
    
    } catch (const std::exception& e) {
        qCritical() << "Exception in paintGL:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in paintGL";
    }
    
    qDebug() << "paintGL method finished (after catch blocks)";
}

void GLWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);
    
    // Update projection matrix
    projectionMatrix.setToIdentity();
    float aspect = float(width) / float(height ? height : 1);
    projectionMatrix.perspective(45.0f, aspect, 0.1f, 100.0f);
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    qDebug() << "GLWidget::mousePressEvent: Button" << event->button() << "at position" << event->pos();
    lastMousePos = event->pos();
    mousePressed = true;
    mouseButton = event->button();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (mousePressed) {
        QPoint delta = event->pos() - lastMousePos;
        qDebug() << "GLWidget::mouseMoveEvent: Delta" << delta << "Button" << mouseButton;
        
        if (mouseButton == Qt::LeftButton) {
            // Rotate the model when dragging with left mouse button
            float sensitivity = 0.5f;
            rotationY += delta.x() * sensitivity;
            rotationX += delta.y() * sensitivity;
            
            // Keep rotation values between -360 and +360 degrees
            while (rotationX > 360.0f) rotationX -= 360.0f;
            while (rotationX < -360.0f) rotationX += 360.0f;
            while (rotationY > 360.0f) rotationY -= 360.0f;
            while (rotationY < -360.0f) rotationY += 360.0f;
            
            qDebug() << "GLWidget::mouseMoveEvent: New rotations X:" << rotationX << "Y:" << rotationY;
        } else if (mouseButton == Qt::RightButton && camera) {
            // Move the view around when dragging with right mouse button
            float panSensitivity = 0.01f; // Lower value for less sensitivity
            float panX = float(delta.x()) * panSensitivity;
            float panY = float(-delta.y()) * panSensitivity;
            qDebug() << "GLWidget::mouseMoveEvent: Panning" << panX << panY << "(sensitivity:" << panSensitivity << ")";
            camera->pan(panX, panY);
        }
        lastMousePos = event->pos();
        update();
    }
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    qDebug() << "GLWidget::mouseReleaseEvent: Button" << event->button();
    Q_UNUSED(event)
    mousePressed = false;
    mouseButton = Qt::NoButton;
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    if (!event) {
        qWarning() << "GLWidget::wheelEvent: Null event received";
        return;
    }
    
    // Use mouse wheel to zoom in and out
    float delta = event->angleDelta().y() / 120.0f;
    float zoomSpeed = 0.1f;
    
    qDebug() << "GLWidget::wheelEvent: Delta" << delta << "Current zoom:" << zoomFactor;
    
    // Make sure we got a sensible scroll value
    if (!qIsFinite(delta) || qAbs(delta) > 10.0f) {
        qWarning() << "GLWidget::wheelEvent: Invalid delta value:" << delta;
        return;
    }
    
    if (camera) {
        float factor = 1.0f - delta * zoomSpeed * 0.1f;
        
        // Make sure zoom factor makes sense before using it
        if (!qIsFinite(factor) || factor <= 0.001f || factor > 1000.0f) {
            qWarning() << "GLWidget::wheelEvent: Invalid zoom factor:" << factor << "- skipping";
            return;
        }
        
        qDebug() << "GLWidget::wheelEvent: Camera zoom factor" << factor;
        
        try {
            camera->zoom(factor);
            qDebug() << "GLWidget::wheelEvent: Camera zoom completed successfully";
        } catch (const std::exception& e) {
            qWarning() << "GLWidget::wheelEvent: Exception during camera zoom:" << e.what();
        } catch (...) {
            qWarning() << "GLWidget::wheelEvent: Unknown exception during camera zoom";
        }
    } else {
        zoomFactor += delta * zoomSpeed;
        zoomFactor = qMax(0.1f, qMin(10.0f, zoomFactor));
        qDebug() << "GLWidget::wheelEvent: New zoom factor" << zoomFactor;
    }
    
    qDebug() << "GLWidget::wheelEvent: About to call update()";
    update();
    qDebug() << "GLWidget::wheelEvent: update() called successfully";
}

bool GLWidget::setupShaders()
{
    // Vertex shader - processes each vertex position and normal
    const char* vertexShaderSource = R"(
        #version 330 core
        
        layout (location = 0) in vec3 a_position;
        layout (location = 1) in vec3 a_normal;
        
        uniform mat4 u_mvpMatrix;
        uniform mat4 u_modelMatrix;
        uniform mat4 u_viewMatrix;
        uniform mat4 u_normalMatrix;
        uniform vec3 u_viewPos;
        uniform vec3 u_lightPos;
        
        out vec3 v_fragPos;
        out vec3 v_normal;
        out vec3 v_viewPos;
        out vec3 v_lightPos;
        out vec3 v_viewDir;
        out vec3 v_lightDir;
        out float v_distance;
        
        void main()
        {
            vec4 worldPos = u_modelMatrix * vec4(a_position, 1.0);
            v_fragPos = worldPos.xyz;
            v_normal = normalize(mat3(u_normalMatrix) * a_normal);
            
            v_viewPos = u_viewPos;
            v_lightPos = u_lightPos;
            v_viewDir = normalize(u_viewPos - v_fragPos);
            v_lightDir = normalize(u_lightPos - v_fragPos);
            v_distance = length(u_lightPos - v_fragPos);
            
            gl_Position = u_mvpMatrix * vec4(a_position, 1.0);
        }
    )";
    
    // Fragment shader - calculates final pixel colors with lighting
    const char* fragmentShaderSource = R"(
        #version 330 core
        
        in vec3 v_fragPos;
        in vec3 v_normal;
        in vec3 v_viewPos;
        in vec3 v_lightPos;
        in vec3 v_viewDir;
        in vec3 v_lightDir;
        in float v_distance;
        
        uniform vec3 u_lightColor;
        uniform vec3 u_materialColor;
        uniform float u_ambientStrength;
        uniform float u_diffuseStrength;
        uniform float u_specularStrength;
        uniform float u_shininess;
        uniform bool u_lightingEnabled;
        uniform bool u_wireframe;
        uniform float u_lightConstant;
        uniform float u_lightLinear;
        uniform float u_lightQuadratic;
        uniform float u_metallic;
        uniform float u_roughness;
        uniform float u_ao;
        
        out vec4 FragColor;
        
        vec3 calculateBlinnPhong(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 lightColor, vec3 materialColor)
        {
            vec3 ambient = u_ambientStrength * lightColor * materialColor;
            
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = u_diffuseStrength * diff * lightColor * materialColor;
            
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfwayDir), 0.0), u_shininess);
            vec3 specular = u_specularStrength * spec * lightColor;
            
            return ambient + diffuse + specular;
        }
        
        float calculateAttenuation(float distance)
        {
            float constant = u_lightConstant > 0.0 ? u_lightConstant : 1.0;
            float linear = u_lightLinear > 0.0 ? u_lightLinear : 0.09;
            float quadratic = u_lightQuadratic > 0.0 ? u_lightQuadratic : 0.032;
            
            return 1.0 / (constant + linear * distance + quadratic * (distance * distance));
        }
        
        void main()
        {
            vec3 normal = normalize(v_normal);
            vec3 lightDir = normalize(v_lightDir);
            vec3 viewDir = normalize(v_viewDir);
            
            vec3 finalColor = u_materialColor;
            
            if (u_wireframe) {
                finalColor = vec3(1.0, 1.0, 1.0);
            }
            else if (u_lightingEnabled) {
                float attenuation = calculateAttenuation(v_distance);
                vec3 attenuatedLightColor = u_lightColor * attenuation;
                
                vec3 litColor = calculateBlinnPhong(normal, lightDir, viewDir, 
                                                   attenuatedLightColor, u_materialColor);
                
                litColor *= u_ao > 0.0 ? u_ao : 1.0;
                finalColor = litColor;
            }
            
            // Gamma correction
            float gamma = 2.2;
            finalColor = pow(finalColor, vec3(1.0/gamma));
            
            FragColor = vec4(finalColor, 1.0);
        }
    )";
    
    // Create and compile shaders
    shaderProgram = new QOpenGLShaderProgram(this);
    
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qCritical() << "Failed to compile vertex shader:" << shaderProgram->log();
        return false;
    }
    
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qCritical() << "Failed to compile fragment shader:" << shaderProgram->log();
        return false;
    }
    
    if (!shaderProgram->link()) {
        qCritical() << "Failed to link shader program:" << shaderProgram->log();
        return false;
    }
    
    return true;
}

void GLWidget::setupDefaultGeometry()
{
    if (!isInitialized) {
        qWarning() << "Cannot setup default geometry - OpenGL not initialized";
        return;
    }
    
    qDebug() << "Setting up default cube geometry";
    
    // Create a cube to show when no STL file is loaded
    // Each line has: X, Y, Z position + X, Y, Z normal vector
    QVector<float> cubeVertices = {
        // Front face
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
        
        // Back face
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
         1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
         1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
        
        // Left face
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
        -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
        
        // Right face
         1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
         1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
         1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
         1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
         1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
         1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
        
        // Bottom face
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
         1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
         1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
         1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
        -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
        
        // Top face
        -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
         1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
         1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
         1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f
    };
    
    // Set up the internal state for displaying a cube
    triangleCount = 12; // 12 triangles for a cube
    hasModel = false;
    indices.clear(); // No indices for cube
    
    setupVertexBuffer(cubeVertices);
    
    qDebug() << "Default cube geometry setup complete";
}

void GLWidget::setupVertexBuffer(const QVector<float>& vertexData)
{
    if (!isInitialized || !context() || !context()->isValid()) {
        qWarning() << "OpenGL context not available during vertex buffer setup";
        return;
    }

    try {
        makeCurrent();

        // Remove any old vertex data first
        if (vertexBuffer.isCreated()) vertexBuffer.destroy();
        if (indexBuffer.isCreated()) indexBuffer.destroy();
        if (vao.isCreated()) vao.destroy();

        // Figure out how big the model is (only for STL files)
        if (hasModel) {
            calculateBoundingBox(vertexData);
        } else {
            boundingBoxValid = false;
        }
    
    // Create VAO (Vertex Array Object) to store our vertex setup
    if (!vao.create()) {
        qCritical() << "Failed to create VAO";
        doneCurrent();
        return;
    }
    vao.bind();

    // Create VBO (Vertex Buffer Object) to hold vertex data in GPU memory
    if (!vertexBuffer.create()) {
        qCritical() << "Failed to create vertex buffer";
        vao.release();
        doneCurrent();
        return;
    }
    
    vertexBuffer.bind();
    vertexBuffer.allocate(vertexData.data(), static_cast<int>(vertexData.size() * sizeof(float)));

    // Tell OpenGL how to interpret our vertex data (position + normal)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    // Set up index buffer for STL models (helps with performance)
    if (hasModel && !indices.isEmpty()) {
        if (!indexBuffer.create()) {
            qCritical() << "Failed to create index buffer";
            vao.release();
            vertexBuffer.release();
            doneCurrent();
            return;
        }
        
        indexBuffer.bind();
        indexBuffer.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(unsigned int)));
        // Keep index buffer bound to VAO
    }
    
    // Release buffers and VAO
    vertexBuffer.release();
    vao.release();
    
        doneCurrent();
        
        qDebug() << "Vertex buffer setup complete. Vertices:" << vertexData.size() / 6 
                 << "Indices:" << indices.size() << "HasModel:" << hasModel;
                 
    } catch (const std::exception& e) {
        qCritical() << "Exception in setupVertexBuffer:" << e.what();
        doneCurrent();
    } catch (...) {
        qCritical() << "Unknown exception in setupVertexBuffer";
        doneCurrent();
    }
}

void GLWidget::loadSTLFile(const QString &fileName)
{
    qDebug() << "Loading STL file:" << fileName;
    
    if (!isInitialized || !context() || !context()->isValid()) {
        qWarning() << "OpenGL not initialized, cannot load STL file";
        return;
    }

    try {
        // Remove any previously loaded model
        cleanupModel();
        
        // Set up the STL file reader
        STLLoader loader;
        loader.setAutoCenter(true);
        loader.setAutoNormalize(true);
        
        // Try to read the STL file
        STLLoader::LoadResult result = loader.loadFile(fileName);
        
        if (result == STLLoader::Success) {
            // Get the 3D model data that was loaded
            const QVector<float>& vertexData = loader.getVertexData();
            const QVector<unsigned int>& indexData = loader.getIndices();
            
            if (vertexData.isEmpty()) {
                qWarning() << "STL file loaded but contains no vertex data";
                setupDefaultGeometry(); // Fallback to cube
                return;
            }
            
            // Check that the loaded data makes sense before using it
            int expectedVertexCount = vertexData.size() / 6; // 6 floats per vertex (pos + normal)
            qDebug() << "STL Data validation:";
            qDebug() << "  Raw vertex data size:" << vertexData.size();
            qDebug() << "  Expected vertex count:" << expectedVertexCount;
            qDebug() << "  Actual vertex count from loader:" << loader.getVertexCount();
            qDebug() << "  Index count:" << indexData.size();
            qDebug() << "  Triangle count:" << loader.getTriangleCount();
        
        // Make sure triangle indices don't point to non-existent vertices
        if (!indexData.isEmpty()) {
            unsigned int maxIndex = *std::max_element(indexData.begin(), indexData.end());
            if (maxIndex >= static_cast<unsigned int>(expectedVertexCount)) {
                qCritical() << "Index out of range! Max index:" << maxIndex 
                           << "Vertex count:" << expectedVertexCount;
                setupDefaultGeometry(); // Fallback to cube
                return;
            }
        }
        
        // Store the model information before setting up GPU buffers
        triangleCount = loader.getTriangleCount();
        hasModel = true;
        indices = indexData;
        
        qDebug() << "STL loaded successfully. Triangles:" << triangleCount 
                 << "Vertices:" << expectedVertexCount;
        
            // Send the model data to the graphics card
            setupVertexBuffer(vertexData);
            
            // Adjust the camera to show the whole model nicely
            QTimer::singleShot(100, this, &GLWidget::fitToWindow);
            
            emit fileLoaded(QFileInfo(fileName).fileName(), triangleCount, expectedVertexCount);
            
            // Redraw the display
            update();        } else {
            QString errorMsg = "Failed to load STL file: " + loader.getErrorString();
            qWarning() << errorMsg;
            // Fallback to default cube on error
            setupDefaultGeometry();
        }
        
    } catch (const std::exception& e) {
        qCritical() << "Exception while loading STL file:" << e.what();
        setupDefaultGeometry(); // Fallback to cube
    } catch (...) {
        qCritical() << "Unknown exception while loading STL file";
        setupDefaultGeometry(); // Fallback to cube
    }
}

void GLWidget::calculateBoundingBox(const QVector<float>& vertexData)
{
    if (vertexData.isEmpty()) {
        boundingBoxValid = false;
        return;
    }
    
    // Initialize with first vertex
    modelMin = QVector3D(vertexData[0], vertexData[1], vertexData[2]);
    modelMax = modelMin;
    
    // Iterate through all vertices (stride of 6: position + normal)
    for (int i = 0; i < vertexData.size(); i += 6) {
        QVector3D vertex(vertexData[i], vertexData[i + 1], vertexData[i + 2]);
        
        // Update min bounds
        modelMin.setX(qMin(modelMin.x(), vertex.x()));
        modelMin.setY(qMin(modelMin.y(), vertex.y()));
        modelMin.setZ(qMin(modelMin.z(), vertex.z()));
        
        // Update max bounds
        modelMax.setX(qMax(modelMax.x(), vertex.x()));
        modelMax.setY(qMax(modelMax.y(), vertex.y()));
        modelMax.setZ(qMax(modelMax.z(), vertex.z()));
    }
    
    // Calculate center and radius
    modelCenter = (modelMin + modelMax) * 0.5f;
    
    // Calculate the maximum extent from center
    QVector3D extent = modelMax - modelMin;
    modelRadius = qMax(extent.x(), qMax(extent.y(), extent.z())) * 0.5f;
    
    // Add small padding
    modelRadius *= 1.1f;
    
    boundingBoxValid = true;
    
    qDebug() << "Model bounds calculated:";
    qDebug() << "  Min:" << modelMin;
    qDebug() << "  Max:" << modelMax;
    qDebug() << "  Center:" << modelCenter;
    qDebug() << "  Radius:" << modelRadius;
}

void GLWidget::centerModel()
{
    if (!camera) {
        return;
    }
    
    if (boundingBoxValid && hasModel) {
        // Center the target on the model, keep current distance
        QVector3D currentPos = camera->getPosition();
        QVector3D currentTarget = camera->getTarget();
        QVector3D offset = currentPos - currentTarget;
        
        camera->setTarget(modelCenter);
        camera->setPosition(modelCenter + offset);
    } else {
        // Default centering
        camera->setTarget(QVector3D(0, 0, 0));
    }
    
    update();
}

void GLWidget::cleanupModel()
{
    if (!isInitialized || !context() || !context()->isValid()) {
        return;
    }

    makeCurrent();

    // Clean up buffers
    if (vertexBuffer.isCreated()) {
        vertexBuffer.destroy();
    }
    if (indexBuffer.isCreated()) {
        indexBuffer.destroy();
    }
    if (vao.isCreated()) {
        vao.destroy();
    }

    doneCurrent();

    // Reset model data
    indices.clear();
    triangleCount = 0;
    hasModel = false;
    boundingBoxValid = false;
}

void GLWidget::resetCamera()
{
    // Put the camera back to its starting position
    if (camera) {
        camera->reset();
    }
    
    // Reset rotation and zoom back to defaults
    zoomFactor = 1.0f;
    rotationX = rotationY = rotationZ = 0.0f;
    
    update();
}

void GLWidget::fitToWindow()
{
    if (!camera) {
        return;
    }
    
    // Start with a fresh camera position
    camera->reset();
    rotationX = rotationY = rotationZ = 0.0f;
    zoomFactor = 1.0f;
    
    if (!boundingBoxValid || !hasModel) {
        // Nothing special to do for the default cube
        update();
        return;
    }
    
    // Make sure the model dimensions make sense
    if (!qIsFinite(modelRadius) || modelRadius <= 0.0f ||
        !qIsFinite(modelCenter.x()) || !qIsFinite(modelCenter.y()) || !qIsFinite(modelCenter.z())) {
        qWarning() << "Invalid bounding box, cannot fit to window";
        update();
        return;
    }
    
    // Figure out how far back the camera needs to be to see the whole model
    float fovRadians = qDegreesToRadians(camera->getFov());
    float aspectRatio = float(width()) / float(height() ? height() : 1);
    
    // Make sure camera settings are reasonable
    if (!qIsFinite(fovRadians) || fovRadians <= 0.0f || fovRadians >= M_PI ||
        !qIsFinite(aspectRatio) || aspectRatio <= 0.0f) {
        qWarning() << "Invalid camera parameters for fit to window";
        update();
        return;
    }
    
    // Calculate how far back the camera should be
    float distance;
    if (aspectRatio >= 1.0f) {
        // Wide window - fit to height
        distance = modelRadius / tanf(fovRadians * 0.5f);
    } else {
        // Tall window - fit to width
        float horizontalFov = 2.0f * atanf(tanf(fovRadians * 0.5f) * aspectRatio);
        distance = modelRadius / tanf(horizontalFov * 0.5f);
    }
    
    // Make sure we got a reasonable distance and add some space around the model
    if (!qIsFinite(distance) || distance <= 0.0f) {
        distance = 5.0f; // Fallback distance
    } else {
        distance *= 1.2f; // Add padding
    }
    
    // Keep distance within reasonable limits
    distance = qBound(0.1f, distance, 100.0f);
    
    // Put the camera back from the model center
    QVector3D cameraPos = modelCenter + QVector3D(0, 0, distance);
    
    // Make sure the camera position makes sense
    if (!qIsFinite(cameraPos.x()) || !qIsFinite(cameraPos.y()) || !qIsFinite(cameraPos.z())) {
        qWarning() << "Invalid camera position calculated";
        camera->reset();
        update();
        return;
    }
    
    camera->setPosition(cameraPos);
    camera->setTarget(modelCenter);
    camera->setUp(QVector3D(0, 1, 0));
    
    qDebug() << "Camera fitted to model:";
    qDebug() << "  Position:" << cameraPos;
    qDebug() << "  Target:" << modelCenter;
    qDebug() << "  Distance:" << distance;
    
    update();
}

void GLWidget::setWireframeMode(bool wireframe)
{
    wireframeMode = wireframe;
    update();
}

void GLWidget::setLightingEnabled(bool enabled)
{
    lightingEnabled = enabled;
    update();
}

void GLWidget::setZoom(float factor)
{
    zoomFactor = qMax(0.1f, qMin(10.0f, factor));
    update();
}

void GLWidget::setRotationX(int degrees)
{
    rotationX = degrees;
    update();
}

void GLWidget::setRotationY(int degrees)
{
    rotationY = degrees;
    update();
}

void GLWidget::setRotationZ(int degrees)
{
    rotationZ = degrees;
    update();
}