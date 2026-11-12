set(LAUNCHER_SOURCES "${SOURCE_DIR}/Launcher/launch_main.cpp")

if (WIN32)
    list(APPEND LAUNCHER_SOURCES
        ${SOURCE_DIR}/Launcher/launch_win32.cpp
        ${SOURCE_DIR}/sys/win_resource.rc
    )
else()
    list(APPEND LAUNCHER_SOURCES
        ${SOURCE_DIR}/Launcher/launch_linux.cpp
    )
endif()

function (create_launcher name type)
    if (BUILD_CLIENT)
        add_executable(openmohaa_launcher_${name} ${LAUNCHER_SOURCES})
        target_compile_definitions(openmohaa_launcher_${name} PRIVATE NO_RC_MANIFEST=1 TARGET_GAME=${type})
        target_compile_features(openmohaa_launcher_${name} PRIVATE cxx_std_17)
        set_target_properties(openmohaa_launcher_${name} PROPERTIES OUTPUT_NAME "launch_openmohaa_${name}${TARGET_BIN_SUFFIX}")
        set_target_properties(openmohaa_launcher_${name} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

        INSTALL(TARGETS openmohaa_launcher_${name} DESTINATION ${INSTALL_BINDIR_FULL})

        # Added in OpenMOHAA Plus
        #  The .desktop entries run the launchers by name: link them from <prefix>/bin.
        #  The launcher finds the client through /proc/self/exe, so the link works.
        if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
            set(launcher_file "$<TARGET_FILE_NAME:openmohaa_launcher_${name}>")
            cmake_path(RELATIVE_PATH INSTALL_BINDIR_FULL BASE_DIRECTORY bin OUTPUT_VARIABLE launcher_dir)
            install(CODE "
                file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\")
                file(CREATE_LINK \"${launcher_dir}/${launcher_file}\"
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/${launcher_file}\" SYMBOLIC)
            ")
        endif()
    endif()

    #add_executable(omohaaded_launcher_${name} ${LAUNCHER_SOURCES})
    #target_compile_definitions(omohaaded_launcher_${name} PRIVATE NO_RC_MANIFEST=1 TARGET_GAME=${type} DEDICATED=1)
    #target_compile_features(omohaaded_launcher_${name} PRIVATE cxx_std_17)
    #set_target_properties(omohaaded_launcher_${name} PROPERTIES OUTPUT_NAME "launch_omohaaded_${name}${TARGET_BIN_SUFFIX}")
    #set_target_properties(omohaaded_launcher_${name} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
    #
    #INSTALL(TARGETS omohaaded_launcher_${name} DESTINATION ${CMAKE_INSTALL_BINDIR}/${PROJECT_INSTALL_SUBDIR})
endfunction()

create_launcher(base 0)
create_launcher(spearhead 1)
create_launcher(breakthrough 2)
