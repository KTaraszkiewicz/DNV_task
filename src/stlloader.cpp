#include "stlloader.h"
#include <QFileInfo>
#include <QDebug>
#include <QtMath>
#include <QRegularExpression>
#include <algorithm>
#include <cfloat>

// These are the magic numbers that define the STL file format
const char* STLLoader::ASCII_STL_HEADER = "solid";
const float STLLoader::DEFAULT_VERTEX_TOLERANCE = 1e-6f;

STLLoader::STLLoader()
    : format(Unknown)
    , autoCenter(true)          // By default, center the model on screen
    , autoNormalize(false)      // Don't resize by default
    , calculateNormals(false)   // Use normals from file by default
    , mergeVertices(true)       // Combine duplicate points by default
    , vertexTolerance(DEFAULT_VERTEX_TOLERANCE)
{
}

STLLoader::~STLLoader()
{
    clear();
}

void STLLoader::clear()
{
    // Throw away all the data and start fresh
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
    
    qDebug() << "Starting to load STL file:" << fileName;
    
    // Make sure the file actually exists and we can read it
    QFileInfo fileInfo(fileName);
    if (!fileInfo.exists()) {
        setError("File does not exist: " + fileName);
        return FileNotFound;
    }
    
    if (!fileInfo.isReadable()) {
        setError("Cannot read file (check permissions): " + fileName);
        return CannotOpenFile;
    }
    
    if (fileInfo.size() == 0) {
        setError("File is empty: " + fileName);
        return EmptyFile;
    }
    
    qDebug() << "File looks good, size:" << fileInfo.size() << "bytes";
    
    // Figure out if this is a binary or text STL file
    format = detectFormat(fileName);
    if (format == Unknown) {
        setError("This doesn't look like a valid STL file");
        return InvalidFormat;
    }
    
    qDebug() << "File format detected:" << (format == Binary ? "Binary STL" : "Text STL");
    
    // Open the file with the right settings
    QFile file(fileName);
    QIODevice::OpenMode openMode = (format == Binary) ? QIODevice::ReadOnly : (QIODevice::ReadOnly | QIODevice::Text);
    
    if (!file.open(openMode)) {
        setError("Cannot open file: " + file.errorString());
        return CannotOpenFile;
    }
    
    LoadResult result = Success;
    
    // Load the file using the appropriate method
    try {
        if (format == Binary) {
            result = loadBinarySTL(file);
        } else if (format == ASCII) {
            result = loadASCIISTL(file);
        }
        
        if (result == Success) {
            qDebug() << "File loaded successfully, now processing the triangles...";
            processTriangles();
            qDebug() << "All done processing";
        }
        
    } catch (const std::exception& e) {
        setError(QString("Something went wrong while reading: ") + e.what());
        result = ReadError;
    } catch (...) {
        setError("Unknown error occurred while reading file");
        result = ReadError;
    }
    
    file.close();
    
    if (result != Success) {
        clear();  // Something went wrong, throw away partial data
    } else {
        qDebug() << "Success! Loaded" << triangles.size() << "triangles with" << vertices.size() << "vertices";
    }
    
    return result;
}

STLLoader::STLFormat STLLoader::detectFormat(const QString& fileName)
{
    // Try binary detection first (it's more reliable)
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
    
    // Binary STL files have a very specific structure
    qint64 fileSize = file.size();
    if (fileSize < BINARY_STL_HEADER_SIZE + 4) { // Need at least header + triangle count
        file.close();
        return false;
    }
    
    // Skip the 80-byte header (it's usually garbage)
    file.seek(BINARY_STL_HEADER_SIZE);
    
    // Read how many triangles the file claims to have
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    
    quint32 triangleCount;
    stream >> triangleCount;
    
    // Calculate what the file size should be if this is really binary
    qint64 expectedSize = BINARY_STL_HEADER_SIZE + 4 + (triangleCount * BINARY_STL_TRIANGLE_SIZE);
    
    file.close();
    
    // Binary STL files must match the expected size exactly
    bool isBinary = (fileSize == expectedSize && triangleCount > 0 && triangleCount < 50000000);
    
    if (isBinary) {
        qDebug() << "This looks like binary STL:" << triangleCount << "triangles, expected size matches actual size";
    }
    
    return isBinary;
}

