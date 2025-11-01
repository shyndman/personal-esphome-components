import esphome.codegen as cg
from esphome.components import microphone
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MICROPHONE, CONF_PORT

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["microphone"]

udp_audio_streamer_ns = cg.esphome_ns.namespace("udp_audio_streamer")
UDPAudioStreamer = udp_audio_streamer_ns.class_("UDPAudioStreamer", cg.Component)

CONF_HOST = "host"
CONF_CHUNK_DURATION = "chunk_duration"
CONF_BUFFER_DURATION = "buffer_duration"
CONF_PASSIVE = "passive"


def _validate_buffer(config):
    chunk_ms = config[CONF_CHUNK_DURATION].total_milliseconds
    buffer_ms = config[CONF_BUFFER_DURATION].total_milliseconds
    if buffer_ms < chunk_ms:
        raise cv.Invalid(
            f"{CONF_BUFFER_DURATION} must be greater than or equal to {CONF_CHUNK_DURATION}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UDPAudioStreamer),
            cv.Required(CONF_HOST): cv.string_strict,
            cv.Required(CONF_PORT): cv.port,
            cv.Optional(CONF_PASSIVE, default=False): cv.boolean,
            cv.Optional(
                CONF_CHUNK_DURATION, default="32ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_BUFFER_DURATION, default="512ms"
            ): cv.positive_time_period_milliseconds,
            cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=32,
                min_channels=1,
                max_channels=2,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_buffer,
)


async def to_code(config):
    chunk_ms = config[CONF_CHUNK_DURATION].total_milliseconds
    buffer_ms = config[CONF_BUFFER_DURATION].total_milliseconds

    if buffer_ms < chunk_ms:
        buffer_ms = chunk_ms

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic_source = await microphone.microphone_source_to_code(
        config[CONF_MICROPHONE], passive=config[CONF_PASSIVE]
    )
    cg.add(var.set_microphone_source(mic_source))
    cg.add(var.set_endpoint(config[CONF_HOST], config[CONF_PORT]))
    cg.add(var.set_chunk_duration(chunk_ms))
    cg.add(var.set_buffer_duration(buffer_ms))
    cg.add(var.set_passive(config[CONF_PASSIVE]))
