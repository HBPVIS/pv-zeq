PROJECT("pv_zeq")

#--------------------------------------------------
# cmake old/new compatibility
#--------------------------------------------------
cmake_minimum_required(VERSION 2.8)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 OLD)
  cmake_policy(SET CMP0017 NEW)
  cmake_policy(SET CMP0023 OLD)
endif(COMMAND cmake_policy)

#-----------------------------------------------------------------------------
# Basic naming/config stuff here
#-----------------------------------------------------------------------------
set (pv_zeq_VERS_MAJOR   0)
set (pv_zeq_VERS_MINOR   1)
set (pv_zeq_VERS_RELEASE 0)

set (pv_zeq_PACKAGE                "pv_zeq")
set (pv_zeq_PACKAGE_NAME           "pv_zeq")
set (pv_zeq_PACKAGE_VERSION        "${pv_zeq_VERS_MAJOR}.${pv_zeq_VERS_MINOR}.${pv_zeq_VERS_RELEASE}")
set (pv_zeq_PACKAGE_VERSION_MAJOR  "${pv_zeq_VERS_MAJOR}.${pv_zeq_VERS_MINOR}")
set (pv_zeq_PACKAGE_VERSION_MINOR  "${pv_zeq_VERS_RELEASE}")
set (pv_zeq_PACKAGE_VERSION_STRING "${pv_zeq_PACKAGE_VERSION}")
set (pv_zeq_PACKAGE_SOVERSION      "${pv_zeq_SOVERS_MAJOR}.${pv_zeq_SOVERS_RELEASE}.${pv_zeq_SOVERS_MINOR}")
set (pv_zeq_PACKAGE_STRING         "${pv_zeq_PACKAGE_NAME} ${pv_zeq_PACKAGE_VERSION_STRING}")
set (pv_zeq_PACKAGE_TARNAME        "${pv_zeq_PACKAGE}")
set (pv_zeq_PACKAGE_URL            "http://www.cscs.ch")
set (pv_zeq_PACKAGE_BUGREPORT      "help@cscs.ch")

#--------------------------------------------------
# Location of cmake files
#--------------------------------------------------
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#--------------------------------------------------
# Find and Use ParaView
#--------------------------------------------------
IF (ParaView_SOURCE_DIR)
  IF(PARAVIEW_BUILD_QT_GUI)
    INCLUDE(${QT_USE_FILE})
  ENDIF(PARAVIEW_BUILD_QT_GUI)
  INCLUDE_DIRECTORIES(
    ${HDF5_INCLUDE_DIR}
    ${PARAVIEW_INCLUDE_DIRS}
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIR}
  )
