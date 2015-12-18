import time
from datetime import datetime
import warnings

from path_helpers import path
try:
    import arduino_helpers.hardware.teensy as teensy
    from .node import (Proxy as _Proxy, I2cProxy as _I2cProxy,
                       SerialProxy as _SerialProxy)

    class ProxyMixin(object):
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

        def __count_pulses(self, pulse_pin, pulse_channel, duration_ms,
                           trigger_direction=teensy.RISING, timeout_s=0):
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
            if not self.update_state(pulse_channel=pulse_channel,
                                     pulse_pin=pulse_pin,
                                     pulse_direction=trigger_direction):
                raise IOError('Error updating state.')
            self._count_pulses(duration_ms)

            # Wait for the expected duration.
            time.sleep(duration_ms / 1e3)

            # Poll until pulse count is complete (or until timeout is reached).
            start = datetime.now()
            while timeout_s == 0 or ((datetime.now() - start).total_seconds() <
                                     timeout_s):
                state = self.state
                if not state.pulse_count_enable:
                    # Pulse counting has finished.
                    return state.pulse_count
            raise RuntimeError('Timed out waiting for pulse count to '
                               'complete.')

        @property
        def config(self):
            from .config import Config

            return Config.FromString(self.serialize_config().tostring())

        @config.setter
        def config(self, value):
            return self.update_config(value)

        @property
        def state(self):
            from .config import State

            return State.FromString(self.serialize_state().tostring())

        @state.setter
        def state(self, value):
            return self.update_state(value)

        def update_config(self, **kwargs):
            '''
            Update fields in the config object based on keyword arguments.

            By default, these values will be saved to EEPROM. To prevent this
            (e.g., to verify system behavior before committing the changes),
            you can pass the special keyword argument 'save=False'. In this case,
            you will need to call the method save_config() to make your changes
            persistent.
            '''

            from .config import Config

            save = True
            if 'save' in kwargs.keys() and not kwargs.pop('save'):
                save = False

            config = Config(**kwargs)
            return_code = super(ProxyMixin, self).update_config(config)

            if save:
                super(ProxyMixin, self).save_config()

            return return_code

        def update_state(self, **kwargs):
            from .config import State

            state = State(**kwargs)
            return super(ProxyMixin, self).update_state(state)

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
