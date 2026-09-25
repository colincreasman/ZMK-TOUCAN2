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
