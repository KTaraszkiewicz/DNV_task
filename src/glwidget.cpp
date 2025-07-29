#include "camera.h"
#include <QCursor>

// Helper for arcball mapping
static QVector3D mapToArcball(int x, int y, int w, int h) {
    float nx = (2.0f * x - w) / w;
    float ny = (h - 2.0f * y) / h;
    float length2 = nx * nx + ny * ny;
    float z = length2 > 1.0f ? 0.0f : sqrtf(1.0f - length2);
    return QVector3D(nx, ny, z).normalized();
}
#include "glwidget.h"
#include <QFileInfo>
#include "stlloader.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QtMath>
#include <QDebug>

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
{
    camera = new Camera();
    // Enable multisampling for better quality
    QSurfaceFormat format;
    format.setSamples(4);
    format.setDepthBufferSize(24);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    
    // Set focus policy to receive key events
    setFocusPolicy(Qt::StrongFocus);
    
    // Animation timer for smooth rendering
    connect(&renderTimer, &QTimer::timeout, this, QOverload<>::of(&GLWidget::update));
    renderTimer.start(16); // ~60 FPS
}

GLWidget::~GLWidget()
{
    makeCurrent();
    if (shaderProgram) { /* ...existing code... */ }
    vertexBuffer.destroy();
    vao.destroy();
    delete camera;
    doneCurrent();
    vao.destroy();
    initializeOpenGLFunctions();


    glDepthFunc(GL_LESS);
    
    // Enable back-face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Enable multisampling
    glEnable(GL_MULTISAMPLE);
    
    // Set up shaders
    setupShaders();
    
    // Create cube as default geometry
    setupDefaultGeometry();
    
    // Initialize matrices
    modelMatrix.setToIdentity();
    viewMatrix.setToIdentity();
    viewMatrix.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
}

