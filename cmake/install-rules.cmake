install(
    TARGETS ndi-rist-encoder
)

set(CPACK_PACKAGE_NAME "NDI Rist Encoder")
set(CPACK_PACKAGE_VENDOR "Pat Carter")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "NDI to RTMP streamer using AV1 over MPEGTS and RIST")

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
