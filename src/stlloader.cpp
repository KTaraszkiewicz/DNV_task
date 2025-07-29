#include "stlloader.h"
#include <QFileInfo>
#include <QDebug>
#include <QtMath>
#include <QRegularExpression>
#include <algorithm>
#include <cfloat>

// Static constants
const char* STLLoader::ASCII_STL_HEADER = "solid";
const float STLLoader::DEFAULT_VERTEX_TOLERANCE = 1e-6f;

STLLoader::STLLoader()
    : format(Unknown)
    , autoCenter(true)
    , autoNormalize(false)
    , calculateNormals(false)
    , mergeVertices(true)
    , vertexTolerance(DEFAULT_VERTEX_TOLERANCE)
{
}

STLLoader::~STLLoader()
{
    clear();
}

void STLLoader::clear()
{
    triangles.clear();
    vertices.clear();
    vertexData.clear();
    indices.clear();
    boundingBox.reset();
    fileName.clear();
    format = Unknown;
    errorString.clear();
}

STLLoader::LoadResult STLLoader::loadFile(const QString& fileName)
{
    clear();
    this->fileName = fileName;
    
    // Check if file exists
    QFileInfo fileInfo(fileName);
    if (!fileInfo.exists()) {
        setError("File does not exist: " + fileName);
        return FileNotFound;
    }
    
    if (!fileInfo.isReadable()) {
        setError("File is not readable: " + fileName);
        return CannotOpenFile;
    }
    
    if (fileInfo.size() == 0) {
        setError("File is empty: " + fileName);
        return EmptyFile;
    }
    
    // Detect format
    format = detectFormat(fileName);
    if (format == Unknown) {
        setError("Unknown or unsupported STL format");
        return InvalidFormat;
    }
    
    // Open file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        setError("Cannot open file: " + file.errorString());
        return CannotOpenFile;
    }
    
    LoadResult result = Success;
    
    // Load based on format
    try {
        if (format == Binary) {
            result = loadBinarySTL(file);
        } else if (format == ASCII) {
            result = loadASCIISTL(file);
        }
        
        if (result == Success) {
            processTriangles();
        }
        
    } catch (const std::exception& e) {
        setError(QString("Exception during loading: ") + e.what());
        result = ReadError;
    } catch (...) {
        setError("Unknown exception during loading");
        result = ReadError;
    }
    
    file.close();
    
    if (result != Success) {
        clear();
    }
    
    return result;
}

STLLoader::STLFormat STLLoader::detectFormat(const QString& fileName)
{
    if (isBinarySTL(fileName)) {
        return Binary;
    } else if (isASCIISTL(fileName)) {
        return ASCII;
    }
    return Unknown;
}

bool STLLoader::isBinarySTL(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    // Check file size - binary STL has specific structure
    qint64 fileSize = file.size();
    if (fileSize < BINARY_STL_HEADER_SIZE + 4) { // Header + triangle count
        file.close();
        return false;
    }
    
    // Skip header
    file.seek(BINARY_STL_HEADER_SIZE);
    
    // Read triangle count
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    
    quint32 triangleCount;
    stream >> triangleCount;
    
    // Calculate expected file size
    qint64 expectedSize = BINARY_STL_HEADER_SIZE + 4 + (triangleCount * BINARY_STL_TRIANGLE_SIZE);
    
    file.close();
    
    // Binary STL should match expected size exactly
    return (fileSize == expectedSize && triangleCount > 0 && triangleCount < 10000000); // Sanity check
}

bool STLLoader::isASCIISTL(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    QString firstLine = stream.readLine().trimmed().toLower();
    file.close();
    
    return firstLine.startsWith("solid");
}

