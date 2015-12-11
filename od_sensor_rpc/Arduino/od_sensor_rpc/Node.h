#ifndef ___NODE__H___
#define ___NODE__H___

#include <stdint.h>
#include <Arduino.h>
#include <NadaMQ.h>
#include <CArrayDefs.h>
#include "RPCBuffer.h"  // Define packet sizes
#include "OdSensorRpc/Properties.h"  // Define package name, URL, etc.
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
#include "od_sensor_rpc_config_validate.h"
#include "od_sensor_rpc_state_validate.h"
#include "SimpleTimer.h"
#include "OdSensorRpc/config_pb.h"


namespace od_sensor_rpc {
const size_t FRAME_SIZE = (3 * sizeof(uint8_t)  // Frame boundary
                           - sizeof(uint16_t)  // UUID
                           - sizeof(uint16_t)  // Payload length
                           - sizeof(uint16_t));  // CRC

class Node;

typedef nanopb::EepromMessage<od_sensor_rpc_Config,
                              config_validate::Validator<Node> > config_t;
typedef nanopb::Message<od_sensor_rpc_State,
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

  static const uint16_t BUFFER_SIZE = 128;  // >= longest property string
  uint8_t buffer_[BUFFER_SIZE];

  static const uint16_t MUX_CHANNEL_A_PIN = 4;
  static const uint16_t MUX_CHANNEL_B_PIN = 8;
  int16_t PULSE_PIN_;
  uint16_t pulse_channel_;

  bool pulse_count_enable_;
  uint32_t pulse_count_;
  SimpleTimer timer_;

  Node() : BaseNode(), BaseNodeConfig<config_t>(od_sensor_rpc_Config_fields),
           BaseNodeState<state_t>(od_sensor_rpc_State_fields), PULSE_PIN_(-1),
           pulse_channel_(0), pulse_count_enable_(false), pulse_count_(0) {
    pinMode(MUX_CHANNEL_A_PIN, OUTPUT);
    pinMode(MUX_CHANNEL_B_PIN, OUTPUT);
    set_pulse_channel(pulse_channel_);
  }

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

  void set_pulse_pin(uint8_t pulse_pin, uint8_t direction) {
    if (PULSE_PIN_ >= 0) {
      detachInterrupt(PULSE_PIN_);
    }
    PULSE_PIN_ = pulse_pin;
    pinMode(PULSE_PIN_, INPUT);
    attachInterrupt(PULSE_PIN_, od_sensor_rpc::pulse_handler,
                    direction);
  }
  int count_pulses(uint32_t duration_ms) {
     // Disable pulse counting after duration of `delay_ms`.
    int timer_id = timer_.setTimeout(duration_ms,
                                     &od_sensor_rpc::on_timeout);
    start_pulse_count();
    return timer_id;
  }
  void start_pulse_count() {
    // Reset pulse count and enable counting.
    pulse_count_ = 0;
    pulse_count_enable_ = true;
  }
  uint32_t stop_pulse_count() {
    pulse_count_enable_ = false;
    return pulse_count_;
  }
  uint32_t pulse_count() const { return pulse_count_; }
  uint32_t pulse_count_enable() const { return pulse_count_enable_; }

  bool set_pulse_channel(uint8_t pulse_channel) {
    // Since pulse channel address is only two bits, max channel address is 3.
    if (pulse_channel > 3) { return false; }

    // Set multiplexer select channels based on selected channel address.
    digitalWrite(MUX_CHANNEL_A_PIN, (pulse_channel & 0x1) ? HIGH : LOW);
    digitalWrite(MUX_CHANNEL_B_PIN, (pulse_channel & 0x2) ? HIGH : LOW);
    pulse_channel_ = pulse_channel;
    return true;
  }
  uint8_t pulse_channel() const { return pulse_channel_; }
};

}  // namespace od_sensor_rpc


#endif  // #ifndef ___NODE__H___
