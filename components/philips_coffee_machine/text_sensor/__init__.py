import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import CONTROLLER_ID, PhilipsCoffeeMachine, philips_coffee_machine_ns

STATUS_SENSOR_ID = "status_sensor_id"

philips_status_sensor_ns = philips_coffee_machine_ns.namespace("philips_status_sensor")
StatusSensor = philips_status_sensor_ns.class_(
    "StatusSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    StatusSensor,
).extend(
    {
        cv.Required(CONTROLLER_ID): cv.use_id(PhilipsCoffeeMachine),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONTROLLER_ID])
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    cg.add(parent.add_status_sensor(var))