STLLoader::LoadResult STLLoader::loadBinarySTL(QFile& file)
{
    if (file.size() < BINARY_STL_HEADER_SIZE + 4) {
        setError("File too small for binary STL format");
        return CorruptedFile;
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    
    // Skip 80-byte header
    file.seek(BINARY_STL_HEADER_SIZE);
    
    // Read triangle count
    quint32 triangleCount;
    stream >> triangleCount;
    
    if (triangleCount == 0) {
        setError("STL file contains no triangles");
        return EmptyFile;
    }
    
    if (triangleCount > 10000000) { // Sanity check
        setError("Triangle count seems unreasonably large: " + QString::number(triangleCount));
        return CorruptedFile;
    }
    
    // Reserve space for efficiency
    triangles.reserve(triangleCount);
    
    // Read triangles
    for (quint32 i = 0; i < triangleCount; ++i) {
        STLTriangle triangle;
        
        // Read normal vector
        float nx, ny, nz;
        stream >> nx >> ny >> nz;
        triangle.normal = QVector3D(nx, ny, nz);
        
        // Read vertices
        float v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z;
        stream >> v1x >> v1y >> v1z;
        stream >> v2x >> v2y >> v2z;
        stream >> v3x >> v3y >> v3z;
        
        triangle.vertex1 = QVector3D(v1x, v1y, v1z);
        triangle.vertex2 = QVector3D(v2x, v2y, v2z);
        triangle.vertex3 = QVector3D(v3x, v3y, v3z);
        
        // Skip 2-byte attribute
        quint16 attribute;
        stream >> attribute;
        
        // Check for read errors
        if (stream.status() != QDataStream::Ok) {
            setError(QString("Error reading triangle %1").arg(i));
            return ReadError;
        }
        
        // Validate triangle
        if (isValidTriangle(triangle)) {
            triangles.append(triangle);
        }
    }
    
    if (triangles.isEmpty()) {
        setError("No valid triangles found in file");
        return EmptyFile;
    }
    
    return Success;
}

STLLoader::LoadResult STLLoader::loadASCIISTL(QFile& file)
{
    QTextStream stream(&file);
    QString line;
    int lineNumber = 0;
    
    // Read first line - should start with "solid"
    line = stream.readLine().trimmed();
    lineNumber++;
    
    if (!line.toLower().startsWith("solid")) {
        setError(QString("Invalid ASCII STL header at line %1").arg(lineNumber));
        return InvalidFormat;
    }
    
    STLTriangle currentTriangle;
    int vertexCount = 0;
    bool inFacet = false;
    bool inLoop = false;
    
    QRegularExpression vectorRegex(R"((-?[\d\.e\-\+]+)\s+(-?[\d\.e\-\+]+)\s+(-?[\d\.e\-\+]+))");
    
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        lineNumber++;
        
        if (line.isEmpty() || line.startsWith("#")) {
            continue; // Skip empty lines and comments
        }
        
        QString keyword;
        QStringList values;
        if (!parseASCIILine(line, keyword, values)) {
            continue; // Skip malformed lines
        }
        
        if (keyword == "facet" && values.size() >= 4 && values[0] == "normal") {
            if (inFacet) {
                setError(QString("Unexpected 'facet' at line %1 - previous facet not closed").arg(lineNumber));
                return CorruptedFile;
            }
            
            // Parse normal vector
            bool ok1, ok2, ok3;
            float nx = values[1].toFloat(&ok1);
            float ny = values[2].toFloat(&ok2);
            float nz = values[3].toFloat(&ok3);
            
            if (!ok1 || !ok2 || !ok3) {
                setError(QString("Invalid normal vector at line %1").arg(lineNumber));
                return CorruptedFile;
            }
            
            currentTriangle.normal = QVector3D(nx, ny, nz);
            inFacet = true;
            vertexCount = 0;
            
        } else if (keyword == "outer" && values.size() >= 1 && values[0] == "loop") {
            if (!inFacet || inLoop) {
                setError(QString("Unexpected 'outer loop' at line %1").arg(lineNumber));
                return CorruptedFile;
            }
            inLoop = true;
            
        } else if (keyword == "vertex" && values.size() >= 3) {
            if (!inLoop) {
                setError(QString("Vertex outside loop at line %1").arg(lineNumber));
                return CorruptedFile;
            }
            
            bool ok1, ok2, ok3;
            float vx = values[0].toFloat(&ok1);
            float vy = values[1].toFloat(&ok2);
            float vz = values[2].toFloat(&ok3);
            
            if (!ok1 || !ok2 || !ok3) {
                setError(QString("Invalid vertex coordinates at line %1").arg(lineNumber));
                return CorruptedFile;
            }
            
            QVector3D vertex(vx, vy, vz);
            
            if (vertexCount == 0) {
                currentTriangle.vertex1 = vertex;
            } else if (vertexCount == 1) {
                currentTriangle.vertex2 = vertex;
            } else if (vertexCount == 2) {
                currentTriangle.vertex3 = vertex;
            } else {
                setError(QString("Too many vertices in facet at line %1").arg(lineNumber));
                return CorruptedFile;
            }
            vertexCount++;
            
        } else if (keyword == "endloop") {
            if (!inLoop) {
                setError(QString("'endloop' without 'outer loop' at line %1").arg(lineNumber));
                return CorruptedFile;
            }
            
            if (vertexCount != 3) {
                setError(QString("Facet has %1 vertices instead of 3 at line %2").arg(vertexCount).arg(lineNumber));
                return CorruptedFile;
            }
            
            inLoop = false;
            
        } else if (keyword == "endfacet") {
            if (!inFacet || inLoop) {
                setError(QString("'endfacet' without proper 'facet' at line %1").arg(lineNumber));
                return CorruptedFile;
            }
            
            // Validate and add triangle
            if (isValidTriangle(currentTriangle)) {
                triangles.append(currentTriangle);
            }
            
            inFacet = false;
            
        } else if (keyword == "endsolid") {
            break; // End of model
        }
    }
    
    if (triangles.isEmpty()) {
        setError("No valid triangles found in ASCII STL file");
        return EmptyFile;
    }
    
    return Success;
}

