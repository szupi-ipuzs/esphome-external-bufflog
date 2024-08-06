import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core
from esphome.const import (
    CONF_ID
)

DEPENDENCIES = ["logger"]

CODEOWNERS = ["@szupi-ipuzs"]
buff_log_ns = cg.esphome_ns.namespace("buff_log")

CONF_BUFFERING_TIMEOUT = "buffering_timeout"
CONF_SIZE_LIMIT = "size_limit"
CONF_DUMP_LINES_PER_LOOP = "dump_lines_per_loop"
CONF_ON_DUMP_LINE = "on_dump_line"
CONF_ON_STATE_CHANGED = "on_dump_state_changed"
CONF_TAGS = "tags"

BufferingLogger = buff_log_ns.class_("BufferingLogger", cg.Component)

CONFIG_SCHEMA = cv.All(
   cv.Schema({
      cv.GenerateID(CONF_ID): cv.declare_id(BufferingLogger),
      cv.Optional(CONF_BUFFERING_TIMEOUT, default="0ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=core.TimePeriod(milliseconds=0), max=core.TimePeriod(milliseconds=10000)),
            ),
      cv.Required(CONF_SIZE_LIMIT): cv.int_range(min=1024, max=1048576),
      cv.Optional(CONF_DUMP_LINES_PER_LOOP, default=20): cv.int_range(min=1, max=100),
      cv.Optional(CONF_TAGS): cv.ensure_list(cv.string_strict),
      cv.Required(CONF_ON_DUMP_LINE): cv.lambda_,
      cv.Optional(CONF_ON_STATE_CHANGED): cv.lambda_
   })
)

async def to_code(config):
   dump_line_lambda_ = await cg.process_lambda(
         config[CONF_ON_DUMP_LINE],
         [(buff_log_ns.BufferingLogger.operator("ref"), "buffLogRef"),
          (int, "level"),
          (cg.std_string, "tag"),
          (cg.std_string, "payload"),
          (cg.uint32, "timestamp")],
         return_type=cg.void,
   )
   if CONF_ON_STATE_CHANGED in config:
      state_changed_lambda_ = await cg.process_lambda(config[CONF_ON_STATE_CHANGED], [(buff_log_ns.BufferingLogger.operator("ref"), "buffLogRef"), (buff_log_ns.DumpingState, "state"), (buff_log_ns.DumpingTrigger, "trigger"), (int, "size"), (int, "lines")], return_type=cg.void)
      var = cg.new_Pvariable(config[CONF_ID],
                             config[CONF_BUFFERING_TIMEOUT],
                             config[CONF_SIZE_LIMIT],
                             config[CONF_DUMP_LINES_PER_LOOP],
                             dump_line_lambda_,
                             state_changed_lambda_)
   else:
      var = cg.new_Pvariable(config[CONF_ID],
                             config[CONF_BUFFERING_TIMEOUT],
                             config[CONF_SIZE_LIMIT],
                             config[CONF_DUMP_LINES_PER_LOOP],
                             dump_line_lambda_)

   if CONF_TAGS in config:
      for tag in config[CONF_TAGS]:
         cg.add(var.addTag(tag))
   await cg.register_component(var, config)