bool STLLoader::isASCIISTL(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    // ASCII STL files always start with "solid"
    QTextStream stream(&file);
    QString firstLine = stream.readLine().trimmed().toLower();
    file.close();
    
    bool isAscii = firstLine.startsWith("solid");
    
    if (isAscii) {
        qDebug() << "This looks like text STL, first line:" << firstLine;
    }
    
    return isAscii;
}

STLLoader::LoadResult STLLoader::loadBinarySTL(QFile& file)
{
    qDebug() << "Reading binary STL file...";
    
    if (file.size() < BINARY_STL_HEADER_SIZE + 4) {
        setError("File is too small to be a valid binary STL");
        return CorruptedFile;
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);  // STL uses little-endian byte order
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    
    // Jump past the 80-byte header (it's usually meaningless)
    file.seek(BINARY_STL_HEADER_SIZE);
    
    // Read the number of triangles
    quint32 triangleCount;
    stream >> triangleCount;
    
    if (triangleCount == 0) {
        setError("This STL file contains no triangles");
        return EmptyFile;
    }
    
    if (triangleCount > 50000000) {
        setError("This file claims to have an unreasonable number of triangles: " + QString::number(triangleCount));
        return CorruptedFile;
    }
    
    qDebug() << "File contains" << triangleCount << "triangles";
    
    // Make room for all the triangles we're about to read
    triangles.reserve(triangleCount);
    
    // Read each triangle
    for (quint32 i = 0; i < triangleCount; ++i) {
        STLTriangle triangle;
        
        // Each triangle starts with its normal vector (surface direction)
        float nx, ny, nz;
        stream >> nx >> ny >> nz;
        
        // Make sure the numbers make sense (not corrupted)
        if (qIsNaN(nx) || qIsInf(nx) || qIsNaN(ny) || qIsInf(ny) || qIsNaN(nz) || qIsInf(nz)) {
            qWarning() << "Triangle" << i << "has invalid normal vector - skipping";
            continue;
        }
        
        triangle.normal = QVector3D(nx, ny, nz);
        
        // Now read the three corners of the triangle
        float v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z;
        stream >> v1x >> v1y >> v1z;
        stream >> v2x >> v2y >> v2z;
        stream >> v3x >> v3y >> v3z;
        
        // Check all coordinates for corruption
        float coords[] = {v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z};
        bool hasCorruptedData = false;
        for (int j = 0; j < 9; ++j) {
            if (qIsNaN(coords[j]) || qIsInf(coords[j])) {
                hasCorruptedData = true;
                break;
            }
        }
        
        if (hasCorruptedData) {
            qWarning() << "Triangle" << i << "has corrupted vertex data - skipping";
            continue;
        }
        
        triangle.vertex1 = QVector3D(v1x, v1y, v1z);
        triangle.vertex2 = QVector3D(v2x, v2y, v2z);
        triangle.vertex3 = QVector3D(v3x, v3y, v3z);
        
        // Skip the 2-byte "attribute" field (usually unused)
        quint16 attribute;
        stream >> attribute;
        
        // Make sure we didn't hit any read errors
        if (stream.status() != QDataStream::Ok) {
            setError(QString("Error reading triangle %1 from file").arg(i));
            return ReadError;
        }
        
        // Only keep triangles that actually make sense geometrically
        if (isValidTriangle(triangle)) {
            triangles.append(triangle);
        } else {
            qWarning() << "Triangle" << i << "is degenerate (zero area) - skipping";
        }
        
        // Show progress for big files
        if (i % 10000 == 0 && i > 0) {
            qDebug() << "Read" << i << "/" << triangleCount << "triangles so far...";
        }
    }
    
    if (triangles.isEmpty()) {
        setError("No valid triangles found in this file");
        return EmptyFile;
    }
    
    qDebug() << "Successfully read" << triangles.size() << "valid triangles from binary STL";
    return Success;
}

