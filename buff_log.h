#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/logger/logger.h"
#include <string>
#include <cstdint>
#include <vector>
#include <functional>

namespace esphome{

namespace buff_log{

static const char* TAG = "bufflog";

struct LogItem
{
   int level;
   std::string tag;
   std::string payload;
   uint32_t timestamp;
};

enum DumpingState
{
   STARTING,
   FINISHED
};

enum DumpingTrigger
{
   NONE,
   OVERFLOW,
   TIMEOUT,
   FORCED
};

struct Config
{
   std::uint32_t bufferingTimeout;
   std::uint32_t sizeLimit;
   std::uint32_t dumpLinesPerLoop;
};

class BufferingLogger : public Component
{
public:

   enum State
   {
      IDLE,
      BUFFERING,
      DUMPING
   };

   using DumpProcessorFunction = std::function<void(BufferingLogger&, int, const std::string&, const std::string&, std::uint32_t)>;
   using DumpingStateChangedHandler = std::function<void(BufferingLogger&, DumpingState, DumpingTrigger, std::uint32_t, std::uint32_t)>;

   BufferingLogger(std::uint32_t bufferingTimeout,
                   std::uint32_t sizeLimit,
                   std::uint32_t dumpLinesPerLoop,
                   DumpProcessorFunction dumpProcessor,
                   DumpingStateChangedHandler stateChangedHandler = [](BufferingLogger&, DumpingState, DumpingTrigger, std::uint32_t, std::uint32_t){}):
   m_config{bufferingTimeout, sizeLimit, dumpLinesPerLoop},
   m_dumpProcessor(std::move(dumpProcessor)),
   m_stateChangedHandler(std::move(stateChangedHandler))
   {}

   float get_setup_priority() const override
   {
      if (logger::global_logger != nullptr)
      {
         return logger::global_logger->get_setup_priority() - 100.0;
      }
      else
      {
         return setup_priority::BUS;
      }
   }

   void setup() override
   {
      ESP_LOGD(TAG, "setup");
      if (logger::global_logger != nullptr)
      {
         logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message)
         {
            if ((m_state != State::BUFFERING) || (m_paused))
            {
               return;
            }

            processLogLine(level, tag, message, esphome::millis());
         });
      }

      m_state = State::BUFFERING;
      m_startTimestamp = esphome::millis();
   }

   void loop() override
   {
      if (m_state == State::BUFFERING)
      {
         loopWhenBuffering();
      }

      if (m_state == State::DUMPING)
      {
         loopWhenDumping();
      }
   }

   void stop()
   {
      m_state = State::IDLE;
      clearQueue();
      ESP_LOGD(TAG, "Stopped");
   }

   void restart()
   {
      ESP_LOGD(TAG, "Restarting");
      clearQueue();
      m_paused = false;
      m_state = State::BUFFERING;
      m_startTimestamp = esphome::millis();
      m_trigger = DumpingTrigger::NONE;
   }

   bool isBuffering() const
   {
      return ((State::BUFFERING == m_state) && (!m_paused));
   }

   std::uint32_t getFillLevel() const
   {
      return m_currentSize;
   }

   void dump()
   {
      m_state = State::DUMPING;
      m_dumpedItemIndex = 0;
      m_paused = false;
      m_trigger = DumpingTrigger::FORCED;
      ESP_LOGD(TAG, "Forcing dump");
   }

   void pause()
   {
      m_paused = true;
      ESP_LOGD(TAG, "Paused");
   }

   void resume()
   {
      m_paused = false;
      ESP_LOGD(TAG, "Resuming");
   }

   bool isPaused() const
   {
      return m_paused;
   }

   DumpingTrigger getDumpingTrigger() const
   {
      return m_trigger;
   }

private:

   void clearQueue()
   {
      m_items.clear();
      m_currentSize = 0;
   }

   void processLogLine(int level, const char* tag, const char* message, uint32_t timestamp)
   {
      uint32_t combinedLength = strlen(tag) + strlen(message);
      m_items.emplace_back(LogItem{level, std::string(tag), std::string(message), timestamp});
      m_currentSize += combinedLength;
      if (m_currentSize >= m_config.sizeLimit)
      {
         m_state = State::DUMPING;
         m_dumpedItemIndex = 0;
         m_trigger = DumpingTrigger::OVERFLOW;
         ESP_LOGD(TAG, "Starting dump due to overflow%s");
         m_stateChangedHandler(*this, DumpingState::STARTING, m_trigger, m_currentSize, m_items.size());
      }
   }

   void loopWhenBuffering()
   {
      if ((m_config.bufferingTimeout != 0) && ((millis() - m_startTimestamp) > m_config.bufferingTimeout))
      {
         m_paused = false;
         m_state = State::DUMPING;
         m_dumpedItemIndex = 0;
         m_trigger = DumpingTrigger::TIMEOUT;
         ESP_LOGD(TAG, "Starting dump due to timeout");
         m_stateChangedHandler(*this, DumpingState::STARTING, m_trigger, m_currentSize, m_items.size());
      }
   }

   void loopWhenDumping()
   {
      uint32_t itemsDumped = 0;
      while ((m_state == State::DUMPING) && (!m_paused) && (itemsDumped < m_config.dumpLinesPerLoop))
      {
         if (m_dumpedItemIndex == m_items.size())
         {
            auto dumpedSize = m_currentSize; // m_currentSize will be reset when stopped
            stop();
            m_stateChangedHandler(*this, DumpingState::FINISHED, m_trigger, dumpedSize, m_dumpedItemIndex);
            break;
         }

         ESP_LOGD(TAG, "dumping %zu/%zu", m_dumpedItemIndex + 1, m_items.size());
         auto & item = m_items[m_dumpedItemIndex];
         m_dumpProcessor(*this, item.level, item.tag, item.payload, item.timestamp);
         ++m_dumpedItemIndex;
         ++itemsDumped;
      }
   }

   const Config m_config;
   DumpProcessorFunction m_dumpProcessor;
   DumpingStateChangedHandler m_stateChangedHandler;
   State m_state{State::IDLE};
   std::uint32_t m_startTimestamp{0u};
   bool m_paused{false};
   DumpingTrigger m_trigger{DumpingTrigger::NONE};

   std::vector<LogItem> m_items;
   std::uint32_t m_currentSize{0u};
   std::size_t m_dumpedItemIndex;
};

}}
