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

// Helper for arcball mapping
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
    
    // Don't create camera tutaj - czekaj na kontekst OpenGL

    // Ustaw domyślny kolor (jasnoszary)
    defaultColor = QVector3D(0.8f, 0.8f, 0.8f);
}

GLWidget::~GLWidget()
{
    // Ensure proper cleanup
    cleanup();
}

void GLWidget::cleanup()
{
    // Stop any timers first
    renderTimer.stop();
    
    // Make sure we have a valid context before cleanup
    if (context() && context()->isValid()) {
        makeCurrent();
        
        // Clean up OpenGL resources
        if (shaderProgram) {
            delete shaderProgram;
            shaderProgram = nullptr;
        }
        
        // Clean up buffers
        if (vertexBuffer.isCreated()) {
            vertexBuffer.destroy();
        }
        
        if (indexBuffer.isCreated()) {
            indexBuffer.destroy();
        }
        
        // Clean up VAO
        if (vao.isCreated()) {
            vao.destroy();
        }
        
        doneCurrent();
    }
    
    // Clean up camera
    if (camera) {
        delete camera;
        camera = nullptr;
    }
    
    isInitialized = false;
}

void GLWidget::initializeGL()
{
    // Initialize OpenGL functions
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

    // Create cube as default geometry
    setupDefaultGeometry();

    // Initialize matrices
    modelMatrix.setToIdentity();
    viewMatrix.setToIdentity();
    viewMatrix.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    // Start animation timer AFTER initialization
    connect(&renderTimer, &QTimer::timeout, this, QOverload<>::of(&GLWidget::update));
    renderTimer.start(16); // ~60 FPS
    
    isInitialized = true;
    
    qDebug() << "OpenGL initialized successfully";
}

