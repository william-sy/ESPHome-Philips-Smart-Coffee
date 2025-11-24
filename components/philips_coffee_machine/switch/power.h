#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"
#include "../commands.h"

#define MESSAGE_REPETITIONS 5
#define POWER_TRIP_RETRY_DELAY 100
#define MAX_POWER_TRIP_COUNT 5
#define POWER_ON_GRACE_PERIOD 10000  // 10 seconds for display to boot after power trip
#define DISPLAY_POWER_CUT_DURATION 2000  // 2 seconds power cut to ensure full shutdown

namespace esphome
{
    namespace philips_coffee_machine
    {
        namespace philips_power_switch
        {

            /**
             * @brief Power Switch wich reflects the power state of the coffee machine.
             * On/Off will change the hardware state of the machine using uart and the power tripping mechanism.
             *
             */
            class Power : public switch_::Switch, public Component
            {
            public:
                void setup() override;
                void loop() override;

                /**
                 * @brief Write a boolean state to this entity which should be propagated to hardware
                 *
                 * @param state new State the entity should write to hardware
                 */
                void write_state(bool state);
                void dump_config() override;

                /**
                 * @brief Sets the mainboard uart reference used by this power switch. The uart is used to fake power on and power off messages.
                 *
                 * @param uart uart reference
                 */
                void set_mainboard_uart(uart::UARTDevice *uart)
                {
                    mainboard_uart_ = uart;
                }

                /**
                 * @brief Sets the power pin reference which is used to trip the display power
                 *
                 * @param ping pin reference
                 */
                void set_power_pin(GPIOPin *pin)
                {
                    power_pin_ = pin;
                }

                /**
                 * @brief Sets the time period which is used for removing power to the display.
                 */
                void set_power_trip_delay(uint32_t time)
                {
                    power_trip_delay_ = time;
                }

                /**
                 * @brief Sets the display boot delay before sending power-on commands
                 */
                void set_display_boot_delay(uint32_t time)
                {
                    display_boot_delay_ = time;
                }

                /**
                 * @brief Sets whether the power pin logic should be inverted
                 */
                void set_invert_power_pin(bool invert)
                {
                    invert_power_pin_ = invert;
                }

                /**
                 * @brief Sets the cleaning status of this power switch.
                 * If true the machine will clean during startup
                 */
                void set_cleaning(bool cleaning)
                {
                    cleaning_ = cleaning;
                }

                /**
                 * @brief Check if power switch is currently injecting commands
                 * Used to block display messages during command injection
                 */
                bool is_injecting_commands() const
                {
                    return injecting_commands_;
                }

                /**
                 * @brief Sets the initial state reference on this power switch
                 *
                 * @param initial_state hub components initial state reference
                 */
                void set_initial_state(bool *initial_state)
                {
                    initial_state_ = initial_state;
                }

                /**
                 * @brief Sets the number of message repetitions to use while turning on the machine
                 *
                 * @param count number of repetitions
                 */
                void set_power_message_repetitions(uint count)
                {
                    power_message_repetitions_ = count;
                }

                /**
                 * @brief Processes and publish the new switch state.
                 */
                void update_state(bool state);

            private:
                /// @brief Reference to uart which is connected to the mainboard
                uart::UARTDevice *mainboard_uart_;
                /// @brief power pin which is used for display power
                GPIOPin *power_pin_;
                /// @brief True if the coffee machine is supposed to clean
                bool cleaning_ = true;
                /// @brief length of power outage applied to the display (can be overridden by YAML config)
                uint32_t power_trip_delay_ = 750;
                /// @brief delay after power restore before sending commands (display boot time)
                uint32_t display_boot_delay_ = 5000;
                /// @brief whether to invert the power pin logic
                bool invert_power_pin_ = false;
                /// @brief Determines wether a power trip should be performed
                bool should_power_trip_ = false;
                /// @brief Indicates if a power trip is currently in progress
                bool power_trip_active_ = false;
                /// @brief Time when the current power trip started
                uint32_t power_trip_start_time_ = 0;
                /// @brief Time of last power trip
                uint32_t last_power_trip_ = 0;
                /// @brief nr of power performed power trips
                uint power_trip_count_ = 0;
                /// @brief determines how often the power on message is repeated
                uint power_message_repetitions_ = 5;
                /// @brief End time of grace period after power-on (prevents premature OFF detection)
                uint32_t power_on_grace_period_end_ = 0;
                /// @brief Indicates if power-on commands are pending after power trip
                bool pending_power_on_commands_ = false;
                /// @brief Stores cleaning preference for pending power-on
                bool cleaning_pending_ = true;
                /// @brief Tracks if currently injecting power commands (blocks display messages)
                bool injecting_commands_ = false;
                /// @brief Timestamp when we should send pending power-on commands (after display boots)
                uint32_t send_commands_at_ = 0;
                /// @brief initial power state reference
                bool *initial_state_;
            };

        } // namespace philips_power_switch
    }     // namespace philips_coffee_machine
} // namespace esphome