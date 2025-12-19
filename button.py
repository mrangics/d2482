import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_ID, 
    CONF_NAME, 
    CONF_ICON, 
    CONF_ENTITY_CATEGORY, 
    CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT, # <--- Missing key 1
    CONF_INTERNAL             # <--- Missing key 2
)
from . import DS2482Component, ds2482_ns

DEPENDENCIES = ['ds2482']
CONF_DS2482_ID = 'ds2482_id'

DS2482ScanButton = ds2482_ns.class_('DS2482ScanButton', button.Button)

# FIXED: Added all standard Entity Base Schema keys manually
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(DS2482ScanButton),
    cv.GenerateID(CONF_DS2482_ID): cv.use_id(DS2482Component),
    
    # Standard Entity Options required by setup_entity()
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_ICON): cv.icon,
    cv.Optional(CONF_ENTITY_CATEGORY): cv.entity_category,
    cv.Optional(CONF_DEVICE_CLASS): cv.string,
    cv.Optional(CONF_DISABLED_BY_DEFAULT, default=False): cv.boolean,
    cv.Optional(CONF_INTERNAL, default=False): cv.boolean,
    
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_DS2482_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    
    if CONF_NAME in config:
        cg.add(var.set_name(config[CONF_NAME]))
        
    # Now config contains 'disabled_by_default', so this won't crash
    await button.register_button(var, config)
    
    cg.add(var.set_parent(hub))