include_guard(GLOBAL)

function(USES_STD target)
    target_compile_features(${target} PUBLIC cxx_std_23)
endfunction()

# Compatibility hook for bundled third-party CMake files.  Their public
# include directories and flags are declared locally in each CMakeLists.txt.
function(cad_3rdparty_setup target)
endfunction()
