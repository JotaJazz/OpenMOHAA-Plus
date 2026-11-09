set(PROJECT_NAME openmohaa)
set(PROJECT_VERSION 0.83.0)
# OpenMOHAA Plus revision on top of the upstream PROJECT_VERSION (version shows as X.Y.Z-plus.N,
# release tags are vX.Y.Z-plus.N). Reset to 1 when an upstream merge bumps PROJECT_VERSION.
set(PRODUCT_VERSION_PLUS 1)

set(SERVER_NAME omohaaded)
set(CLIENT_NAME openmohaa)

set(BASEGAME)

set(CGAME_MODULE cgame)
set(GAME_MODULE game)
set(UI_MODULE ui)

set(WINDOWS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/windows/openmohaa.ico)

set(MACOS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/macos/openmohaa.icns)
set(MACOS_BUNDLE_ID org.openmohaa.${CLIENT_NAME})

set(COPYRIGHT "OpenMoHAA Copyright © 2025 OpenMoHAA Team, Inc. All rights reserved.")

set(CONTACT_EMAIL "")
set(PROTOCOL_HANDLER_SCHEME openmohaa)