void GLWidget::paintGL()
{
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

    // Update model matrix with rotations and zoom
    modelMatrix.setToIdentity();
    modelMatrix.scale(zoomFactor);
    modelMatrix.rotate(rotationX, 1, 0, 0);
    modelMatrix.rotate(rotationY, 0, 1, 0);
    modelMatrix.rotate(rotationZ, 0, 0, 1);

    // Calculate MVP matrix
    QMatrix4x4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    QMatrix4x4 normalMatrix = modelMatrix.inverted().transposed();

    // Set transformation uniforms
    shaderProgram->setUniformValue("u_mvpMatrix", mvpMatrix);
    shaderProgram->setUniformValue("u_modelMatrix", modelMatrix);
    shaderProgram->setUniformValue("u_viewMatrix", viewMatrix);
    shaderProgram->setUniformValue("u_normalMatrix", normalMatrix);

    // Lighting uniforms
    // Directional light (sun)
    shaderProgram->setUniformValue("u_lightDir", QVector3D(-0.5f, -1.0f, -0.3f));
    shaderProgram->setUniformValue("u_lightColor", QVector3D(1.0f, 1.0f, 1.0f));
    // Point light (headlight)
    if (camera) {
        shaderProgram->setUniformValue("u_lightPos", camera->getPosition());
        shaderProgram->setUniformValue("u_viewPos", camera->getPosition());
    } else {
        shaderProgram->setUniformValue("u_lightPos", QVector3D(0.0f, 0.0f, 5.0f));
        shaderProgram->setUniformValue("u_viewPos", QVector3D(0.0f, 0.0f, 5.0f));
    }
    // Ambient light
    shaderProgram->setUniformValue("u_ambientStrength", 0.2f);

    // Material uniforms
    shaderProgram->setUniformValue("u_materialColor", QVector3D(0.8f, 0.8f, 0.8f)); // base color
    shaderProgram->setUniformValue("u_specularStrength", 0.5f);
    shaderProgram->setUniformValue("u_shininess", 32.0f);

    // Rendering mode uniforms
    shaderProgram->setUniformValue("u_wireframe", wireframeMode);
    shaderProgram->setUniformValue("u_lightingEnabled", lightingEnabled);
    shaderProgram->setUniformValue("u_normalMatrix", normalMatrix);
    shaderProgram->setUniformValue("u_viewPos", QVector3D(0, 0, 5));
    shaderProgram->setUniformValue("u_lightingEnabled", lightingEnabled);
    shaderProgram->setUniformValue("u_wireframe", wireframeMode);
    
    // Light properties
    shaderProgram->setUniformValue("u_lightPos", QVector3D(10, 10, 10));
    shaderProgram->setUniformValue("u_lightColor", QVector3D(1.0f, 1.0f, 1.0f));
    shaderProgram->setUniformValue("u_ambientStrength", 0.3f);
    shaderProgram->setUniformValue("u_diffuseStrength", 0.7f);
    shaderProgram->setUniformValue("u_specularStrength", 0.5f);
    shaderProgram->setUniformValue("u_shininess", 32.0f);
    
    // Light attenuation parameters
    shaderProgram->setUniformValue("u_lightConstant", 1.0f);
    shaderProgram->setUniformValue("u_lightLinear", 0.09f);
    shaderProgram->setUniformValue("u_lightQuadratic", 0.032f);
    
    // Material properties for PBR-style rendering
    QVector3D materialColor = hasModel ? QVector3D(0.8f, 0.8f, 0.9f) : QVector3D(0.7f, 0.3f, 0.3f);
    shaderProgram->setUniformValue("u_materialColor", materialColor);
    shaderProgram->setUniformValue("u_metallic", 0.1f);      // Slightly metallic
    shaderProgram->setUniformValue("u_roughness", 0.5f);     // Medium roughness
    shaderProgram->setUniformValue("u_ao", 1.0f);            // No ambient occlusion
    
    // Bind VAO and draw
    vao.bind();
    
    if (hasModel && !indices.isEmpty()) {
        // Draw indexed geometry
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
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
    if (event->button() == Qt::LeftButton) {
        lastMousePos = event->pos();
        mousePressed = true;
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (mousePressed && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->pos() - lastMousePos;
        
        // Convert mouse movement to rotation
        float sensitivity = 0.5f;
        rotationY += delta.x() * sensitivity;
        rotationX += delta.y() * sensitivity;
        
        // Clamp rotations to reasonable range
        while (rotationX > 360.0f) rotationX -= 360.0f;
        while (rotationX < -360.0f) rotationX += 360.0f;
        while (rotationY > 360.0f) rotationY -= 360.0f;
        while (rotationY < -360.0f) rotationY += 360.0f;
        
        lastMousePos = event->pos();
        update();
    }
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mousePressed = false;
    }
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    // Zoom with mouse wheel
    float delta = event->angleDelta().y() / 120.0f; // Standard wheel step
    float zoomSpeed = 0.1f;
    
    zoomFactor += delta * zoomSpeed;
    zoomFactor = qMax(0.1f, qMin(10.0f, zoomFactor)); // Clamp zoom
    
    update();
}

void GLWidget::setupShaders()
{
    // Vertex shader source - enhanced with more lighting calculations
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
    
    // Fragment shader source - enhanced with Blinn-Phong and attenuation
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
            
            float gamma = 2.2;
            finalColor = pow(finalColor, vec3(1.0/gamma));
            
            FragColor = vec4(finalColor, 1.0);
        }
    )";
    
    // Create and compile shaders
    shaderProgram = new QOpenGLShaderProgram(this);
    
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qWarning() << "Failed to compile vertex shader:" << shaderProgram->log();
        return;
    }
    
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qWarning() << "Failed to compile fragment shader:" << shaderProgram->log();
        return;
    }
    
    if (!shaderProgram->link()) {
        qWarning() << "Failed to link shader program:" << shaderProgram->log();
        return;
    }
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
    
    // Create VAO
    if (!vao.isCreated()) {
        vao.create();
    }
    vao.bind();
    
    // Create VBO
    if (!vertexBuffer.isCreated()) {
        vertexBuffer.create();
    }
    vertexBuffer.bind();
    vertexBuffer.allocate(vertexData.data(), vertexData.size() * sizeof(float));
    
    // Set up vertex attributes
    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    // Normal attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    vao.release();
    vertexBuffer.release();
    
    doneCurrent();
}

// Public interface methods
void GLWidget::loadSTLFile(const QString &fileName)
{
    STLLoader loader;
    loader.setAutoCenter(true);
    loader.setAutoNormalize(true);
    
    STLLoader::LoadResult result = loader.loadFile(fileName);
    
    if (result == STLLoader::Success) {
        // Get vertex data from loader
        const QVector<float>& vertexData = loader.getVertexData();
        const QVector<unsigned int>& indexData = loader.getIndices();
        
        triangleCount = loader.getTriangleCount();
        hasModel = true;
        indices = indexData;
        
        // Set up vertex buffer with STL data
        setupVertexBuffer(vertexData);
        
        // Set up index buffer if available
        if (!indices.isEmpty()) {
            makeCurrent();
            vao.bind();
            
            if (!indexBuffer.isCreated()) {
                indexBuffer.create();
            }
            indexBuffer.bind();
            indexBuffer.allocate(indices.data(), indices.size() * sizeof(unsigned int));
            
            vao.release();
            doneCurrent();
        }
        
        // Reset view to show the model
        resetCamera();
        update();
        
        emit fileLoaded(QFileInfo(fileName).fileName(), triangleCount, loader.getVertexCount());
    } else {
        qWarning() << "Failed to load STL file:" << loader.getErrorString();
        // Keep showing the default cube
    }
}

void GLWidget::resetCamera()
{
    zoomFactor = 1.0f;
    rotationX = rotationY = rotationZ = 0.0f;
    update();
}

void GLWidget::fitToWindow()
{
    // This would require calculating the model's bounding box
    // For now, just reset the zoom
    zoomFactor = 1.0f;
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