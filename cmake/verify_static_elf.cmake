if(NOT DEFINED NM_EXECUTABLE OR NM_EXECUTABLE STREQUAL "")
  message(FATAL_ERROR "NM_EXECUTABLE is required")
endif()
if(NOT DEFINED ELF_FILE OR ELF_FILE STREQUAL "")
  message(FATAL_ERROR "ELF_FILE is required")
endif()

execute_process(
  COMMAND "${NM_EXECUTABLE}" -u "${ELF_FILE}"
  RESULT_VARIABLE nm_u_result
  OUTPUT_VARIABLE nm_u_output
  ERROR_VARIABLE nm_u_error
)
if(NOT nm_u_result EQUAL 0)
  message(FATAL_ERROR "nm -u failed: ${nm_u_error}")
endif()

execute_process(
  COMMAND "${NM_EXECUTABLE}" --defined-only "${ELF_FILE}"
  RESULT_VARIABLE nm_d_result
  OUTPUT_VARIABLE nm_d_output
  ERROR_VARIABLE nm_d_error
)
if(NOT nm_d_result EQUAL 0)
  message(FATAL_ERROR "nm --defined-only failed: ${nm_d_error}")
endif()

set(nm_u_text "\n${nm_u_output}\n")
set(nm_d_text "\n${nm_d_output}\n")

foreach(symbol IN ITEMS malloc calloc realloc free ucHeap pvPortMalloc vPortFree)
  if(nm_u_text MATCHES "\n[ \t]*U ${symbol}\n")
    message(FATAL_ERROR "forbidden undefined dynamic memory symbol: ${symbol}")
  endif()
  if(nm_d_text MATCHES "\n[0-9A-Fa-f]+ [A-Za-z] ${symbol}\n")
    message(FATAL_ERROR "forbidden defined dynamic memory symbol: ${symbol}")
  endif()
endforeach()

if(REQUIRE_STACK_OVERFLOW_HOOK)
  if(NOT nm_d_text MATCHES "\n[0-9A-Fa-f]+ T vApplicationStackOverflowHook\n")
    message(FATAL_ERROR "vApplicationStackOverflowHook must be linked as a strong text symbol")
  endif()
endif()
