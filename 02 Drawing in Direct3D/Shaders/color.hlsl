// 隐式填充为 256B
// 矩阵变量 gWorldViewProj 存于常量缓冲区（constant buffer）内
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj; // 4*4*4=64B
};

// // 显式填充为 256B
// cbuffer cbPerObject : register(b0)
// {
//   float4x4 gWorldViewProj;
//   float4x4 Pad0;
//   float4x4 Pad1;
//   float4x4 Pad2;
// };

struct VertexIn
{
    float3 PosL : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    // 把顶点变换到齐次裁剪空间
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    // 直接将顶点的颜色信息传至像素着色器
    vout.Color = vin.Color;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}
