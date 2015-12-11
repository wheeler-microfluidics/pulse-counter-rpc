#ifndef ___NODE__H___
#define ___NODE__H___

#include <stdint.h>
#include <Arduino.h>
#include <NadaMQ.h>
#include <CArrayDefs.h>
#include "RPCBuffer.h"  // Define packet sizes
#include "PulseCounterRpc/Properties.h"  // Define package name, URL, etc.
#include <BaseNodeRpc/BaseNodeRpc.h>
#include <BaseNodeRpc/BaseNode.h>
#include <BaseNodeRpc/BaseNodeEeprom.h>
#include <BaseNodeRpc/BaseNodeI2c.h>
#include <BaseNodeRpc/BaseNodeConfig.h>
#include <BaseNodeRpc/BaseNodeState.h>
#include <BaseNodeRpc/BaseNodeSerialHandler.h>
#include <BaseNodeRpc/BaseNodeI2cHandler.h>
#include <BaseNodeRpc/I2cHandler.h>
#include <BaseNodeRpc/SerialHandler.h>
#include <pb_validate.h>
#include <pb_eeprom.h>
#include "pulse_counter_rpc_config_validate.h"
#include "pulse_counter_rpc_state_validate.h"
#include "SimpleTimer.h"
#include "PulseCounterRpc/config_pb.h"


namespace pulse_counter_rpc {
const size_t FRAME_SIZE = (3 * sizeof(uint8_t)  // Frame boundary
                           - sizeof(uint16_t)  // UUID
                           - sizeof(uint16_t)  // Payload length
                           - sizeof(uint16_t));  // CRC

class Node;

typedef nanopb::EepromMessage<pulse_counter_rpc_Config,
                              config_validate::Validator<Node> > config_t;
typedef nanopb::Message<pulse_counter_rpc_State,
                        state_validate::Validator<Node> > state_t;
void pulse_handler();
void on_timeout();

class Node :
  public BaseNode,
  public BaseNodeEeprom,
  public BaseNodeI2c,
  public BaseNodeConfig<config_t>,
  public BaseNodeState<state_t>,
#ifndef DISABLE_SERIAL
  public BaseNodeSerialHandler,
#endif  // #ifndef DISABLE_SERIAL
  public BaseNodeI2cHandler<base_node_rpc::i2c_handler_t> {
public:
  typedef PacketParser<FixedPacket> parser_t;

  static const uint16_t BUFFER_SIZE = 64;  // >= longest property string
  uint8_t buffer_[BUFFER_SIZE];

  SimpleTimer timer_;

  Node() : BaseNode(), BaseNodeConfig<config_t>(pulse_counter_rpc_Config_fields),
           BaseNodeState<state_t>(pulse_counter_rpc_State_fields) {}

  UInt8Array get_buffer() { return UInt8Array_init(sizeof(buffer_), buffer_); }
  /* This is a required method to provide a temporary buffer to the
   * `BaseNode...` classes. */

  void begin();
  void set_i2c_address(uint8_t value);  // Override to validate i2c address

  /****************************************************************************
   * # User-defined methods #
   *
   * Add new methods below.  When Python package is generated using the
   * command, `paver sdist` from the project root directory, the signatures of
   * the methods below will be scanned and code will automatically be generated
   * to support calling the methods from Python over a serial connection.
   *
   * e.g.
   *
   *     bool less_than(float a, float b) { return a < b; }
   *
   * See [`arduino_rpc`][1] and [`base_node_rpc`][2] for more details.
   *
   * [1]: https://github.com/wheeler-microfluidics/arduino_rpc
   * [2]: https://github.com/wheeler-microfluidics/base_node_rpc
   */
  void loop() { timer_.run(); }

  bool on_config_mux_channel_a_pin_changed(uint32_t mux_channel_a_pin) {
    // Set pin mode for MUX A address channel pin to output.
    pinMode(mux_channel_a_pin, OUTPUT);
    // Re-apply pulse channel since MUX pin for A address channel has changed.
    on_state_pulse_channel_changed(state_._.pulse_channel);
    return true;
  }
  bool on_config_mux_channel_b_pin_changed(uint32_t mux_channel_b_pin) {
    // Set pin mode for MUX B address channel pin to output.
    pinMode(mux_channel_b_pin, OUTPUT);
    // Re-apply pulse channel since MUX pin for B address channel has changed.
    on_state_pulse_channel_changed(state_._.pulse_channel);
    return true;
  }
  bool on_state_pulse_pin_changed(int32_t old_pulse_pin, int32_t pulse_pin) {
    set_pulse_pin(pulse_pin);
    return true;
  }
  bool on_state_pulse_direction_changed() { return true; }
  bool on_state_pulse_channel_changed(uint32_t pulse_channel) {
    // Since pulse channel address is only two bits, max channel address is 3.
    if (pulse_channel > 3) { return false; }

    // Set multiplexer select channels based on selected channel address.
    digitalWrite(config_._.mux_channel_a_pin,
                 (pulse_channel & 0x1) ? HIGH : LOW);
    digitalWrite(config_._.mux_channel_b_pin,
                 (pulse_channel & 0x2) ? HIGH : LOW);
    return true;
  }

  void set_pulse_pin(uint8_t pulse_pin) {
    /* Configure the pin to count pulses on (including attaching appropriate
     * interrupt).
     *
     * Args:
     *
     *     pulse_pin (uint8_t) : Pin to count pulses on.
     */
    if (state_._.pulse_pin >= 0) { detachInterrupt(state_._.pulse_pin); }
    pinMode(pulse_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(pulse_pin),
                    pulse_counter_rpc::pulse_handler, RISING);
  }
  int count_pulses(uint32_t duration_ms) {
    /* Start pulse counting and start count down timer to stop pulse counting
     * after specified duration.
     *
     * Args:
     *
     *     duration_ms (uint32_t) : Duration (in milliseconds) to activate
     *         pulse counting.
     *
     * Returns:
     *
     *     (int) : Identifier of countdown timer assigned to trigger pulse
     *         counting to stop.
     */
    // Disable pulse counting after duration of `delay_ms`.
    int timer_id = timer_.setTimeout(duration_ms,
                                     &pulse_counter_rpc::on_timeout);
    start_pulse_count();
    return timer_id;
  }
  void start_pulse_count() {
    /* Reset pulse count and enable counting. */
    state_._.pulse_count = 0;
    state_._.has_pulse_count = true;
    state_._.pulse_count_enable = true;
    state_._.has_pulse_count_enable = true;
  }
  uint32_t stop_pulse_count() {
    /* Stop counting pulses.
     *
     * Returns:
     *
     *     (uint32_t) : Number of pulses counted.
     */
    state_._.pulse_count_enable = false;
    state_._.has_pulse_count_enable = true;
    return state_._.pulse_count;
  }
};

}  // namespace pulse_counter_rpc


#endif  // #ifndef ___NODE__H___
