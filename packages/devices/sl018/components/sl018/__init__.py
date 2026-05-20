import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["text_sensor"]

CONF_TAG_PIN = "tag_pin"
CONF_TAG_UID = "tag_uid"

sl018_ns = cg.esphome_ns.namespace("sl018")
SL018Component = sl018_ns.class_(
    "SL018Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SL018Component),
            cv.Optional(CONF_TAG_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_TAG_UID): text_sensor.text_sensor_schema(
                icon="mdi:nfc"
            ),
            cv.Optional(CONF_UPDATE_INTERVAL, default="100ms"): cv.update_interval,
        }
    )
    .extend(cv.polling_component_schema("100ms"))
    .extend(i2c.i2c_device_schema(0x50))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if tag_pin_config := config.get(CONF_TAG_PIN):
        pin = await cg.gpio_pin_expression(tag_pin_config)
        cg.add(var.set_tag_pin(pin))

    if tag_uid_config := config.get(CONF_TAG_UID):
        sens = await text_sensor.new_text_sensor(tag_uid_config)
        cg.add(var.set_tag_sensor(sens))
