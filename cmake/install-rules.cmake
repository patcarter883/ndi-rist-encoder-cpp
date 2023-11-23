install(
    TARGETS ndi-rist-encoder_exe ndi-rist-server_exe
    RUNTIME COMPONENT ndi-rist_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