void STLLoader::processTriangles()
{
    if (triangles.isEmpty()) {
        return;
    }
    
    // Calculate bounding box first
    calculateBoundingBox();
    
    // Center model if requested
    if (autoCenter) {
        centerModel();
    }
    
    // Normalize model if requested
    if (autoNormalize) {
        normalizeModel();
    }
    
    // Generate vertex buffer
    generateVertexBuffer();
    
    // Generate indices if vertex merging is enabled
    if (mergeVertices) {
        generateIndices();
    }
}

void STLLoader::calculateBoundingBox()
{
    boundingBox.reset();
    
    for (const STLTriangle& triangle : triangles) {
        boundingBox.update(triangle.vertex1);
        boundingBox.update(triangle.vertex2);
        boundingBox.update(triangle.vertex3);
    }
    
    boundingBox.finalize();
}

void STLLoader::centerModel()
{
    if (!boundingBox.isValid()) {
        return;
    }
    
    QVector3D offset = -boundingBox.center;
    
    // Apply offset to all triangles
    for (STLTriangle& triangle : triangles) {
        triangle.vertex1 += offset;
        triangle.vertex2 += offset;
        triangle.vertex3 += offset;
    }
    
    // Update bounding box
    boundingBox.min += offset;
    boundingBox.max += offset;
    boundingBox.center = QVector3D(0, 0, 0);
}

void STLLoader::normalizeModel()
{
    if (!boundingBox.isValid() || boundingBox.maxDimension == 0) {
        return;
    }
    
    float scale = 2.0f / boundingBox.maxDimension; // Scale to fit in [-1, 1]
    
    // Apply scale to all triangles
    for (STLTriangle& triangle : triangles) {
        triangle.vertex1 *= scale;
        triangle.vertex2 *= scale;
        triangle.vertex3 *= scale;
    }
    
    // Update bounding box
    boundingBox.min *= scale;
    boundingBox.max *= scale;
    boundingBox.center *= scale;
    boundingBox.size *= scale;
    boundingBox.maxDimension *= scale;
}