void GLWidget::paintGL()
{
    // Ensure we're initialized and have valid context
    if (!isInitialized || !context() || !context()->isValid()) {
        return;
    }

    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (!shaderProgram || !vao.isCreated()) {
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

    // Update camera matrices
    if (camera) {
        camera->aspect = float(width()) / float(height() ? height() : 1);
        viewMatrix = camera->getViewMatrix();
        projectionMatrix = camera->getProjectionMatrix();
    }
    
    modelMatrix.setToIdentity();
    modelMatrix.scale(zoomFactor);
    modelMatrix.rotate(rotationX, 1, 0, 0);
    modelMatrix.rotate(rotationY, 0, 1, 0);
    modelMatrix.rotate(rotationZ, 0, 0, 1);

    QMatrix4x4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    QMatrix4x4 normalMatrix = modelMatrix.inverted().transposed();

    // Set shader uniforms
    shaderProgram->setUniformValue("u_mvpMatrix", mvpMatrix);
    shaderProgram->setUniformValue("u_modelMatrix", modelMatrix);
    shaderProgram->setUniformValue("u_color", defaultColor);
    shaderProgram->setUniformValue("u_viewMatrix", viewMatrix);
    shaderProgram->setUniformValue("u_normalMatrix", normalMatrix);

    // Lighting uniforms
    shaderProgram->setUniformValue("u_lightDir", QVector3D(-0.5f, -1.0f, -0.3f));
    shaderProgram->setUniformValue("u_lightColor", QVector3D(1.0f, 1.0f, 1.0f));
    
    if (camera) {
        shaderProgram->setUniformValue("u_lightPos", camera->getPosition());
        shaderProgram->setUniformValue("u_viewPos", camera->getPosition());
    } else {
        shaderProgram->setUniformValue("u_lightPos", QVector3D(0.0f, 0.0f, 5.0f));
        shaderProgram->setUniformValue("u_viewPos", QVector3D(0.0f, 0.0f, 5.0f));
    }
    
    shaderProgram->setUniformValue("u_ambientStrength", 0.2f);
    shaderProgram->setUniformValue("u_specularStrength", 0.5f);
    shaderProgram->setUniformValue("u_shininess", 32.0f);

    // Material uniforms
    QVector3D materialColor = hasModel ? QVector3D(0.8f, 0.8f, 0.9f) : QVector3D(0.7f, 0.3f, 0.3f);
    shaderProgram->setUniformValue("u_materialColor", materialColor);
    shaderProgram->setUniformValue("u_wireframe", wireframeMode);
    shaderProgram->setUniformValue("u_lightingEnabled", lightingEnabled);
    
    // Additional lighting parameters
    shaderProgram->setUniformValue("u_diffuseStrength", 0.7f);
    shaderProgram->setUniformValue("u_lightConstant", 1.0f);
    shaderProgram->setUniformValue("u_lightLinear", 0.09f);
    shaderProgram->setUniformValue("u_lightQuadratic", 0.032f);
    shaderProgram->setUniformValue("u_metallic", 0.1f);
    shaderProgram->setUniformValue("u_roughness", 0.5f);
    shaderProgram->setUniformValue("u_ao", 1.0f);
    
    // Bind VAO and draw
    vao.bind();
    
    if (hasModel && !indices.isEmpty()) {
        // Draw indexed geometry - fixed size_t to int conversion
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    } else {
        // Draw non-indexed geometry
        glDrawArrays(GL_TRIANGLES, 0, triangleCount * 3);
    }
    
    vao.release();
    shaderProgram->release();
    
    // Reset polygon mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    
    emit frameRendered();
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
    lastMousePos = event->pos();
    mousePressed = true;
    mouseButton = event->button();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (mousePressed) {
        QPoint delta = event->pos() - lastMousePos;
        if (mouseButton == Qt::LeftButton) {
            // Simple rotation
            float sensitivity = 0.5f;
            rotationY += delta.x() * sensitivity;
            rotationX += delta.y() * sensitivity;
            
            // Clamp rotations
            while (rotationX > 360.0f) rotationX -= 360.0f;
            while (rotationX < -360.0f) rotationX += 360.0f;
            while (rotationY > 360.0f) rotationY -= 360.0f;
            while (rotationY < -360.0f) rotationY += 360.0f;
        } else if (mouseButton == Qt::RightButton && camera) {
            // Pan
            float panX = float(delta.x());
            float panY = float(-delta.y());
            camera->pan(panX, panY);
        }
        lastMousePos = event->pos();
        update();
    }
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    mousePressed = false;
    mouseButton = Qt::NoButton;
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    // Zoom with mouse wheel
    float delta = event->angleDelta().y() / 120.0f;
    float zoomSpeed = 0.1f;
    
    if (camera) {
        float factor = 1.0f - delta * zoomSpeed * 0.1f;
        camera->zoom(factor);
    } else {
        zoomFactor += delta * zoomSpeed;
        zoomFactor = qMax(0.1f, qMin(10.0f, zoomFactor));
    }
    update();
}

bool GLWidget::setupShaders()
{
    // Enhanced vertex shader
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
    
    // Enhanced fragment shader
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
    // Create a simple cube as default geometry
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
    
    triangleCount = 12; // 12 triangles for a cube
    hasModel = false;
    
    setupVertexBuffer(cubeVertices);
}

void GLWidget::setupVertexBuffer(const QVector<float>& vertexData)
{
    makeCurrent();
    

    if (vertexBuffer.isCreated()) vertexBuffer.destroy();
    if (indexBuffer.isCreated()) indexBuffer.destroy();
    if (vao.isCreated()) vao.destroy();

    // Add proper context validation
    if (!context() || !context()->isValid()) {
        qWarning() << "OpenGL context not available during vertex buffer setup";
        doneCurrent();
        return;
    }

    // Calculate bounding box for loaded models
    if (hasModel) {
        calculateBoundingBox(vertexData);
    } else {
        boundingBoxValid = false;
    }
    
    // Create VAO
    if (!vao.isCreated()) {
        if (!vao.create()) {
            qCritical() << "Failed to create VAO";
            doneCurrent();
            return;
        }
    }
    vao.bind();

    // Create VBO
    if (!vertexBuffer.isCreated()) {
        if (!vertexBuffer.create()) {
            qCritical() << "Failed to create vertex buffer";
            vao.release();
            doneCurrent();
            return;
        }
    }
    
    vertexBuffer.bind();
    vertexBuffer.allocate(vertexData.data(), static_cast<int>(vertexData.size() * sizeof(float)));

    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    // Set up index buffer if we have indices
    if (hasModel && !indices.isEmpty()) {
        if (!indexBuffer.isCreated()) {
            if (!indexBuffer.create()) {
                qCritical() << "Failed to create index buffer";
                vao.release();
                vertexBuffer.release();
                doneCurrent();
                return;
            }
        }
        
        indexBuffer.bind();
        indexBuffer.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(unsigned int)));
    }
    
    // Release buffers and VAO
    if (indexBuffer.isCreated()) {
        indexBuffer.release();
    }
    vertexBuffer.release();
    vao.release();
    
    doneCurrent();
    
    qDebug() << "Vertex buffer setup complete. Vertices:" << vertexData.size() / 6;
}

