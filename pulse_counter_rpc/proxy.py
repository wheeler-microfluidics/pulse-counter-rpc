import time
from datetime import datetime
import warnings

from path_helpers import path
try:
    from base_node_rpc.proxy import ConfigMixinBase, StateMixinBase
    import arduino_helpers.hardware.teensy as teensy
    from .node import (Proxy as _Proxy, I2cProxy as _I2cProxy,
                       SerialProxy as _SerialProxy)
    from .config import Config
    from .state import State

    class ProxyMixin(ConfigMixinBase, StateMixinBase):
        '''
        Mixin class to add convenience wrappers around methods of the generated
        `node.Proxy` class.

        For example, expose config and state getters/setters as attributes.
        '''
        host_package_name = str(path(__file__).parent.name.replace('_', '-'))

        def __init__(self, *args, **kwargs):
            super(ProxyMixin, self).__init__(*args, **kwargs)
            # Override device proxy `count_pulses(...)` method with
            # `__count_pulses` Python method (defined below).
            count_pulses = self.count_pulses
            self.count_pulses = self.__count_pulses
            self._count_pulses = count_pulses

        @property
        def config_class(self):
            return Config

        @property
        def state_class(self):
            return State

        def __count_pulses(self, pulse_pin, pulse_channel, duration_ms,
                           trigger_direction=teensy.RISING):
            '''
            Count the number of pulses that occur on a channel connected to an
            input pin within the specified duration.

            Args:

                pulse_pin (int) : Input pin to count pulses on.
                pulse_channel (int) : Multiplexer channel (select from multiple
                    sources).
                duration_ms (int) : Number of milliseconds to measure for.
                trigger_direction (int) : From
                    `arduino_helpers.hardware.teensy`; `RISING`, `FALLING`, or
                    `CHANGE`.
                timeout_s (int,float) : Maximum length of time to wait for
                    device to complete pulse count request.  If set to 0,
                    timeout is disabled.

            Returns:

                (int) : Number of pulses counted.
            '''
            if self.state.pulse_count_enable:
                raise IOError('Pulse count already in progress.')

            # Set options for pulse count operation.
            self.state = {'pulse_pin': pulse_pin,
                          'pulse_channel': pulse_channel,
                          'pulse_direction': trigger_direction}
            self.start_pulse_count()

            # Wait for specified duration.
            time.sleep(duration_ms * 1e-3)

            return self.stop_pulse_count()


    class Proxy(ProxyMixin, _Proxy):
        pass

    class I2cProxy(ProxyMixin, _I2cProxy):
        pass

    class SerialProxy(ProxyMixin, _SerialProxy):
        pass

except (ImportError, TypeError), exception:
    Proxy = None
    I2cProxy = None
    SerialProxy = None
    warnings.warn(str(exception))
