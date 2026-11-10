set(contact_patch "${CMAKE_CURRENT_LIST_DIR}/../patches/tps43-three-finger-contact.patch")
set(azoteq_dir "${ZEPHYR_ZMK_DRIVER_AZOTEQ_MODULE_DIR}")
if(NOT EXISTS "${azoteq_dir}/drivers/input/tps43.c")
  message(FATAL_ERROR "Cannot find TPS43 driver for three-finger contact patch")
endif()

execute_process(
  COMMAND git apply --reverse --check "${contact_patch}"
  WORKING_DIRECTORY "${azoteq_dir}"
  RESULT_VARIABLE already_applied
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT already_applied EQUAL 0)
  execute_process(
    COMMAND git apply "${contact_patch}"
    WORKING_DIRECTORY "${azoteq_dir}"
    RESULT_VARIABLE patch_result
    ERROR_VARIABLE patch_error
  )
  if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "TPS43 contact patch failed: ${patch_error}")
  endif()
endif()