ELSE (ParaView_SOURCE_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
ENDIF (ParaView_SOURCE_DIR)

#------------------------------------------------------------------------------
# Boost
#------------------------------------------------------------------------------
set(Boost_ADDITIONAL_VERSIONS
  ${Boost_ADDITIONAL_VERSIONS}
  "1.59.0" "1.59"
  "1.58.0" "1.58"
  "1.57.0" "1.57")

find_package(Boost
  1.55
  REQUIRED
  COMPONENTS
  regex date_time serialization
  program_options filesystem log log_setup system thread
)

include_directories(${Boost_INCLUDE_DIRS})

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
INCLUDE_DIRECTORIES(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
)

#--------------------------------------------------
# Lunchbox (required for servus etc)
#--------------------------------------------------
find_package(Lunchbox)
include_directories(${LUNCHBOX_INCLUDE_DIRS})

#--------------------------------------------------
# ZeroEQ
#--------------------------------------------------
find_package(zeq)
include_directories(${ZEQ_INCLUDE_DIRS})

#--------------------------------------------------
# Monsteer
#--------------------------------------------------
find_package(Monsteer)
include_directories(${MONSTEER_INCLUDE_DIRS})
message("dirs are ${MONSTEER_INCLUDE_DIRS}")

#--------------------------------------------------
# Set Includes
#--------------------------------------------------
INCLUDE_DIRECTORIES(
)

#------------------------------------------------------
# MPI support if ParaView compiled with MPI
#------------------------------------------------------
IF (PARAVIEW_USE_MPI)
#  FIND_PACKAGE(MPI)   
#  INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
#  ADD_DEFINITIONS(-DMPICH_SKIP_MPICXX)
ENDIF (PARAVIEW_USE_MPI)

#--------------------------------------------------
# Set Definitions 
#--------------------------------------------------
IF (WIN32)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

#--------------------------------------------------
# Setup GUI custom Qt panel sources and wrapping
#--------------------------------------------------
IF(PARAVIEW_BUILD_QT_GUI)
  if (PARAVIEW_QT_VERSION VERSION_GREATER "4")
    set (Qt5_FIND_COMPONENTS Widgets)
    include (ParaViewQt5)
    
    QT5_WRAP_CPP(
      ZEQ_MOC_SRCS 
        ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerDockWindow.h
    )
    
    QT5_WRAP_UI(
      ZEQ_UI_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerPanel.ui
    )
    
  else ()
    include (${QT_USE_FILE})
    
    #--------------------------------------------------
    # make sure all *.ui dialogs/etc are wrapped
    #--------------------------------------------------
    QT4_WRAP_UI(
      ZEQ_UI_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerPanel.ui
    )

    #--------------------------------------------------
    # make sure all *.h files for ui are wrapped by moc
    #--------------------------------------------------
    QT4_WRAP_CPP(
      ZEQ_MOC_SRCS 
        ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerDockWindow.h
    )
  endif()
  
  #--------------------------------------------------
  # invoke macro that adds our SPH manager panel
  #--------------------------------------------------
  ADD_PARAVIEW_OBJECT_PANEL(
    ZEQ_IFACE_O
    ZEQ_IFACE_SRCS_O
    CLASS_NAME 
      pqZeqManagerPanel
    XML_NAME 
     "ZeqManager"
    XML_GROUP 
     zeq_helpers
  )

  #--------------------------------------------------
  # invoke macro that adds our Zeq manager dock window
  #--------------------------------------------------
  ADD_PARAVIEW_DOCK_WINDOW(
    ZEQ_IFACE_D
    ZEQ_IFACE_SRCS_D
    CLASS_NAME 
      pqZeqManagerDockWindow 
    DOCK_AREA 
      Left Right Top Bottom
  )

ENDIF(PARAVIEW_BUILD_QT_GUI)

#--------------------------------------------------
# Define Wrapping hints
#--------------------------------------------------
#SET(VTK_WRAP_HINTS "${CMAKE_CURRENT_SOURCE_DIR}/hints.txt" )
 
#--------------------------------------------------
# Define Plugin 
#--------------------------------------------------
SET(PLUGIN_NAME pv_zeq)
ADD_PARAVIEW_PLUGIN(
  ${PLUGIN_NAME}
  "1.0" 
  REQUIRED_PLUGINS 
# pv_common

  SERVER_MANAGER_XML 
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkZeqManager.xml
  SERVER_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkZeqManager.cxx
  SERVER_SOURCES
  GUI_INTERFACES 
    ${ZEQ_IFACE_W}
    ${ZEQ_IFACE_O}
    ${ZEQ_IFACE_O2}
    ${ZEQ_IFACE_D}
  GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerPanel.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerDockWindow.cxx
    ${ZEQ_IFACE_SRCS_W}
    ${ZEQ_IFACE_SRCS_O}
    ${ZEQ_IFACE_SRCS_O2}
    ${ZEQ_IFACE_SRCS_D}
    ${ZEQ_MOC_SRCS}
    ${ZEQ_UI_SOURCES}
  GUI_RESOURCES
    # none for now  
  GUI_RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/pqZeqManagerPanel.ui
)

SET_TARGET_PROPERTIES(${PLUGIN_NAME} PROPERTIES COMPILE_FLAGS "${OpenMP_CXX_FLAGS}")

TARGET_LINK_LIBRARIES(${PLUGIN_NAME}
  PUBLIC
    vtksys
    Monsteer
    zeq
    zeqHBP
)

#--------------------------------------------------
# Testing
#--------------------------------------------------
IF (BUILD_TESTING)
  SET(PLUGIN_TEST_DIR ${PROJECT_BINARY_DIR}/Testing/Temporary)
#  MAKE_DIRECTORY(${PLUGIN_TEST_DIR})
#  ADD_SUBDIRECTORY(Testing)
ENDIF (BUILD_TESTING)  

#--------------------------------------------------------
# Create the configuration header.
#--------------------------------------------------------
CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/module_configure.h.in
               ${PROJECT_BINARY_DIR}/pv_zeq_configure.h
               @ONLY IMMEDIATE)

#-----------------------------------------------------------------------------
# Configure the project-config.cmake file for the build directory
#-----------------------------------------------------------------------------
set (${PROJECT_NAME}_INCLUDES_BUILD_TIME
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
)

include(CMakeInstallation)

return()

#--------------------------------------------------
# Install
#--------------------------------------------------
SET(INSTALL_PATH 
  "${CMAKE_INSTALL_PREFIX}/lib/paraview-${PARAVIEW_VERSION_MAJOR}.${PARAVIEW_VERSION_MINOR}"
)

#INSTALL(
#  FILES ${TOOL_CFG_FILES}
#  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
#)

INSTALL(
  TARGETS
    ${PLUGIN_NAME}
  DESTINATION 
    ${INSTALL_PATH}
)
