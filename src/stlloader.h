#ifndef STLLOADER_H
#define STLLOADER_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QFile>
#include <QTextStream>
#include <QDataStream>

// A triangle in 3D space - the basic building block of 3D models
struct STLTriangle {
    QVector3D normal;    // Direction the triangle face is pointing
    QVector3D vertex1;   // First corner of the triangle
    QVector3D vertex2;   // Second corner of the triangle
    QVector3D vertex3;   // Third corner of the triangle
    
    STLTriangle() = default;
    STLTriangle(const QVector3D& n, const QVector3D& v1, const QVector3D& v2, const QVector3D& v3)
        : normal(n), vertex1(v1), vertex2(v2), vertex3(v3) {}
};

// A single point in 3D space with information about surface direction
struct STLVertex {
    QVector3D position;  // Where this point is in 3D space
    QVector3D normal;    // Which direction the surface faces at this point
    
    STLVertex() = default;
    STLVertex(const QVector3D& pos, const QVector3D& norm)
        : position(pos), normal(norm) {}
};

// A box that completely surrounds the 3D model - useful for centering and sizing
struct BoundingBox {
    QVector3D min;          // Bottom-left-back corner of the box
    QVector3D max;          // Top-right-front corner of the box
    QVector3D center;       // Exact middle point of the model
    QVector3D size;         // How wide, tall, and deep the model is
    float maxDimension;     // The largest dimension (width, height, or depth)
    
    BoundingBox() {
        reset();
    }
    
    // Start over with empty bounding box
    void reset() {
        min = QVector3D(FLT_MAX, FLT_MAX, FLT_MAX);
        max = QVector3D(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        center = QVector3D(0, 0, 0);
        size = QVector3D(0, 0, 0);
        maxDimension = 0.0f;
    }
    
    // Expand bounding box to include this new point
    void update(const QVector3D& point) {
        min.setX(qMin(min.x(), point.x()));
        min.setY(qMin(min.y(), point.y()));
        min.setZ(qMin(min.z(), point.z()));
        
        max.setX(qMax(max.x(), point.x()));
        max.setY(qMax(max.y(), point.y()));
        max.setZ(qMax(max.z(), point.z()));
    }
    
    // Calculate the final center, size, etc. after adding all points
    void finalize() {
        center = (min + max) * 0.5f;
        size = max - min;
        maxDimension = qMax(qMax(size.x(), size.y()), size.z());
    }
    
    // Check if we actually have a valid bounding box
    bool isValid() const {
        return min.x() != FLT_MAX && max.x() != -FLT_MAX;
    }
};

class STLLoader
{
public:
    // STL files can be stored in two different ways
    enum STLFormat {
        Unknown,    // We don't know what format this is
        Binary,     // Compact binary format (smaller files)
        ASCII       // Text format (human readable, larger files)
    };
    
    // All the things that can go wrong when loading a file
    enum LoadResult {
        Success,              // Everything worked perfectly
        FileNotFound,         // The file doesn't exist
        CannotOpenFile,       // File exists but we can't read it
        InvalidFormat,        // This isn't a valid STL file
        CorruptedFile,        // File is damaged or incomplete
        EmptyFile,            // File has no triangles in it
        UnsupportedFormat,    // This STL variant isn't supported
        ReadError             // Something went wrong while reading
    };

public:
    STLLoader();
    ~STLLoader();

    // The main function - load an STL file from disk
    LoadResult loadFile(const QString& fileName);
    void clear();  // Forget everything and start fresh
    
    // Figure out if a file is binary or ASCII format
    STLFormat detectFormat(const QString& fileName);
    static bool isBinarySTL(const QString& fileName);
    static bool isASCIISTL(const QString& fileName);
    
    // Get the loaded 3D model data
    const QVector<STLTriangle>& getTriangles() const { return triangles; }
    const QVector<STLVertex>& getVertices() const { return vertices; }
    const QVector<float>& getVertexData() const { return vertexData; }        // Ready for OpenGL
    const QVector<unsigned int>& getIndices() const { return indices; }       // For efficient drawing
    const BoundingBox& getBoundingBox() const { return boundingBox; }
    
