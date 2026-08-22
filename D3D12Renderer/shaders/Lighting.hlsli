#pragma once

#include "LightConstants.hlsli"

// Shading in world space
// TODO : ambient add multiple times, fix this.
float3 PhongReflection(LightConstants light, float3 toLightWorld, float3 toCameraWorld, float lightFactor, float3 texColor, float3 normalWorld,
    float3 materialAmbient, float3 materialSpecular, float shininess)
{
    float3 halfWay = normalize(toLightWorld + toCameraWorld);
    
    // Ambient
    float3 ambient = materialAmbient * texColor;
    
    // Diffuse
    float nDotL = max(dot(normalWorld, toLightWorld), 0.0f);
    float3 diffuse = texColor * nDotL * light.lightColor * light.lightIntensity;
    
    // Specular
    float nDotH = max(dot(normalWorld, halfWay), 0.0f);
    float3 specular = pow(nDotH, shininess) * materialSpecular * light.lightColor * light.lightIntensity;
            
    return ambient + (diffuse + specular) * lightFactor;
}

float CalcAttenuation(float dist, float range)
{
    // Linear attenuation
    static const float fallOffStart = 0.4f;
    
    return saturate((range - dist) / (range - fallOffStart));
}

float CalcAngularAttenuation(LightConstants light, float3 lightToPixel)
{
    float cosTheta = dot(lightToPixel, light.lightDir);
    
    return saturate((cosTheta - light.cosOuterAngle) / (light.cosInnerAngle - light.cosOuterAngle));
}
