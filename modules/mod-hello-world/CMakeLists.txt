# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AddModuleScript("mod-hello-world")

CollectSourceFiles(${CMAKE_CURRENT_SOURCE_DIR}/src MOD_HELLO_WORLD_SOURCES)
AddModuleSource(${MOD_HELLO_WORLD_SOURCES})
