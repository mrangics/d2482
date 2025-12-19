import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID
from . import DS2482Component, ds2482_ns

DEPENDENCIES = ['ds2482']
CONF_DS2482_ID = 'ds2482_id'

DS2482Output = ds2482_ns.class_('DS2482Output', output.FloatOutput)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.GenerateID(CONF_ID): cv.declare_id(DS2482Output),
    cv.GenerateID(CONF_DS2482_ID): cv.use_id(DS2482Component),
})

async def to_code(config):
    hub = await cg.get_variable(config[CONF_DS2482_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    cg.add(var.set_parent(hub))