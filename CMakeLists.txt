target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_LONG_PRESS app PRIVATE
    src/behavior_long_press.c
)

target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_ACCEL app PRIVATE
    src/input_processor_accel.c
)
