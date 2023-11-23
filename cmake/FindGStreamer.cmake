set(GSTREAMER_FOUND FALSE)
 
if (WIN32)
 
  set(GSTREAMER_DIR $ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64})
 
  if (EXISTS ${GSTREAMER_DIR}/include/gstreamer-1.0/gst/gst.h)
    set(GSTREAMER_FOUND TRUE)
    set(GSTREAMER_VERSION_STRING 1.0 CACHE STRING "")
    set(GSTREAMER_INCLUDE_DIRS
      ${GSTREAMER_DIR}/include/gstreamer-1.0/
      ${GSTREAMER_DIR}/include/glib-2.0/
      ${GSTREAMER_DIR}/lib/glib-2.0/include/
      ${GSTREAMER_DIR}/lib/gstreamer-1.0/include/
      ${GSTREAMER_DIR}/include/libxml2/
      CACHE STRING ""
    )
 
    file(GLOB gst_lib_files ${GSTREAMER_DIR}/lib/*.lib)
    set(GSTREAMER_LIBRARIES ${gst_lib_files} CACHE STRING "")
 
    mark_as_advanced(GSTREAMER_INCLUDE_DIRS GSTREAMER_LIBRARIES)
 
  endif()
 
endif()