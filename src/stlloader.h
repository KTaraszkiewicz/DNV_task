#ifndef STLLOADER_H
#define STLLOADER_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QFile>
#include <QTextStream>
#include <QDataStream>

struct STLTriangle {
    QVector3D normal;
    QVector3D vertex1;
    QVector3D vertex2;
    QVector3D vertex3;
    
    STLTriangle() = default;
    STLTriangle(const QVector3D& n, const QVector3D& v1, const QVector3D& v2, const QVector3D& v3)
        : normal(n), vertex1(v1), vertex2(v2), vertex3(v3) {}
};

struct STLVertex {
    QVector3D position;
    QVector3D normal;
    
    STLVertex() = default;
    STLVertex(const QVector3D& pos, const QVector3D& norm)
        : position(pos), normal(norm) {}
};

struct BoundingBox {
    QVector3D min;
    QVector3D max;
    QVector3D center;
    QVector3D size;
    float maxDimension;
    
    BoundingBox() {
        reset();
    }
    
    void reset() {
        min = QVector3D(FLT_MAX, FLT_MAX, FLT_MAX);
        max = QVector3D(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        center = QVector3D(0, 0, 0);
        size = QVector3D(0, 0, 0);
        maxDimension = 0.0f;
    }
    
    void update(const QVector3D& point) {
        min.setX(qMin(min.x(), point.x()));
        min.setY(qMin(min.y(), point.y()));
        min.setZ(qMin(min.z(), point.z()));
        
        max.setX(qMax(max.x(), point.x()));
        max.setY(qMax(max.y(), point.y()));
        max.setZ(qMax(max.z(), point.z()));
    }
    
    void finalize() {
        center = (min + max) * 0.5f;
        size = max - min;
        maxDimension = qMax(qMax(size.x(), size.y()), size.z());
    }
    
    bool isValid() const {
        return min.x() != FLT_MAX && max.x() != -FLT_MAX;
    }
};

class STLLoader
{
public:
    enum STLFormat {
        Unknown,
        Binary,
        ASCII
    };
    
    enum LoadResult {
        Success,
        FileNotFound,
        CannotOpenFile,
        InvalidFormat,
        CorruptedFile,
        EmptyFile,
        UnsupportedFormat,
        ReadError
    };

public:
    STLLoader();
    ~STLLoader();

    // Main loading functions
    LoadResult loadFile(const QString& fileName);
    void clear();
    
    // Format detection
    STLFormat detectFormat(const QString& fileName);
    static bool isBinarySTL(const QString& fileName);
    static bool isASCIISTL(const QString& fileName);
    
    // Data access
    const QVector<STLTriangle>& getTriangles() const { return triangles; }
    const QVector<STLVertex>& getVertices() const { return vertices; }
    const QVector<float>& getVertexData() const { return vertexData; }
    const QVector<unsigned int>& getIndices() const { return indices; }
    const BoundingBox& getBoundingBox() const { return boundingBox; }
    
    // Statistics
    int getTriangleCount() const { return triangles.size(); }
    int getVertexCount() const { return vertices.size(); }
    QString getFileName() const { return fileName; }
    STLFormat getFormat() const { return format; }
    QString getFormatString() const;
    QString getErrorString() const { return errorString; }
    
    // Options
    void setAutoCenter(bool enable) { autoCenter = enable; }
    void setAutoNormalize(bool enable) { autoNormalize = enable; }
    void setCalculateNormals(bool enable) { calculateNormals = enable; }
    void setMergeVertices(bool enable) { mergeVertices = enable; }
    void setVertexTolerance(float tolerance) { vertexTolerance = tolerance; }
    
    bool getAutoCenter() const { return autoCenter; }
    bool getAutoNormalize() const { return autoNormalize; }
    bool getCalculateNormals() const { return calculateNormals; }
    bool getMergeVertices() const { return mergeVertices; }
    float getVertexTolerance() const { return vertexTolerance; }

private:
    // Loading implementation
    LoadResult loadBinarySTL(QFile& file);
    LoadResult loadASCIISTL(QFile& file);
    
    // Data processing
    void processTriangles();
    void calculateBoundingBox();
    void centerModel();
    void normalizeModel();
    void generateVertexBuffer();
    void generateIndices();
    QVector3D calculateTriangleNormal(const QVector3D& v1, const QVector3D& v2, const QVector3D& v3);
    
    // Utility functions
    void setError(const QString& error);
    bool isValidTriangle(const STLTriangle& triangle);
    int findOrAddVertex(QVector<STLVertex>& uniqueVertices, const STLVertex& vertex);
    bool verticesEqual(const STLVertex& a, const STLVertex& b, float tolerance);
    
    // Parsing helpers for ASCII
    QVector3D parseVector3D(const QStringList& tokens, int startIndex);
    bool parseASCIILine(const QString& line, QString& keyword, QStringList& values);
    
    // Data members
    QVector<STLTriangle> triangles;
    QVector<STLVertex> vertices;
    QVector<float> vertexData;        // Interleaved position + normal data for OpenGL
    QVector<unsigned int> indices;    // Index buffer for OpenGL
    BoundingBox boundingBox;
    
    QString fileName;
    STLFormat format;
    QString errorString;
    
    // Processing options
    bool autoCenter;
    bool autoNormalize;
    bool calculateNormals;
    bool mergeVertices;
    float vertexTolerance;
    
    // Constants
    static const quint32 BINARY_STL_HEADER_SIZE = 80;
    static const quint32 BINARY_STL_TRIANGLE_SIZE = 50; // 4 bytes per float * 12 floats + 2 bytes attribute
    static const char* ASCII_STL_HEADER;
    static const float DEFAULT_VERTEX_TOLERANCE;
};

#endif // STLLOADER_H