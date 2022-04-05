#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

// 程序性几何体的生成代码
// 程序性几何体（procedural geometry，也有译作过程化几何体。这个词的译法较多，大意就是
// “根据用户提供的参数以程序自动生成对应的几何体”

// 一个工具类，用于生成如栅格、球体、柱体以及长方体这类简单的几何体，在我们的演示程序中将常常见到它们的身影。
// 此类将数据生成在系统内存中，而我们必须将这些数据复制到顶点缓冲区和索引缓冲区内。GeometryGenerator 类还可创建出
// 一些后续章节要用到的顶点数据，由于我们当前的演示程序中还用不到它们，所以暂时不会将这些数据复制到顶点缓冲区。
class GeometryGenerator
{
  public:
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;

    struct Vertex
    {
        Vertex()
        {
        }
        // clang-format off
        Vertex(
            const DirectX::XMFLOAT3& p,
            const DirectX::XMFLOAT3& n,
            const DirectX::XMFLOAT3& t,
            const DirectX::XMFLOAT2& uv) :
              Position(p),
              Normal(n),
              TangentU(t),
              TexC(uv) {}

        Vertex(
            float px, float py, float pz,
            float nx, float ny, float nz,
            float tx, float ty, float tz,
            float u, float v) :
              Position(px,py,pz),
              Normal(nx,ny,nz),
              TangentU(tx, ty, tz),
              TexC(u,v) {}
        // clang-format on

        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        // 切线
        DirectX::XMFLOAT3 TangentU;
        DirectX::XMFLOAT2 TexC;
    };

    // MeshData 是一个嵌套在 GeometryGenerator 类中用于存储顶点列表和索引列表的简易结构体
    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<uint32> Indices32;
        std::vector<uint16> &GetIndices16()
        {
            if (mIndices16.empty())
            {
                mIndices16.resize(Indices32.size());
                for (size_t i = 0; i < Indices32.size(); ++i)
                    mIndices16[i] = static_cast<uint16>(Indices32[i]);
            }

            return mIndices16;
        }

      private:
        std::vector<uint16> mIndices16;
    };

    /// <summary>
    /// Creates a box centered at the origin with the given dimensions, where each
    /// face has m rows and n columns of vertices.
    /// </summary>
    MeshData CreateBox(float width, float height, float depth, uint32 numSubdivisions);

    /// <summary>
    /// Creates a sphere centered at the origin with the given radius.  The
    /// slices and stacks parameters control the degree of tessellation.
    /// </summary>
    MeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);

    /// <summary>
    /// Creates a geosphere centered at the origin with the given radius.  The
    /// depth controls the level of tessellation.
    /// </summary>
    MeshData CreateGeosphere(float radius, uint32 numSubdivisions);

    /// <summary>
    /// Creates a cylinder parallel to the y-axis, and centered about the origin.
    /// The bottom and top radius can vary to form various cone shapes rather than true
    /// cylinders. The slices and stacks parameters control the degree of tessellation.
    /// </summary>
    MeshData CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);

    /// <summary>
    /// Creates an mxn grid in the xz-plane with m rows and n columns, centered
    /// at the origin with the specified width and depth.
    /// </summary>
    MeshData CreateGrid(float width, float depth, uint32 m, uint32 n);

    /// <summary>
    /// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
    /// </summary>
    MeshData CreateQuad(float x, float y, float w, float h, float depth);

  private:
    void Subdivide(MeshData& meshData);
    Vertex MidPoint(const Vertex& v0, const Vertex& v1);
    void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount,
                             MeshData &meshData);
    void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount,
                                MeshData &meshData);
};
