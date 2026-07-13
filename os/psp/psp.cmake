include("${PSPDEV}/psp/share/CreatePBP.cmake")

set(EXECUTABLE_NAME fallout-ce)
set(PSP_APP_NAME "Fallout CE")
set(PSP_TITLEID "FOUT00001")

create_pbp_file(
    TARGET ${EXECUTABLE_NAME}
    TITLE ${PSP_APP_NAME}
    ICON_PATH ${CMAKE_SOURCE_DIR}/os/psp/icon.png
)
