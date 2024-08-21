## buff_log
This component is supposed to help esphome developers with debugging.
It should come in handy when you need to see logs from the very startup, too early for the device to output them via wifi.
Of course in such cases you can always debug via serial... But what if you can't?
`buff_log` will buffer the log lines for some time and allow you to specify when to dump them.

### How it works
At startup, `buff_log` connects to `esphome logger` and enters BUFFERING state in which all the log lines are gathered. When a certain condition (overflow and/or timeout) is met, the component goes into DUMPING state, in which it will pass the buffered data (line by line) to the user-provided lambda function. After that it enters IDLE state.

There are 2 possible triggers for going to DUMPING state:
- buffer overflow - when total amount of logged data reaches the configured maximum
- timeout - when the configured number of milliseconds (counted from buffering start) passes.

The memory condition is more important than the time condition, which means overflowing the buffer will trigger DUMPING even though the timeout is not reached.

The user can also specify additional lambda that will get called when the DUMPING state is starting and finished. This is helpful if you need to pause the dumping until something other event.


At any time the user can pause/resume the operation by calling pause() or resume() method.
There's also an option to restart the buffering process at any time (losing the already buffered data!).

### Usage:
1. Add this external component to your yaml
``` yaml
external_components:
  - source: github://szupi-ipuzs/esphome-external-components
    components: buff_log
```

2. Configure the component, 
The configuration items:
* `size_limit` [required]: maximum number of __bytes__ that can be buffered. This lets you control the memory imprint. Reaching the limit will trigger going into the DUMPING state.
* `buffering_timeout` [optional]: maximum time (in milliseconds) to wait before going into the DUMPING state. If not specified, 0ms is set.
* `dump_lines_per_loop` [required]: maximum number of buffered log lines to be passed to `on_dump_line` in one component loop() call.
* `on_dump_line` [required]: lambda that will get called during DUMPING state for each buffered log line. It receives the following parameters:  
 --> buffLogRef - reference to the buff_log component. Usefull if you need to call it without using id() (which causes circular dependency).  
 --> level - esphome debug level of the message  
 --> tag - tag of the message  
 --> payload - content of the message  
 --> timestamp - reception timestamp (in milliseconds)  
* `on_dump_state_changed` [optional]: lambda that will get called right before DUMPING is started and right after it's finished. Useful if you need to eg. pause the dumping. It receives the following parameters:  
 --> buffLogRef - reference to the buff_log component. Usefull if you need to call it without using id() (which causes circular dependency).  
 --> state - one of buff_log::DumpingState (STARTING or FINISHED) that explain why the lambda is called.  
 --> trigger - one of buff_log::DumpingTrigger that explains the reason for dumping (timeout/memory/forced).  
 --> size - size (in bytes) of the buffered data  
 --> lines - total number of lines of the buffered data  
 * `tags` [optional]: list of logger tags (strings) that should be buffered. If not specified logs with all tags will be buffered.  

Example yaml:
```yaml
buff_log:
  id: buffering_logger
  size_limit: 32768
  dump_lines_per_loop: 10
  buffering_timeout: 10000ms
  on_dump_state_changed: |-
    if (state == buff_log::DumpingState::STARTING)
    {
       ESP_LOGI("buff_log", "Starting dump (%zu, %zu), trigger reason: %d", size, lines, trigger);
       if (trigger == buff_log::DumpingTrigger::OVERFLOW)
       {
          if (!esphome::wifi::global_wifi_component->is_connected())
          {
              buffLogRef.pause();
          }
       }
    }
    else
    {
       ESP_LOGI("buff_log", "Dumping finished (%zu, %zu), trigger reason: %d", size, lines, trigger);
    }
  on_dump_line: |-
    ESP_LOGI("buff_log", "[%u][%d][%s]: %s", timestamp, level, tag.c_str(), payload.c_str());

wifi:
  on_connect:
    - lambda: !lambda |-
        id(buffering_logger).resume();

button:
  - platform: template
    name: "Restart buffering"
    on_press: 
      then:
        - lambda: !lambda |-
            id(buffering_logger).restart();
```

