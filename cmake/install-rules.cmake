install(
    TARGETS ndi-rist-encoder ndi-rist-server
    RUNTIME COMPONENT ndi-rist_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
