install(
    TARGETS ndi-rist_exe
    RUNTIME COMPONENT ndi-rist_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
