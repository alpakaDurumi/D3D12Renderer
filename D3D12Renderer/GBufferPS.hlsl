Texture2D g_textures[] : register(t0, space0);

// Dynamic samplers for ordinary texture sampling
SamplerState g_samplers[] : register(s0, space0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1; // Do not interpolate w component of tangent vector.
    float distView : TEXCOORD2; // Distance in view space for determining CSM index.
    nointerpolation uint materialIndex : INSTANCE_MATERIAL_INDEX;
};

struct GBufferOutput
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;             // pack with shininess
    float4 materialAmbient : SV_Target2;
    float4 materialSpecular : SV_Target3;
};

cbuffer CameraConstantBuffer : register(b0, space0)
{
    float3 cameraPos;
    float4x4 view;
    float4x4 projection;
}

struct MaterialConstants
{
    float3 materialAmbient;
    float3 materialSpecular;
    float shininess;
    uint4 textureIndices;
    uint4 samplerIndices;
    float4 textureTileScales;
};
ConstantBuffer<MaterialConstants> MaterialConstantBuffers[] : register(b0, space1);

// Parallax Occlusion Mapping
float2 ParallaxMapping(float2 texCoord, float3 toCamera, uint heightMapIdx, uint heightMapSamplerIdx)
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
    
    float currentHeightMapValue = 1.0f - g_textures[heightMapIdx].SampleGrad(g_samplers[heightMapSamplerIdx], currentTexCoord, dx, dy).r;
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
        currentHeightMapValue = 1.0f - g_textures[heightMapIdx].SampleGrad(g_samplers[heightMapSamplerIdx], currentTexCoord, dx, dy).r;
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

GBufferOutput main(PSInput input)
{
    GBufferOutput output;
    
    uint materialIdx = input.materialIndex;
    
    uint4 textureIndices = MaterialConstantBuffers[materialIdx].textureIndices;
    uint albedoIdx = textureIndices[0];
    uint normalMapIdx = textureIndices[1];
    uint heightMapIdx = textureIndices[2];
    
    uint4 samplerIndices = MaterialConstantBuffers[materialIdx].samplerIndices;
    uint albedoSamplerIdx = samplerIndices[0];
    uint normalMapSamplerIdx = samplerIndices[1];
    uint heightMapSamplerIdx = samplerIndices[2];
    
    float4 textureTileScales = MaterialConstantBuffers[materialIdx].textureTileScales;
    float albedoScale = textureTileScales[0];
    float normalMapScale = textureTileScales[1];
    float heightMapScale = textureTileScales[2];
    
    // For POM, use inaccurate inverse-TBN
    float3 iT = normalize(input.tangentWorld);
    float3 iN = normalize(input.normalWorld);
    iT = normalize(iT - dot(iT, iN) * iN); // Gram-Schmidt process
    float3 iB = input.tangentW * cross(iN, iT); // W represents handedness
    float3x3 iiTBN = transpose(float3x3(iT, iB, iN)); // Inverse of orthogonal matrix is same as transpose.
    
    float3 toCameraWorld = normalize(cameraPos - input.posWorld);
    float3 toCameraTangent = normalize(mul(toCameraWorld, iiTBN));
    
    float2 texCoord = ParallaxMapping(input.texCoord * heightMapScale, toCameraTangent, heightMapIdx, heightMapSamplerIdx);
    float2 albedoTexCoord = texCoord * albedoScale / heightMapScale;
    float2 normalMapTexCoord = texCoord * normalMapScale / heightMapScale;
    
    // Set up TBN matrix
    float3 B = input.tangentW * cross(input.normalWorld, input.tangentWorld);
    float3x3 TBN = float3x3(input.tangentWorld, B, input.normalWorld);
    
    // Sample textures
    float3 texColor = g_textures[albedoIdx].Sample(g_samplers[albedoSamplerIdx], albedoTexCoord).rgb;
    float3 normal = g_textures[normalMapIdx].Sample(g_samplers[normalMapSamplerIdx], normalMapTexCoord).rgb * 2.0f - 1.0f;
    float3 normalWorld = normalize(mul(normal, TBN));
    
    output.albedo = float4(texColor, 1.0f);
    output.normal = float4(normalWorld, MaterialConstantBuffers[materialIdx].shininess);
    output.materialAmbient = float4(MaterialConstantBuffers[materialIdx].materialAmbient, 1.0f);
    output.materialSpecular = float4(MaterialConstantBuffers[materialIdx].materialSpecular, 1.0f);
    
    return output;
}