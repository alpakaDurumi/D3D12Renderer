#pragma once

// Parallax Occlusion Mapping
float2 ParallaxMapping(float2 texCoord, float3 toCamera, Texture2D heightMap, SamplerState heightMapSampler)
{
    static const float heightScale = 0.02f;
    
    // Set numLayers based on view direction
    static const float minLayers = 8.0f;
    static const float maxLayers = 32.0f;
    const float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0f, 0.0f, 1.0f), toCamera)));
    
    float layerStep = 1.0f / numLayers;
    
    // Texture coord offset per layer
    // xy / z = offset / (1.0 * heightScale)
    float2 deltaTexCoord = toCamera.xy / max(toCamera.z, 0.001f) * heightScale / numLayers;
    
    float2 currentTexCoord = texCoord;
    float2 dx = ddx(currentTexCoord);
    float2 dy = ddy(currentTexCoord);
    
    float currentHeightMapValue = 1.0f - heightMap.SampleGrad(heightMapSampler, currentTexCoord, dx, dy).r;
    float currentLayerHeight = 0.0f;
    
    float2 prevTexCoord = currentTexCoord;
    float prevHeightMapValue = currentHeightMapValue;
    float prevLayerHeight = currentLayerHeight;
    
    [loop]
    while (currentLayerHeight < 1.0f && currentLayerHeight < currentHeightMapValue)
    {
        prevTexCoord = currentTexCoord;
        prevHeightMapValue = currentHeightMapValue;
        prevLayerHeight = currentLayerHeight;
        
        currentTexCoord -= deltaTexCoord;
        currentHeightMapValue = 1.0f - heightMap.SampleGrad(heightMapSampler, currentTexCoord, dx, dy).r;
        currentLayerHeight += layerStep;
    }
    
    // Interpolate
    float diffAfter = currentLayerHeight - currentHeightMapValue;
    float diffBefore = prevHeightMapValue - prevLayerHeight;
    
    // Eliminate divide by zero
    float weight = saturate(diffBefore / max(diffAfter + diffBefore, 0.00001f));
    float2 interpolatedTexCoord = lerp(prevTexCoord, currentTexCoord, weight);
    
    return interpolatedTexCoord;
}
