import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ADDRESS, CONF_ID
from . import DS2482Component, ds2482_ns

DEPENDENCIES = ['ds2482']
CONF_DS2482_ID = 'ds2482_id'

DS2482Sensor = ds2482_ns.class_('DS2482Sensor', sensor.Sensor)

CONFIG_SCHEMA = sensor.sensor_schema(
    DS2482Sensor,
    accuracy_decimals=2
).extend({
    cv.GenerateID(CONF_DS2482_ID): cv.use_id(DS2482Component),
    cv.Optional(CONF_ADDRESS): cv.hex_uint64_t,
})

async def to_code(config):
    hub = await cg.get_variable(config[CONF_DS2482_ID])
    var = await sensor.new_sensor(config)
    
    # This is the specific line that pushes your YAML address to C++
    addr = config.get(CONF_ADDRESS, 0)
    cg.add(hub.register_sensor(var, addr, 0))