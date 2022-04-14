add_rules("mode.release", "mode.debug")

function Execute(map, func)
    if map ~= nil then
        func(map)
    end
end

function SetException(enableException)
    if (enableException ~= nil) and (enableException) then
        add_cxflags("/EHsc", {
            force = true
        })
    end
end

function BuildProject(config)
    target(config.projectName)

    set_languages("clatest")
    set_languages("cxx20")
    projectType = config.projectType
    if projectType ~= nil then
        set_kind(projectType)
    end
    Execute(config.macros, add_defines)
    Execute(config.files, add_files)
    Execute(config.includePaths, add_includedirs)
    Execute(config.depends, add_deps)
    unityBuildBatch = config.unityBuildBatch
    if (unityBuildBatch ~= nil) and (unityBuildBatch > 0) then
        add_rules("c.unity_build", {
            batchsize = unityBuildBatch
        })
        add_rules("c++.unity_build", {
            batchsize = unityBuildBatch
        })
    end

    after_build(function(target)
        build_path = ""
        src_path = config.projectFolder .. "/" .. "Shaders/"
        if is_mode("release") then
            build_path = "$(buildir)/windows/x64/release/"
        else
            build_path = "$(buildir)/windows/x64/debug/"
        end
        os.cp(src_path .. "*", build_path .. "Shaders/")
    end)

    if is_mode("release") then
        set_optimize("aggressive")
        if is_plat("windows") then
            set_runtimes("MD")
            add_cxflags("/Zi", "/W0", "/MP", "/Ob2", "/Oi", "/Ot", "/Oy", "/GT", "/GF", "/GS-", "/Gy", "/FS", "/arch:AVX2",
                "/Gr", "/sdl-", "/GL", "/Zc:preprocessor", "/Gr", "/TP", {
                    force = true
                })
            SetException(config.releaseException)
        end
        Execute(config.releaseMacros, add_defines)
    else
        set_optimize("none")
        if is_plat("windows") then
            set_runtimes("MDd")
            add_cxflags("/Zi", "/W0", "/MP", "/Ob0", "/Oy-", "/GF", "/GS", "/arch:AVX2", "/Gr", "/TP", "/Gr", "/FS",
                "/Zc:preprocessor", {
                    force = true
                })
            SetException(config.debugException)
        end
        Execute(config.debugMacros, add_defines)
    end
end


add_syslinks("User32", "kernel32", "Gdi32", "Shell32", "DXGI", "D3D12", "D3DCompiler")

includes("01_DirectX12_Initialization")
includes("02_Drawing_in_Direct3D-Box")
includes("03_Drawing_in_Direct3D_Part_II-Shapes")
includes("04_Drawing_in_Direct3D_Part_II-LandAndWaves")
includes("05_Lighting-LitWaves")
