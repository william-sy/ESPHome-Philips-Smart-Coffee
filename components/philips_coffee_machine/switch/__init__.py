import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import CONTROLLER_ID, PhilipsCoffeeMachine, philips_coffee_machine_ns

DEPENDENCIES = ["philips_coffee_machine"]

CLEAN_DURING_START = "clean"

power_switch_namespace = philips_coffee_machine_ns.namespace("philips_power_switch")
PowerSwitch = power_switch_namespace.class_("Power", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(
    PowerSwitch,
).extend(
    {
        cv.Required(CONTROLLER_ID): cv.use_id(PhilipsCoffeeMachine),
        cv.Optional(CLEAN_DURING_START, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    controller = await cg.get_variable(config[CONTROLLER_ID])
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    cg.add(var.set_cleaning(config[CLEAN_DURING_START]))
    cg.add(controller.register_power_switch(var))
