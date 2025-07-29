#version 330 core

// Input vertex attributes
layout (location = 0) in vec3 a_position;    // Vertex position
layout (location = 1) in vec3 a_normal;      // Vertex normal

// Uniforms - transformation matrices
uniform mat4 u_mvpMatrix;       // Model-View-Projection matrix
uniform mat4 u_modelMatrix;     // Model matrix (for world space calculations)
uniform mat4 u_viewMatrix;      // View matrix
uniform mat4 u_normalMatrix;    // Normal transformation matrix (inverse transpose of model)

// Uniforms - lighting
uniform vec3 u_viewPos;         // Camera/view position in world space
uniform vec3 u_lightPos;        // Light position in world space

// Output to fragment shader
out vec3 v_fragPos;             // Fragment position in world space
out vec3 v_normal;              // Transformed normal in world space
out vec3 v_viewPos;             // View position (passed through)
out vec3 v_lightPos;            // Light position (passed through)

// Additional outputs for advanced lighting
out vec3 v_viewDir;             // Direction from fragment to camera
out vec3 v_lightDir;            // Direction from fragment to light
out float v_distance;           // Distance from fragment to light

void main()
{
    // Transform vertex position to world space
    vec4 worldPos = u_modelMatrix * vec4(a_position, 1.0);
    v_fragPos = worldPos.xyz;
    
    // Transform normal to world space using normal matrix
    // Normal matrix is the inverse transpose of the model matrix
    // This preserves normal direction under non-uniform scaling
    v_normal = normalize(mat3(u_normalMatrix) * a_normal);
    
    // Pass view and light positions to fragment shader
    v_viewPos = u_viewPos;
    v_lightPos = u_lightPos;
    
    // Pre-calculate lighting vectors for efficiency
    v_viewDir = normalize(u_viewPos - v_fragPos);
    v_lightDir = normalize(u_lightPos - v_fragPos);
    v_distance = length(u_lightPos - v_fragPos);
    
    // Transform vertex to clip space
    gl_Position = u_mvpMatrix * vec4(a_position, 1.0);
}