STLLoader::LoadResult STLLoader::loadASCIISTL(QFile& file)
{
    qDebug() << "Reading text STL file...";
    
    QTextStream stream(&file);
    QString line;
    int lineNumber = 0;
    
    // First line should be "solid <optional name>"
    line = stream.readLine().trimmed();
    lineNumber++;
    
    if (!line.toLower().startsWith("solid")) {
        setError(QString("Text STL should start with 'solid' but line %1 says: %2").arg(lineNumber).arg(line));
        return InvalidFormat;
    }
    
    STLTriangle currentTriangle;
    int vertexCount = 0;
    bool inFacet = false;     // Are we currently reading a triangle?
    bool inLoop = false;      // Are we currently reading the triangle's vertices?
    int trianglesParsed = 0;
    
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        lineNumber++;
        
        if (line.isEmpty() || line.startsWith("#")) {
            continue; // Skip blank lines and comments
        }
        
        QString keyword;
        QStringList values;
        if (!parseASCIILine(line, keyword, values)) {
            continue; // Skip lines we can't understand
        }
        
        if (keyword == "facet" && values.size() >= 4 && values[0] == "normal") {
            // Starting a new triangle
            if (inFacet) {
                setError(QString("Line %1: Found new triangle before finishing previous one").arg(lineNumber));
                return CorruptedFile;
            }
            
            // Read the surface normal (direction the triangle faces)
            bool ok1, ok2, ok3;
            float nx = values[1].toFloat(&ok1);
            float ny = values[2].toFloat(&ok2);
            float nz = values[3].toFloat(&ok3);
            
            if (!ok1 || !ok2 || !ok3) {
                qWarning() << "Line" << lineNumber << ": Can't read normal vector, using default";
                nx = ny = nz = 0.0f;
            }
            
            // Fix any corrupted values
            if (qIsNaN(nx) || qIsInf(nx)) nx = 0.0f;
            if (qIsNaN(ny) || qIsInf(ny)) ny = 0.0f;
            if (qIsNaN(nz) || qIsInf(nz)) nz = 0.0f;
            
            currentTriangle.normal = QVector3D(nx, ny, nz);
            inFacet = true;
            vertexCount = 0;
            
        } else if (keyword == "outer" && values.size() >= 1 && values[0] == "loop") {
            // Starting to read the triangle's corner points
            if (!inFacet || inLoop) {
                setError(QString("Line %1: 'outer loop' in wrong place").arg(lineNumber));
                return CorruptedFile;
            }
            inLoop = true;
            
        } else if (keyword == "vertex" && values.size() >= 3) {
            // Reading one corner point of the triangle
            if (!inLoop) {
                setError(QString("Line %1: Found vertex outside of loop").arg(lineNumber));
                return CorruptedFile;
            }
            
            bool ok1, ok2, ok3;
            float vx = values[0].toFloat(&ok1);
            float vy = values[1].toFloat(&ok2);
            float vz = values[2].toFloat(&ok3);
            
            if (!ok1 || !ok2 || !ok3) {
                setError(QString("Line %1: Cannot read vertex coordinates").arg(lineNumber));
                return CorruptedFile;
            }
            
            // Check for corrupted coordinate values
            if (qIsNaN(vx) || qIsInf(vx) || qIsNaN(vy) || qIsInf(vy) || qIsNaN(vz) || qIsInf(vz)) {
                setError(QString("Line %1: Vertex has corrupted coordinates").arg(lineNumber));
                return CorruptedFile;
            }
            
            QVector3D vertex(vx, vy, vz);
            
            // Store this vertex in the right position
            if (vertexCount == 0) {
                currentTriangle.vertex1 = vertex;
            } else if (vertexCount == 1) {
                currentTriangle.vertex2 = vertex;
            } else if (vertexCount == 2) {
                currentTriangle.vertex3 = vertex;
            } else {
                setError(QString("Line %1: Triangle has too many vertices").arg(lineNumber));
                return CorruptedFile;
            }
            vertexCount++;
            
        } else if (keyword == "endloop") {
            // Finished reading the triangle's vertices
            if (!inLoop) {
                setError(QString("Line %1: 'endloop' without matching 'outer loop'").arg(lineNumber));
                return CorruptedFile;
            }
            
            if (vertexCount != 3) {
                setError(QString("Line %1: Triangle has %2 vertices but should have exactly 3").arg(lineNumber).arg(vertexCount));
                return CorruptedFile;
            }
            
            inLoop = false;
            
        } else if (keyword == "endfacet") {
            // Finished reading this triangle completely
            if (!inFacet || inLoop) {
                setError(QString("Line %1: 'endfacet' without proper triangle structure").arg(lineNumber));
                return CorruptedFile;
            }
            
            // Check if this triangle makes geometric sense and save it
            if (isValidTriangle(currentTriangle)) {
                triangles.append(currentTriangle);
                trianglesParsed++;
                
                // Show progress for large files
                if (trianglesParsed % 1000 == 0) {
                    qDebug() << "Parsed" << trianglesParsed << "triangles so far...";
                }
            } else {
                qWarning() << "Line" << lineNumber << ": Triangle has zero area - skipping";
            }
            
            inFacet = false;
            
        } else if (keyword == "endsolid") {
            break; // We've reached the end of the model
        }
    }
    
    if (triangles.isEmpty()) {
        setError("No valid triangles found in text STL file");
        return EmptyFile;
    }
    
    qDebug() << "Successfully read" << triangles.size() << "valid triangles from text STL";
    return Success;
}

