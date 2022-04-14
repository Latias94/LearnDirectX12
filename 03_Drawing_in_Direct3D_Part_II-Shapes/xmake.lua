BuildProject({
    projectName = "t03",
    projectFolder = "03_Drawing_in_Direct3D_Part_II-Shapes",
    projectType = "binary",
    macros = {"_XM_NO_INTRINSICS_=1", "NOMINMAX", "UNICODE", "m128_f32=vector4_f32", "m128_u32=vector4_u32"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"./src/**.cpp", "../Common/**.cpp", "../ThirdParty/**.cpp" },
    includePaths = {"./", "../Common/", "../ThirdParty/DirectXMath/Inc/"},
    debugException = true,
    releaseException = true
})
