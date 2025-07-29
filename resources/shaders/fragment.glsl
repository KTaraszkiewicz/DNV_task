#version 330 core

// Input from vertex shader
in vec3 v_fragPos;              // Fragment position in world space
in vec3 v_normal;               // Interpolated normal
in vec3 v_viewPos;              // Camera position
in vec3 v_lightPos;             // Light position
in vec3 v_viewDir;              // Direction to camera
in vec3 v_lightDir;             // Direction to light
in float v_distance;            // Distance to light

// Uniforms - lighting properties
uniform vec3 u_lightColor;      // Light color
uniform vec3 u_materialColor;   // Base material color
uniform float u_ambientStrength;    // Ambient light strength
uniform float u_diffuseStrength;    // Diffuse light strength
uniform float u_specularStrength;   // Specular light strength
uniform float u_shininess;          // Material shininess (specular exponent)

// Uniforms - rendering modes
uniform bool u_lightingEnabled; // Enable/disable lighting calculations
uniform bool u_wireframe;       // Wireframe rendering mode

// Uniforms - advanced lighting
uniform float u_lightConstant;   // Light attenuation constant term
uniform float u_lightLinear;     // Light attenuation linear term
uniform float u_lightQuadratic;  // Light attenuation quadratic term

// Material properties for more realistic rendering
uniform float u_metallic;       // Metallic factor (0.0 = dielectric, 1.0 = metallic)
uniform float u_roughness;      // Surface roughness (0.0 = mirror, 1.0 = completely rough)
uniform float u_ao;             // Ambient occlusion factor

// Output color
out vec4 FragColor;

// Function to calculate Blinn-Phong lighting
vec3 calculateBlinnPhong(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 lightColor, vec3 materialColor)
{
    // Ambient lighting
    vec3 ambient = u_ambientStrength * lightColor * materialColor;
    
    // Diffuse lighting (Lambertian)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = u_diffuseStrength * diff * lightColor * materialColor;
    
    // Specular lighting (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), u_shininess);
    vec3 specular = u_specularStrength * spec * lightColor;
    
    return ambient + diffuse + specular;
}

// Function to calculate Phong lighting (alternative)
vec3 calculatePhong(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 lightColor, vec3 materialColor)
{
    // Ambient lighting
    vec3 ambient = u_ambientStrength * lightColor * materialColor;
    
    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = u_diffuseStrength * diff * lightColor * materialColor;
    
    // Specular lighting (Phong reflection model)
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_shininess);
    vec3 specular = u_specularStrength * spec * lightColor;
    
    return ambient + diffuse + specular;
}

// Function to calculate light attenuation
float calculateAttenuation(float distance)
{
    float constant = u_lightConstant > 0.0 ? u_lightConstant : 1.0;
    float linear = u_lightLinear > 0.0 ? u_lightLinear : 0.09;
    float quadratic = u_lightQuadratic > 0.0 ? u_lightQuadratic : 0.032;
    
    return 1.0 / (constant + linear * distance + quadratic * (distance * distance));
}

// Simplified PBR-style material calculation
vec3 calculatePBRMaterial(vec3 baseColor, float metallic, float roughness)
{
    // Simplified PBR material adjustment
    vec3 dielectricSpecular = vec3(0.04); // Common dielectric F0
    vec3 diffuseColor = baseColor * (1.0 - metallic);
    vec3 specularColor = mix(dielectricSpecular, baseColor, metallic);
    
    return diffuseColor;
}

void main()
{
    // Normalize interpolated vectors (they may have been denormalized during interpolation)
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(v_lightDir);
    vec3 viewDir = normalize(v_viewDir);
    
    vec3 finalColor = u_materialColor;
    
    // Check rendering mode
    if (u_wireframe) {
        // Wireframe mode - simple white or colored lines
        finalColor = vec3(1.0, 1.0, 1.0); // White wireframe
    }
    else if (u_lightingEnabled) {
        // Full lighting calculations
        
        // Calculate light attenuation based on distance
        float attenuation = calculateAttenuation(v_distance);
        vec3 attenuatedLightColor = u_lightColor * attenuation;
        
        // Choose lighting model (Blinn-Phong is generally preferred)
        vec3 litColor = calculateBlinnPhong(normal, lightDir, viewDir, 
                                           attenuatedLightColor, u_materialColor);
        
        // Apply ambient occlusion if available
        litColor *= u_ao > 0.0 ? u_ao : 1.0;
        
        finalColor = litColor;
    }
    else {
        // No lighting - flat shading with material color
        finalColor = u_materialColor;
    }
    
    // Gamma correction (optional - makes colors more visually accurate)
    float gamma = 2.2;
    finalColor = pow(finalColor, vec3(1.0/gamma));
    
    // Output final color with full opacity
    FragColor = vec4(finalColor, 1.0);
}