    // Get information about the loaded model
    int getTriangleCount() const { return triangles.size(); }
    int getVertexCount() const { return vertices.size(); }
    QString getFileName() const { return fileName; }
    STLFormat getFormat() const { return format; }
    QString getFormatString() const;  // Get format as readable text
    QString getErrorString() const { return errorString; }  // What went wrong
    
    // Settings for how to process the loaded model
    void setAutoCenter(bool enable) { autoCenter = enable; }          // Move model to center of screen
    void setAutoNormalize(bool enable) { autoNormalize = enable; }    // Scale model to standard size
    void setCalculateNormals(bool enable) { calculateNormals = enable; }  // Recalculate surface directions
    void setMergeVertices(bool enable) { mergeVertices = enable; }     // Combine duplicate points
    void setVertexTolerance(float tolerance) { vertexTolerance = tolerance; }  // How close is "same point"
    
    // Get current settings
    bool getAutoCenter() const { return autoCenter; }
    bool getAutoNormalize() const { return autoNormalize; }
    bool getCalculateNormals() const { return calculateNormals; }
    bool getMergeVertices() const { return mergeVertices; }
    float getVertexTolerance() const { return vertexTolerance; }

private:
    // The actual work of reading binary and text STL files
    LoadResult loadBinarySTL(QFile& file);
    LoadResult loadASCIISTL(QFile& file);
    
    // Clean up and organize the loaded data
    void processTriangles();         // Do all the processing steps
    void calculateBoundingBox();     // Figure out model size and position
    void centerModel();              // Move model to center of screen
    void normalizeModel();           // Scale model to fit nicely
    void generateVertexBuffer();     // Prepare data for graphics card
    void generateIndices();          // Create index list for efficient rendering
    QVector3D calculateTriangleNormal(const QVector3D& v1, const QVector3D& v2, const QVector3D& v3);
    
    // Helper functions
    void setError(const QString& error);  // Record what went wrong
    bool isValidTriangle(const STLTriangle& triangle);  // Check if triangle makes sense
    int findOrAddVertex(QVector<STLVertex>& uniqueVertices, const STLVertex& vertex);  // Avoid duplicate vertices
    bool verticesEqual(const STLVertex& a, const STLVertex& b, float tolerance);  // Are two points the same?
    
    // Help with reading text STL files
    QVector3D parseVector3D(const QStringList& tokens, int startIndex);
    bool parseASCIILine(const QString& line, QString& keyword, QStringList& values);
    
    // All the data we've loaded
    QVector<STLTriangle> triangles;      // All the triangles that make up the model
    QVector<STLVertex> vertices;         // All the unique points
    QVector<float> vertexData;           // Data formatted for OpenGL graphics
    QVector<unsigned int> indices;       // List of which vertices make each triangle
    BoundingBox boundingBox;             // Size and position info
    
    QString fileName;        // Name of file we loaded
    STLFormat format;        // Whether it was binary or text format
    QString errorString;     // What went wrong (if anything)
    
    // User preferences for how to process the model
    bool autoCenter;         // Should we move model to center?
    bool autoNormalize;      // Should we scale model to standard size?
    bool calculateNormals;   // Should we recalculate surface directions?
    bool mergeVertices;      // Should we combine duplicate points?
    float vertexTolerance;   // How close before we consider points identical?
    
    // Important numbers for the STL file format
    static const quint32 BINARY_STL_HEADER_SIZE = 80;      // Binary files start with 80-byte header
    static const quint32 BINARY_STL_TRIANGLE_SIZE = 50;    // Each triangle takes exactly 50 bytes
    static const char* ASCII_STL_HEADER;                   // Text files start with "solid"
    static const float DEFAULT_VERTEX_TOLERANCE;           // Default distance for "same point"
};

#endif // STLLOADER_H