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
                        ESP_LOGD(TAG, "Power trip %d starting - initial_state: %d, invert_power_pin: %d", 
                                 power_trip_count_ + 1, initial, invert_power_pin_);
                        if (invert_power_pin_) {
                            initial = !initial;
                        }
                        bool trip_value = !initial;
                        ESP_LOGD(TAG, "Cutting power to display - setting pin from %d to %d (goal: cut power)", 
                                 initial, trip_value);
                        power_pin_->digital_write(trip_value);
                        power_trip_active_ = true;
                        power_trip_start_time_ = now;
                    }
                    
                    // Check if we need to restore power
                    if (power_trip_active_ && now - power_trip_start_time_ >= power_trip_delay_)
                    {
                        // Restore power to display
                        bool restore_value = *initial_state_;
                        if (invert_power_pin_) {
                            restore_value = !restore_value;
                        }
                        ESP_LOGD(TAG, "Restoring power to display - setting pin to %d (goal: restore power)", 
                                 restore_value);
                        power_pin_->digital_write(restore_value);
                        ESP_LOGD(TAG, "Completed power trip %d", power_trip_count_ + 1);
                        power_trip_active_ = false;
                        last_power_trip_ = now;
                        power_trip_count_++;
                        
                        // If this was the first power trip and we have pending commands, schedule them
                        if (power_trip_count_ == 1 && pending_power_on_commands_)
                        {
                            // Schedule command sending after display boot delay
                            // Display takes approximately 8-9 seconds to boot and start communicating
                            send_commands_at_ = millis() + display_boot_delay_;
                            ESP_LOGD(TAG, "Scheduled power-on commands to be sent in %d ms (at millis=%u)", 
                                     display_boot_delay_, send_commands_at_);
                            
                            // Set grace period to start NOW (when power is restored)
                            // Grace period must be longer than display boot delay to allow commands to be sent
                            uint32_t grace_period = display_boot_delay_ + 5000;  // Extra 5s buffer after commands
                            power_on_grace_period_end_ = millis() + grace_period;
                            ESP_LOGD(TAG, "Grace period set to %u ms (until millis=%u)", 
                                     grace_period, power_on_grace_period_end_);
                        }
                    }
                }
                
                // Check if it's time to send pending power-on commands
                if (pending_power_on_commands_ && send_commands_at_ > 0 && millis() >= send_commands_at_)
                {
                    ESP_LOGD(TAG, "Sending power-on commands after display boot delay");
                    
                    // Start blocking ALL display messages during automated power-on sequence
                    // This is OK because user initiated via phone/GUI, not physical button
                    injecting_commands_ = true;
                    
                    // Send commands multiple times with delays to catch the display as it boots
                    for (int attempt = 0; attempt < 3; attempt++)
                    {
                        ESP_LOGD(TAG, "Command attempt %d - sending %d pre-power + power messages", 
                                 attempt + 1, power_message_repetitions_ + 1);
                        
                        // Send pre-power on message
                        for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                            mainboard_uart_->write_array(command_pre_power_on);

                        // Send power on message
                        if (cleaning_pending_)
                        {
                            // Send power WITH cleaning (starts flush cycle)
                            ESP_LOGD(TAG, "Sending power-on WITH cleaning command");
                            for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                                mainboard_uart_->write_array(command_power_with_cleaning);
                        }
                        else
                        {
                            // Send power on command without cleaning
                            ESP_LOGD(TAG, "Sending power-on WITHOUT cleaning command");
                            for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                                mainboard_uart_->write_array(command_power_without_cleaning);
                        }

                        mainboard_uart_->flush();
                        
                        if (attempt < 2)
                            delay(300);  // Wait between attempts
                    }
                    
                    // Keep blocking for a bit longer to ensure commands reach mainboard
                    delay(500);
                    
                    // Stop blocking - physical button can work again
                    injecting_commands_ = false;
                    
                    pending_power_on_commands_ = false;
                    send_commands_at_ = 0;  // Clear scheduled time
                    
                    ESP_LOGD(TAG, "Power-on commands sent (3 attempts)");
                    
                    // Stop power tripping - we've done our job
                    should_power_trip_ = false;
                    ESP_LOGD(TAG, "Power trip sequence complete");
                }
            }

            void Power::write_state(bool state)
            {
                if (state)
                {
                    // Check if display is already communicating (machine already on)
                    if (this->state)
                    {
                        ESP_LOGD(TAG, "Power ON requested but display already communicating - just sending commands");
                        
                        // Block display messages ONLY while injecting our commands
                        injecting_commands_ = true;
                        
                        // Send pre-power on message
                        for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                            mainboard_uart_->write_array(command_pre_power_on);

                        // Send power on message
                        if (cleaning_)
                        {
                            ESP_LOGD(TAG, "Sending power-on WITH cleaning");
                            for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                                mainboard_uart_->write_array(command_power_with_cleaning);
                        }
                        else
                        {
                            ESP_LOGD(TAG, "Sending power-on WITHOUT cleaning");
                            for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                                mainboard_uart_->write_array(command_power_without_cleaning);
                        }
                        mainboard_uart_->flush();
                        
                        // Stop blocking immediately - allow physical button to work
                        injecting_commands_ = false;
                        
                        ESP_LOGD(TAG, "Power-on commands sent (no power trip needed)");
                        return;
                    }
                    
                    ESP_LOGD(TAG, "Power ON requested - display not communicating, will power trip first");
                    
                    // First, perform power trip to wake up display
                    // The display needs to be communicating for commands to work properly
                    should_power_trip_ = true;
                    power_trip_count_ = 0;
                    last_power_trip_ = 0; // Trigger immediately
                    
                    // Mark that we need to send power-on commands after power trip
                    pending_power_on_commands_ = true;
                    cleaning_pending_ = cleaning_;
                    send_commands_at_ = 0;  // Reset scheduled time (will be set after power trip)
                    
                    // Grace period will be set AFTER power trip completes in loop()
                    // Don't set it here - we need to start it when power is restored, not now
                    power_on_grace_period_end_ = 0;
                }
                else
                {
                    // Block display messages ONLY while injecting power-off command
                    injecting_commands_ = true;
                    
                    // Send power off message multiple times to ensure it's received
                    ESP_LOGD(TAG, "Sending power-off command (%d repetitions)", power_message_repetitions_ + 1);
                    for (unsigned int i = 0; i <= power_message_repetitions_; i++)
                        mainboard_uart_->write_array(command_power_off);
                    mainboard_uart_->flush();
                    
                    // Stop blocking immediately
                    injecting_commands_ = false;
                    
                    // Wait for machine to process power-off (without blocking physical button)
                    ESP_LOGD(TAG, "Waiting 2s for machine to process power-off command");
                    delay(2000);
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
                    // Silently ignore OFF states during grace period
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
                        // DON'T clear pending_power_on_commands_ or send_commands_at_ here!
                        // Commands are scheduled and will be sent at the right time in loop()
                    }
                    
                    // If machine is ON and we're in grace period, end grace period early
                    // This means power-on was successful and machine is responding
                    if (state && power_on_grace_period_end_ > 0)
                    {
                        ESP_LOGD(TAG, "Machine ON detected, ending grace period early");
                        power_on_grace_period_end_ = 0;
                    }

                    ESP_LOGD(TAG, "Publishing state change: %s (grace period end: %u, now: %u)", 
                             state ? "ON" : "OFF", power_on_grace_period_end_, now);
                    publish_state(state);
                    
                    // If transitioning to OFF after grace period, clear any power trip state
                    if (!state && now >= power_on_grace_period_end_)
                    {
                        should_power_trip_ = false;
                        power_trip_count_ = 0;
                        power_trip_active_ = false;
                        pending_power_on_commands_ = false;
                        send_commands_at_ = 0;  // Clear any scheduled commands
                    }
                }
            }

        } // namespace philips_power_switch
    }     // namespace philips_coffee_machine
} // namespace esphome