void STLLoader::generateVertexBuffer()
{
    vertices.clear();
    vertexData.clear();
    
    vertices.reserve(triangles.size() * 3);
    vertexData.reserve(triangles.size() * 3 * 6); // 3 vertices * 6 floats (pos + normal)
    
    for (const STLTriangle& triangle : triangles) {
        QVector3D normal = triangle.normal;
        
        // Calculate normal if not provided or if requested
        if (calculateNormals || normal.lengthSquared() < 0.001f) {
            normal = calculateTriangleNormal(triangle.vertex1, triangle.vertex2, triangle.vertex3);
        }
        
        // Add vertices
        vertices.append(STLVertex(triangle.vertex1, normal));
        vertices.append(STLVertex(triangle.vertex2, normal));
        vertices.append(STLVertex(triangle.vertex3, normal));
        
        // Add to interleaved vertex data (position + normal)
        // Vertex 1
        vertexData.append(triangle.vertex1.x());
        vertexData.append(triangle.vertex1.y());
        vertexData.append(triangle.vertex1.z());
        vertexData.append(normal.x());
        vertexData.append(normal.y());
        vertexData.append(normal.z());
        
        // Vertex 2
        vertexData.append(triangle.vertex2.x());
        vertexData.append(triangle.vertex2.y());
        vertexData.append(triangle.vertex2.z());
        vertexData.append(normal.x());
        vertexData.append(normal.y());
        vertexData.append(normal.z());
        
        // Vertex 3
        vertexData.append(triangle.vertex3.x());
        vertexData.append(triangle.vertex3.y());
        vertexData.append(triangle.vertex3.z());
        vertexData.append(normal.x());
        vertexData.append(normal.y());
        vertexData.append(normal.z());
    }
}

void STLLoader::generateIndices()
{
    QVector<STLVertex> uniqueVertices;
    indices.clear();
    
    uniqueVertices.reserve(vertices.size());
    indices.reserve(vertices.size());
    
    for (const STLVertex& vertex : vertices) {
        int index = findOrAddVertex(vertex);
        indices.append(index);
    }
    
    // Update vertices to unique set
    vertices = uniqueVertices;
    
    // Regenerate vertex data
    vertexData.clear();
    vertexData.reserve(vertices.size() * 6);
    
    for (const STLVertex& vertex : vertices) {
        vertexData.append(vertex.position.x());
        vertexData.append(vertex.position.y());
        vertexData.append(vertex.position.z());
        vertexData.append(vertex.normal.x());
        vertexData.append(vertex.normal.y());
        vertexData.append(vertex.normal.z());
    }
}

QVector3D STLLoader::calculateTriangleNormal(const QVector3D& v1, const QVector3D& v2, const QVector3D& v3)
{
    QVector3D edge1 = v2 - v1;
    QVector3D edge2 = v3 - v1;
    QVector3D normal = QVector3D::crossProduct(edge1, edge2);
    
    if (normal.lengthSquared() > 0) {
        normal.normalize();
    }
    
    return normal;
}

void STLLoader::setError(const QString& error)
{
    errorString = error;
    qWarning() << "STLLoader Error:" << error;
}

bool STLLoader::isValidTriangle(const STLTriangle& triangle)
{
    // Check for degenerate triangles
    float area = QVector3D::crossProduct(triangle.vertex2 - triangle.vertex1, 
                                        triangle.vertex3 - triangle.vertex1).length();
    return area > 1e-10f; // Very small threshold for valid area
}

int STLLoader::findOrAddVertex(const STLVertex& vertex)
{
    // Find existing vertex within tolerance
    for (int i = 0; i < vertices.size(); ++i) {
        if (verticesEqual(vertices[i], vertex, vertexTolerance)) {
            return i;
        }
    }
    
    // Add new vertex
    vertices.append(vertex);
    return vertices.size() - 1;
}

bool STLLoader::verticesEqual(const STLVertex& a, const STLVertex& b, float tolerance)
{
    return (a.position - b.position).lengthSquared() < tolerance * tolerance;
}

bool STLLoader::parseASCIILine(const QString& line, QString& keyword, QStringList& values)
{
    QStringList tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    
    if (tokens.isEmpty()) {
        return false;
    }
    
    keyword = tokens[0].toLower();
    values = tokens.mid(1);
    
    return true;
}

QString STLLoader::getFormatString() const
{
    switch (format) {
        case Binary: return "Binary STL";
        case ASCII: return "ASCII STL";
        default: return "Unknown";
    }
}