void STLLoader::processTriangles()
{
    if (triangles.isEmpty()) {
        qWarning() << "No triangles to process";
        return;
    }
    
    qDebug() << "Processing" << triangles.size() << "triangles...";
    
    // First, figure out how big the model is and where it sits
    calculateBoundingBox();
    
    // Move the model to the center of the screen if requested
    if (autoCenter) {
        qDebug() << "Moving model to center of screen...";
        centerModel();
    }
    
    // Scale the model to a standard size if requested
    if (autoNormalize) {
        qDebug() << "Scaling model to standard size...";
        normalizeModel();
    }
    
    // Convert triangles into format that graphics card can use efficiently
    qDebug() << "Converting to graphics format...";
    generateVertexBuffer();
    
    // Combine duplicate vertices to save memory if requested
    if (mergeVertices) {
        qDebug() << "Removing duplicate vertices...";
        generateIndices();
    }
    
    qDebug() << "Processing complete. Final model has" << vertices.size() << "vertices";
}

void STLLoader::calculateBoundingBox()
{
    boundingBox.reset();
    
    // Look at every corner of every triangle to find the extremes
    for (const STLTriangle& triangle : triangles) {
        boundingBox.update(triangle.vertex1);
        boundingBox.update(triangle.vertex2);
        boundingBox.update(triangle.vertex3);
    }
    
    // Calculate the center, size, etc.
    boundingBox.finalize();
    
    qDebug() << "Model bounding box calculated:";
    qDebug() << "  Bottom-left-back corner:" << boundingBox.min;
    qDebug() << "  Top-right-front corner:" << boundingBox.max;
    qDebug() << "  Center point:" << boundingBox.center;
    qDebug() << "  Dimensions:" << boundingBox.size;
    qDebug() << "  Largest dimension:" << boundingBox.maxDimension;
}

