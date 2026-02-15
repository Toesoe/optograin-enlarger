target_sources(${FW_ELF_FILE} PRIVATE
   src/main.c
   src/motion.c
   src/interface.c
)

target_include_directories(${FW_ELF_FILE} PUBLIC
    inc
)

nanopb_generate_cpp(TARGET proto optograin.proto)
target_link_libraries(${FW_ELF_FILE} PRIVATE proto)