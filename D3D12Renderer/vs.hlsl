struct VSInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
#ifdef INSTANCED
	float4x4 instanceWorld : INSTANCE_WORLD;
	float4x4 instanceInverseTranspose : INSTANCE_INVTRANSPOSE;
#endif
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float3 posTangent : POSITION0;
    float3 lightPosTangent : POSITION1;
    float3 cameraPosTangent : POSITION2;
};

cbuffer MeshConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 inverseTranspose;
    float textureTileScale;
};

cbuffer LightConstantBuffer : register(b2)
{
    float3 lightPos;
    float3 lightDir;
    float3 lightColor;
    float lightIntensity;
}

cbuffer CameraConstantBuffer : register(b3)
{
    float3 cameraPos;
    float4x4 viewProjection;
}

PSInput main(VSInput input)
{
    PSInput output;
	
    // Calculate position in world space
    float3 posWorld;
#ifdef INSTANCED
	posWorld = mul(float4(input.pos, 1.0f), mul(world, input.instanceWorld)).xyz;
#else
    posWorld = mul(float4(input.pos, 1.0f), world).xyz;
#endif
    
    output.pos = mul(float4(posWorld, 1.0f), viewProjection);
    output.texCoord = input.texCoord;
    
    // Set up TBN matrix
    float3 T, B, N;
#ifdef INSTANCED
    T = normalize(mul(float4(input.tangent, 0.0f), mul(input.instanceInverseTranspose, inverseTranspose)).xyz);
	N = normalize(mul(float4(input.normal, 0.0f), mul(input.instanceInverseTranspose, inverseTranspose)).xyz);
#else
    T = normalize(mul(float4(input.tangent, 0.0f), inverseTranspose).xyz);
    N = normalize(mul(float4(input.normal, 0.0f), inverseTranspose).xyz);
#endif
    
    T = normalize(T - dot(T, N) * N);   // Gram-Schmidt process
    B = cross(N, T);         // Multiply w element of input.tangent if it exists. (for handedess)
    
    // Transform vectors from world space to tangent space.
    // Since the TBN matrix is orthogonal, its inverse is equal to its transpose.
    float3x3 inverseTBN = transpose(float3x3(T, B, N));
    
    output.posTangent = mul(posWorld, inverseTBN);
    output.lightPosTangent = mul(lightPos, inverseTBN);
    output.cameraPosTangent = mul(cameraPos, inverseTBN);
	
    return output;
}