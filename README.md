# 跟着龙书学 DirectX 12

```shell
git clone --recurse-submodules -j8 https://github.com/Latias94/LearnDirectX12
```

## 示例

* [01 DirectX12 Initialization](./01_DirectX12_Initialization)
* [02 Drawing in Direct3D](./02_Drawing_in_Direct3D-Box)
* [03 Drawing in Direct3D Part II - Shapes](./03_Drawing_in_Direct3D_Part_II-Shapes)
* [04 Drawing in Direct3D Part II - LandAndWaves](./04_Drawing_in_Direct3D_Part_II-LandAndWaves)

## Build

* Clion with MSVC toolchain
* [Visual Studio 中的 CMake 项目](https://docs.microsoft.com/zh-cn/cpp/build/cmake-projects-in-visual-studio?view=msvc-170&viewFallbackFrom=vs-2019)
* `cmake -G "Visual Studio 17 2022" -A x64` or other target mentioned in [doc](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html)
* install [XMake](https://xmake.io/) and Clang
  * `cd '.\01_DirectX12_Initialization\'` `xmake && xmake run`
  * ref: [Windows渲染引擎开发入门教学（4.5）: 更舒适的编译流程](https://zhuanlan.zhihu.com/p/495864590)
  * vscode 安装 Clangd 插件，`cd '.\01_DirectX12_Initialization\'` `code .`

## Reference

* [《DirectX 12 3D 游戏开发实战》](https://book.douban.com/subject/30426701/)
* [d3dcoder/d3d12book](https://github.com/d3dcoder/d3d12book)  
* [DirectX11-With-Windows-SDK-Book](https://mkxjun.github.io/DirectX11-With-Windows-SDK-Book/)
* [从零开始手敲次世代游戏引擎](https://zhuanlan.zhihu.com/p/28589792) 系列