void STLLoader::centerModel()
{
    if (!boundingBox.isValid()) {
        qWarning() << "Cannot center model - bounding box is invalid";
        return;
    }
    
    // Calculate how far to move the model to center it at (0,0,0)
    QVector3D offset = -boundingBox.center;
    
    // Move every triangle by this offset
    for (STLTriangle& triangle : triangles) {
        triangle.vertex1 += offset;
        triangle.vertex2 += offset;
        triangle.vertex3 += offset;
    }
    
    // Update our bounding box info
    boundingBox.min += offset;
    boundingBox.max += offset;
    boundingBox.center = QVector3D(0, 0, 0);
    
    qDebug() << "Model centered by moving it" << offset;
}

void STLLoader::normalizeModel()
{
    if (!boundingBox.isValid() || boundingBox.maxDimension <= 0) {
        qWarning() << "Cannot resize model - invalid dimensions";
        return;
    }
    
    // Scale factor to make the largest dimension equal to 2 (so model fits in -1 to +1 box)
    float scale = 2.0f / boundingBox.maxDimension;
    
    // Scale every triangle by this factor
    for (STLTriangle& triangle : triangles) {
        triangle.vertex1 *= scale;
        triangle.vertex2 *= scale;
        triangle.vertex3 *= scale;
    }
    
    // Update our bounding box info
    boundingBox.min *= scale;
    boundingBox.max *= scale;
    boundingBox.center *= scale;
    boundingBox.size *= scale;
    boundingBox.maxDimension *= scale;
    
    qDebug() << "Model scaled by factor of" << scale;
}

void STLLoader::generateVertexBuffer()
{
    vertices.clear();
    vertexData.clear();
    
    int totalVertices = triangles.size() * 3;  // Each triangle has 3 corners
    vertices.reserve(totalVertices);
    vertexData.reserve(totalVertices * 6); // Each vertex needs 6 numbers: 3 for position, 3 for normal
    
    for (const STLTriangle& triangle : triangles) {
        QVector3D normal = triangle.normal;
        
        // If the file didn't provide good normals, or we want to recalculate them
        if (calculateNormals || normal.lengthSquared() < 0.001f) {
            normal = calculateTriangleNormal(triangle.vertex1, triangle.vertex2, triangle.vertex3);
        }
        
        // Make sure the normal vector has length 1
        if (normal.lengthSquared() > 0.001f) {
            normal.normalize();
        } else {
            // If we can't calculate a good normal, use a default
            normal = QVector3D(0, 0, 1);
        }
        
        // Create vertex objects for each corner of the triangle
        vertices.append(STLVertex(triangle.vertex1, normal));
        vertices.append(STLVertex(triangle.vertex2, normal));
        vertices.append(STLVertex(triangle.vertex3, normal));
        
        // Also create the raw data array that OpenGL graphics can use directly
        // For each vertex: x, y, z, normal_x, normal_y, normal_z
        
        // First corner
        vertexData.append(triangle.vertex1.x());
        vertexData.append(triangle.vertex1.y());
        vertexData.append(triangle.vertex1.z());
        vertexData.append(normal.x());
        vertexData.append(normal.y());
        vertexData.append(normal.z());
        
        // Second corner
        vertexData.append(triangle.vertex2.x());
        vertexData.append(triangle.vertex2.y());
        vertexData.append(triangle.vertex2.z());
        vertexData.append(normal.x());
        vertexData.append(normal.y());
        vertexData.append(normal.z());
        
        // Third corner
        vertexData.append(triangle.vertex3.x());
        vertexData.append(triangle.vertex3.y());
        vertexData.append(triangle.vertex3.z());
        vertexData.append(normal.x());
        vertexData.append(normal.y());
        vertexData.append(normal.z());
    }
    
    qDebug() << "Created vertex buffer with" << vertices.size() << "vertices (" << vertexData.size() << "numbers total)";
}