// FIXED: File loading now runs on main thread with proper error handling
void GLWidget::loadSTLFile(const QString &fileName)
{

    cleanupModel();

    if (!isInitialized || !context() || !context()->isValid()) {
        qWarning() << "OpenGL not initialized, cannot load STL file";
        return;
    }
    qDebug() << "Loading STL file:" << fileName;
    
    // Create loader and configure it
    STLLoader loader;
    loader.setAutoCenter(true);
    loader.setAutoNormalize(true);
    
    // Load the file (this is the critical fix - ensure this runs on main thread)
    STLLoader::LoadResult result = loader.loadFile(fileName);
    
    if (result == STLLoader::Success) {
        // Get vertex data from loader
        const QVector<float>& vertexData = loader.getVertexData();
        const QVector<unsigned int>& indexData = loader.getIndices();
        
        if (vertexData.isEmpty()) {
            qWarning() << "STL file loaded but contains no vertex data";
            return;
        }
        
        triangleCount = loader.getTriangleCount();
        hasModel = true;
        indices = indexData;
        
        qDebug() << "STL loaded successfully. Triangles:" << triangleCount << "Vertices:" << loader.getVertexCount();
        
        // Set up index buffer if available
        if (!indices.isEmpty()) {
            makeCurrent();
            vao.bind();
            
            if (!indexBuffer.isCreated()) {
                if (!indexBuffer.create()) {
                    qCritical() << "Failed to create index buffer";
                    vao.release();
                    doneCurrent();
                    return;
                }
            }
            
            indexBuffer.bind();
            indexBuffer.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(unsigned int)));
            
            vao.release();
            doneCurrent();
        }
        
        // Automatically fit the model to window after loading
        fitToWindow();
        
        emit fileLoaded(QFileInfo(fileName).fileName(), triangleCount, loader.getVertexCount());
        
        // Force an update
        update();
        
    } else {
        QString errorMsg = "Failed to load STL file: " + loader.getErrorString();
        qWarning() << errorMsg;
        // Keep showing the default cube on error
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
    // Reset the Camera object to its default state
    if (camera) {
        camera->reset();
    }
    
    // Reset the old rotation and zoom variables
    zoomFactor = 1.0f;
    rotationX = rotationY = rotationZ = 0.0f;
    
    update();
}

void GLWidget::fitToWindow()
{
    if (!camera) {
        return;
    }
    
    // Reset camera first
    camera->reset();
    rotationX = rotationY = rotationZ = 0.0f;
    zoomFactor = 1.0f;
    
    if (!boundingBoxValid || !hasModel) {
        // Just reset for default cube or invalid bounds
        update();
        return;
    }
    
    // Calculate optimal camera distance
    float fovRadians = qDegreesToRadians(camera->getFov());
    float aspectRatio = float(width()) / float(height() ? height() : 1);
    
    // Calculate distance needed to fit the model
    float distance;
    if (aspectRatio >= 1.0f) {
        // Landscape or square - fit to height
        distance = modelRadius / tanf(fovRadians * 0.5f);
    } else {
        // Portrait - fit to width, accounting for aspect ratio
        float horizontalFov = 2.0f * atanf(tanf(fovRadians * 0.5f) * aspectRatio);
        distance = modelRadius / tanf(horizontalFov * 0.5f);
    }
    
    // Add some padding
    distance *= 1.2f;
    
    // Position camera to look at model center
    QVector3D cameraPos = modelCenter + QVector3D(0, 0, distance);
    
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