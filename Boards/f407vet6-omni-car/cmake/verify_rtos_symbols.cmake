if(NOT DEFINED NM_EXECUTABLE)
  message(FATAL_ERROR "NM_EXECUTABLE is required")
endif()
if(NOT DEFINED ELF_FILE)
  message(FATAL_ERROR "ELF_FILE is required")
endif()

execute_process(
  COMMAND "${NM_EXECUTABLE}" --defined-only "${ELF_FILE}"
  RESULT_VARIABLE nm_result
  OUTPUT_VARIABLE nm_output
  ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
  message(FATAL_ERROR "nm failed: ${nm_error}")
endif()

set(nm_text "\n${nm_output}\n")
foreach(task IN ITEMS Control UART Monitor)
  set(symbol "RTOS_${task}_StartTask")
  if(NOT nm_text MATCHES "\n[0-9A-Fa-f]+ [Tt] ${symbol}\n")
    message(FATAL_ERROR
      "${symbol} must be linked as the strong RTOS task entry"
    )
  endif()
endforeach()

foreach(symbol IN ITEMS ucHeap pvPortMalloc vPortFree)
  if(nm_text MATCHES "\n[0-9A-Fa-f]+ [A-Za-z] ${symbol}\n")
    message(FATAL_ERROR "dynamic FreeRTOS symbol is forbidden: ${symbol}")
  endif()
endforeach()
