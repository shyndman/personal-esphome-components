import esphome.codegen as cg
from esphome import automation
from esphome.components import microphone
from esphome.automation import maybe_simple_id
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MICROPHONE,
)

mic_recorder_ns = cg.esphome_ns.namespace("microphone_recorder")
MicrophoneRecorder = mic_recorder_ns.class_("MicrophoneRecorder", cg.Component)
StartRecordingAction = mic_recorder_ns.class_(
    "StartRecordingAction", automation.Action, cg.Parented.template(MicrophoneRecorder)
)
StopRecordingAction = mic_recorder_ns.class_(
    "StopRecordingAction", automation.Action, cg.Parented.template(MicrophoneRecorder)
)

CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_D0_PIN = "d0_pin"
CONF_D1_PIN = "d1_pin"
CONF_D2_PIN = "d2_pin"
CONF_D3_PIN = "d3_pin"
CONF_MOUNT_POINT = "mount_point"
CONF_FILENAME_PREFIX = "filename_prefix"
CONF_MAX_DURATION = "max_duration"
CONF_FORMAT_ON_FAIL = "format_if_mount_failed"

microphone_recorder_schema = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MicrophoneRecorder),
        cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
            min_bits_per_sample=16,
            max_bits_per_sample=16,
            min_channels=1,
            max_channels=2,
        ),
        cv.Required(CONF_CLK_PIN): cv.int_,
        cv.Required(CONF_CMD_PIN): cv.int_,
        cv.Required(CONF_D0_PIN): cv.int_,
        cv.Optional(CONF_D1_PIN, default=-1): cv.int_,
        cv.Optional(CONF_D2_PIN, default=-1): cv.int_,
        cv.Optional(CONF_D3_PIN, default=-1): cv.int_,
        cv.Optional(CONF_MOUNT_POINT, default="/sdcard"): cv.string,
        cv.Optional(CONF_FILENAME_PREFIX, default="rec"): cv.string,
        cv.Optional(CONF_MAX_DURATION, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_FORMAT_ON_FAIL, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = microphone_recorder_schema

MICROPHONE_RECORDER_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(MicrophoneRecorder)})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
    cg.add(var.set_microphone_source(mic_source))

    cg.add(var.set_sd_pins(
        config[CONF_CLK_PIN],
        config[CONF_CMD_PIN],
        config[CONF_D0_PIN],
        config[CONF_D1_PIN],
        config[CONF_D2_PIN],
        config[CONF_D3_PIN],
    ))
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))
    cg.add(var.set_filename_prefix(config[CONF_FILENAME_PREFIX]))
    cg.add(var.set_max_duration_ms(config[CONF_MAX_DURATION].total_milliseconds))
    cg.add(var.set_format_if_mount_failed(config[CONF_FORMAT_ON_FAIL]))


@automation.register_action(
    "microphone_recorder.start", StartRecordingAction, MICROPHONE_RECORDER_ACTION_SCHEMA
)
async def microphone_recorder_start_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "microphone_recorder.stop", StopRecordingAction, MICROPHONE_RECORDER_ACTION_SCHEMA
)
async def microphone_recorder_stop_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
