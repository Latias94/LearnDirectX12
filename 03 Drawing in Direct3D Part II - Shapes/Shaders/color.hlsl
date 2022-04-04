// 隐式填充为 256B
// 我们也已修改了物体常量缓冲区（即 cbPerObject），使之仅存储一个与物体有关的常量。
// 就目前的情况而言，为了绘制物体，与之唯一相关的常量就是它的世界矩阵
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld; // 4*4*4=64B
};

// 我们的演示程序可能不会用到所有的常量数据，但是它们的存在却使工作变得更加方便，而且提供这些额外的数据也只需少量开销。
// 例如，虽然我们现在无须知道渲染目标的尺寸，但当要实现某些后期处理效果之时，这个信息将会派上用场。
cbuffer cbPass : register(b1)
{
  float4x4 gView;
  float4x4 gInvView;
  float4x4 gProj;
  float4x4 gInvProj;
  float4x4 gViewProj;
  float4x4 gInvViewProj;
  float3 gEyePosW;
  float cbPerObjectPad1;
  float2 gRenderTargetSize;
  float2 gInvRenderTargetSize;
  float gNearZ;
  float gFarZ;
  float gTotalTime;
  float gDeltaTime;
};

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
