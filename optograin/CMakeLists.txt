target_sources(${FW_ELF_FILE} PRIVATE
   src/main.c
)

target_include_directories(${FW_ELF_FILE} PUBLIC
    inc
)