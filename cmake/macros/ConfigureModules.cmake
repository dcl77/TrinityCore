# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Returns the base path to the module directory in the source directory
function(GetModulesBasePath variable)
  set(${variable} "${CMAKE_SOURCE_DIR}/modules" PARENT_SCOPE)
endfunction()

# Creates a list of all modules and stores it in the given variable.
function(GetModuleList variable)
  GetModulesBasePath(BASE_PATH)
  file(GLOB LOCALE_MODULE_LIST RELATIVE
    ${BASE_PATH}
    ${BASE_PATH}/*)

  set(${variable})
  foreach(MODULE ${LOCALE_MODULE_LIST})
    if(IS_DIRECTORY "${BASE_PATH}/${MODULE}" AND EXISTS "${BASE_PATH}/${MODULE}/CMakeLists.txt")
      list(APPEND ${variable} ${MODULE})
    endif()
  endforeach()
  set(${variable} ${${variable}} PARENT_SCOPE)
endfunction()

# Global variables to store module data
set(MOD_SCRIPTS_LIST "" CACHE INTERNAL "List of module script registration functions")
set(MOD_SOURCES_LIST "" CACHE INTERNAL "List of module source files")

# Macro for modules to register their scripts
macro(AddModuleScript LOAD_FUNCTION)
  list(APPEND MOD_SCRIPTS_LIST ${LOAD_FUNCTION})
  set(MOD_SCRIPTS_LIST ${MOD_SCRIPTS_LIST} CACHE INTERNAL "List of module script registration functions")
endmacro()

# Macro for modules to register their sources
macro(AddModuleSource)
  list(APPEND MOD_SOURCES_LIST ${ARGN})
  set(MOD_SOURCES_LIST ${MOD_SOURCES_LIST} CACHE INTERNAL "List of module source files")
endmacro()