void STLLoader::generateIndices()
{
    if (vertices.isEmpty()) {
        qWarning() << "Cannot create indices - no vertices available";
        return;
    }
    
    QVector<STLVertex> uniqueVertices;
    indices.clear();
    
    uniqueVertices.reserve(vertices.size() / 2); // Rough guess at number of unique vertices
    indices.reserve(vertices.size());
    
    // Go through every vertex and either find an existing identical one or add it as new
    for (const STLVertex& vertex : vertices) {
        int index = findOrAddVertex(uniqueVertices, vertex);
        indices.append(index);
    }
    
    // Replace our vertex list with just the unique ones
    vertices = uniqueVertices;
    
    // Rebuild the raw data array for the unique vertices
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
    
    qDebug() << "Created" << indices.size() << "indices pointing to" << vertices.size() << "unique vertices";
}

QVector3D STLLoader::calculateTriangleNormal(const QVector3D& v1, const QVector3D& v2, const QVector3D& v3)
{
    // Calculate which direction this triangle face is pointing
    // We do this using the "cross product" of two edges of the triangle
    QVector3D edge1 = v2 - v1;  // Vector from first to second corner
    QVector3D edge2 = v3 - v1;  // Vector from first to third corner
    QVector3D normal = QVector3D::crossProduct(edge1, edge2);
    
    if (normal.lengthSquared() > 1e-12f) {
        normal.normalize();  // Make it length 1
    } else {
        // Triangle is too small or degenerate, use default normal
        normal = QVector3D(0, 0, 1);
    }
    
    return normal;
}

void STLLoader::setError(const QString& error)
{
    errorString = error;
    qWarning() << "STL Loader Error:" << error;
}

bool STLLoader::isValidTriangle(const STLTriangle& triangle)
{
    // Check if this triangle actually has area (isn't just a line or point)
    QVector3D edge1 = triangle.vertex2 - triangle.vertex1;
    QVector3D edge2 = triangle.vertex3 - triangle.vertex1;
    QVector3D cross = QVector3D::crossProduct(edge1, edge2);
    
    float area = cross.length() * 0.5f;
    
    // If area is too small, this triangle is useless
    if (area < 1e-10f) {
        return false;
    }
    
    // Check if any two vertices are the same (which would make area zero)
    if ((triangle.vertex1 - triangle.vertex2).lengthSquared() < 1e-12f ||
        (triangle.vertex2 - triangle.vertex3).lengthSquared() < 1e-12f ||
        (triangle.vertex3 - triangle.vertex1).lengthSquared() < 1e-12f) {
        return false;
    }
    
    return true;
}

int STLLoader::findOrAddVertex(QVector<STLVertex>& uniqueVertices, const STLVertex& vertex)
{
    // Look through existing vertices to see if we already have this one
    for (int i = 0; i < uniqueVertices.size(); ++i) {
        if (verticesEqual(uniqueVertices[i], vertex, vertexTolerance)) {
            return i;  // Found existing vertex, return its index
        }
    }
    
    // This is a new vertex, add it to the list
    uniqueVertices.append(vertex);
    return uniqueVertices.size() - 1;  // Return index of the new vertex
}

bool STLLoader::verticesEqual(const STLVertex& a, const STLVertex& b, float tolerance)
{
    // Two vertices are considered the same if they're very close together
    return (a.position - b.position).lengthSquared() < tolerance * tolerance;
}

bool STLLoader::parseASCIILine(const QString& line, QString& keyword, QStringList& values)
{
    // Split a line like "vertex 1.0 2.0 3.0" into keyword="vertex" and values=["1.0", "2.0", "3.0"]
    QStringList tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    
    if (tokens.isEmpty()) {
        return false;
    }
    
    keyword = tokens[0].toLower();  // First word is the command
    values = tokens.mid(1);         // Rest are the parameters
    
    return true;
}

QString STLLoader::getFormatString() const
{
    // Convert the format enum to human-readable text
    switch (format) {
        case Binary: return "Binary STL";
        case ASCII: return "Text STL";
        default: return "Unknown format";
    }
}