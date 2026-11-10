target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dts)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

if(CONFIG_INPUT_TPS43)
  include(cmake/tps43-contact.cmake)
endif()

target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_LONG_PRESS app PRIVATE
    src/behavior_long_press.c
)

target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_ACCEL app PRIVATE
    src/input_processor_accel.c
)

target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_SWIPE_ARBITER app PRIVATE
    src/input_processor_swipe_arbiter.c
)

target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_SCROLL_SHORTCUT app PRIVATE
    src/input_processor_scroll_shortcut.c
)

target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_ROTATE app PRIVATE
    src/input_processor_rotate.c
)

target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_TAP_DRAG app PRIVATE
    src/input_processor_tap_drag.c
)

target_sources_ifdef(CONFIG_TOUCAN_OS_MODE app PRIVATE
    src/toucan_os_mode.c
)

target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_OS_SELECT app PRIVATE
    src/behavior_os_select.c
)

target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_OS_MODE app PRIVATE
    src/behavior_os_mode.c
)

target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_OS_STATUS app PRIVATE
    src/behavior_os_status.c
)

# Raises key events, which only the central half can send to a host.
if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_WIN_SWITCHER app PRIVATE
      src/behavior_win_switcher.c
  )
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_HYPER_GUARD app PRIVATE
      src/behavior_hyper_guard.c
  )
endif()
