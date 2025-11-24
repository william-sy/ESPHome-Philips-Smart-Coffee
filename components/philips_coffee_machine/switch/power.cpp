#include "esphome/core/log.h"
#include "power.h"

namespace esphome
{
    namespace philips_coffee_machine
    {
        namespace philips_power_switch
        {

            static const char *TAG = "philips_power_switch";

            void Power::setup()
            {
            }

            void Power::loop()
            {
                if (should_power_trip_)
                {
                    uint32_t now = millis();
                    
                    // Check if we need to start a new power trip
                    if (!power_trip_active_ && now - last_power_trip_ > power_trip_delay_ + POWER_TRIP_RETRY_DELAY)
                    {
                        if (power_trip_count_ >= MAX_POWER_TRIP_COUNT)
                        {
                            should_power_trip_ = false;
                            ESP_LOGE(TAG, "Power tripping display failed!");
                            return;
                        }

                        // Start power trip - cut power to display
                        bool initial = *initial_state_;
                        bool trip_value = !initial;
                        ESP_LOGD(TAG, "Starting power trip %d - setting pin from %d to %d", 
                                 power_trip_count_ + 1, initial, trip_value);
                        power_pin_->digital_write(trip_value);
                        power_trip_active_ = true;
                        power_trip_start_time_ = now;
                    }
                    
                    // Check if we need to restore power
                    if (power_trip_active_ && now - power_trip_start_time_ >= power_trip_delay_)
                    {
                        // Restore power to display
                        bool restore_value = *initial_state_;
                        ESP_LOGD(TAG, "Completed power trip %d - restoring pin to %d", 
                                 power_trip_count_ + 1, restore_value);
                        power_pin_->digital_write(restore_value);
                        power_trip_active_ = false;
                        last_power_trip_ = now;
                        power_trip_count_++;
                        
                        // If this was the first power trip and we have pending commands, send them now
                        if (power_trip_count_ == 1 && pending_power_on_commands_)
                        {
                            // Wait a moment for display to start booting
                            delay(500);
                            
                            ESP_LOGD(TAG, "Sending power-on commands after display boot");
                            
                            // Send pre-power on message
                            for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                                mainboard_uart_->write_array(command_pre_power_on);

                            // Send power on message
                            if (cleaning_pending_)
                            {
                                // Send power on command with cleaning
                                for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                                    mainboard_uart_->write_array(command_power_with_cleaning);
                            }
                            else
                            {
                                // Send power on command without cleaning
                                for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                                    mainboard_uart_->write_array(command_power_without_cleaning);
                            }

                            mainboard_uart_->flush();
                            pending_power_on_commands_ = false;
                            
                            ESP_LOGD(TAG, "Power-on commands sent");
                        }
                    }
                }
            }

            void Power::write_state(bool state)
            {
                if (state)
                {
                    ESP_LOGD(TAG, "Power ON requested - will power trip first, then send commands");
                    
                    // First, perform power trip to wake up display
                    // The display needs to be communicating for commands to work properly
                    should_power_trip_ = true;
                    power_trip_count_ = 0;
                    last_power_trip_ = 0; // Trigger immediately
                    
                    // Mark that we need to send power-on commands after power trip
                    pending_power_on_commands_ = true;
                    cleaning_pending_ = cleaning_;
                    
                    // Set grace period to prevent immediate OFF detection
                    // Display needs time to boot after power trip
                    power_on_grace_period_end_ = millis() + POWER_ON_GRACE_PERIOD;
                }
                else
                {
                    // Send power off message
                    for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                        mainboard_uart_->write_array(command_power_off);
                    mainboard_uart_->flush();
                }

                // The state will be published once the display starts sending messages
            }

            void Power::dump_config()
            {
                ESP_LOGCONFIG(TAG, "Philips Coffee Machine Power Switch");
            }

            void Power::update_state(bool state)
            {
                uint32_t now = millis();
                
                // During grace period after power-on, ignore OFF state
                // Give the display time to boot and start communicating
                if (!state && now < power_on_grace_period_end_)
                {
                    ESP_LOGD(TAG, "Ignoring OFF state during power-on grace period (remaining: %u ms)", 
                             power_on_grace_period_end_ - now);
                    return;
                }
                
                if (this->state != state)
                {
                    // Only stop power trip if we completed at least one full trip cycle
                    // Don't stop if display happens to be already communicating
                    if (state && should_power_trip_ && power_trip_count_ > 0)
                    {
                        ESP_LOGD(TAG, "Performed %i power trip(s).", power_trip_count_);
                        should_power_trip_ = false;
                        power_trip_count_ = 0;
                        power_trip_active_ = false;
                    }

                    ESP_LOGD(TAG, "Publishing state change: %s (grace period end: %u, now: %u)", 
                             state ? "ON" : "OFF", power_on_grace_period_end_, now);
                    publish_state(state);
                }
            }

        } // namespace philips_power_switch
    }     // namespace philips_coffee_machine
} // namespace esphome