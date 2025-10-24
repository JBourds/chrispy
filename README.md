# Chrispy

High-performance multichannel ADC sampling and audio recording.
Developed as part of the CIROH research project at the University of Vermont.

Author: Jordan Bourdeau

API Documentation: [jbourds.github.io/chrispy](https://jbourds.github.io/chrispy)

GitHub Repository: [github.com/jbourds/chrispy](https://github.com/jbourds/chrispy)

## Repository Layout

- `examples`: Example programs showcasing various uses of library APIs.
- `src`: Source code containing all .cpp and .h files.
- `helpers`: Helper programs/scripts.
- `doc`: Doxygen setup.
- `library.json`: Repo metadata for PlatformIO.
- `library.properties`: Repo metadata for Arduino.

## Example Programs

Example programs were written and tested for the [AURA board](https://ieeexplore.ieee.org/document/11175467)
developed as part of the research project this work was a part of.
The AURA board has an ATMEGA2560 processor and shares many of the pin numbers
as the Arduino [Mayfly Data Logger](https://www.envirodiy.org/mayfly/).

## Theory of Operation

### Selecting Input Channels

A list of input channels is specified with an array of [`Channel`](https://jbourds.github.io/chrispy/structadc_1_1Channel.html)
instances. These are simple data objects which just contain information about
the pin and (optionally) power pin required along with information on whether
it is active low or active high. This information is uses by the `adc` module to
ensure all input channels are configured as inputs and are being powered prior
to beginning recording. The `pin` field of the `Channel` struct also maps to a
3-bit binary string which is used in the ADC's ADMUX register to select which
channel is active.

### Configuring the ADC

The ADC controls are implemented as freestanding functions within the `adc`
module. An object-oriented approach was not opted for here, as the ADC is
rightfully a singleton object (there is only a single hardware ADC) and should
not be able to be moved around.

Prior to use, the ADC must be initialized with the [`init`](https://jbourds.github.io/chrispy/namespaceadc.html#ad1e0fe0282574d1733c87c25737966a5)
function. The `adc` module requires initialization before being able to be used
for anything. This can be done as many times as desired to change parameters,
but must be done when the ADC is inactive. Initialization requires the set of
channels to be recorded from, as well as a buffer and buffer size to use. The
buffer size must be a power of 2. Recommendations are to use a factor of 1024.
This ie because whatever buffer is supplied gets subdivided into 2 buffers when
used by the ISR. `SdFat` can transmit 512 bytes at a time and so passing in
memory which is an increment of 1024 ensures the size of each individual
buffer coincides with the optimal size for `SdFat`. This is crucial in high
performance sampling applications such as audio recording which can sample at
40kHz and above and requires optimized performance when writing incoming data
out to the SD card.

One the `adc` module has been initialized, the ADC can be started. The [`start`](https://jbourds.github.io/chrispy/namespaceadc.html#ae4487b3f66a694f51d662dbed5590052)
function requires parameters for the bit resolution to use, per-channel sample
rate, the number of samples per channel before switching (must be a power of 2),
and the number of
milliseconds to wait as a warmup period after beginning the ADC before returning
from the function. This step configures the following:

- Timings for sampling each channel at the desired clock rate (ADC prescaler,
Timer1 compare/match values)
- Global ADC frame referenced by the ISR when ingesting samples.
- ADC registers to begin autotriggering interrupts

### Multi-Channel Switching

This library supports alternating between channels every `n` samples, where `n`
is some increment of 2. Note that this *does add noise* to the target signal.
This is a result of residual noise from the previous channel not being fully
dissipated within the ADC. Future work for this library will look for ways to
cut down on this noise.

The `adc` module supports multiple channels by further partitioning each of its
double buffers into `nchannels` sub-buffers. This way, wheneover a new channel
is swapped to it can simply update the write-head to the new channel buffer.
This design was preferred over writing samples interleaved between samples
because it allows for large buffers of channel data to be written out at a time
after being retrieved with `swap_buffer`. This function takes a `size_t` by
reference and will put the channel index a buffer is from whenever a new buffer
is received. This design is preferred, because it requires less work to be done
by the caller when sorting samples. If samples were interleaved, it would
require iterating over every sample in the buffer to place each sample with its
corresponding channel.

### Ingesting ADC Data

Once the `adc` module has been started, the interrupt service routine within
the module will be triggered every time a sample becomes ready. Internally, the
ISR is writing to a double buffer split from the buffer earlier passed in by
the user to `init`. Once one buffer fills up, the ISR will jump to the next. At
this point, the earlier buffer becomes available to be written out. The intended
workflow is for the application to call the [`swap_buffer`](https://jbourds.github.io/chrispy/namespaceadc.html#ad3820624ae8adaa92589f2b847755b30)
function in a loop, taking the desired action whenever a buffer becomes available
before calling the function again with the buffer it just wrote out. This function
essentially "lends" a full ADC buffer to the caller and expects to receive it
back through the same function. Once the function is called again with a full
buffer, it internally marks the buffer as free to write to again.

The `single_channel_adc` and `multi_channel_adc` examples demonstrate how to
ingest high amounts of data from the ADC with audio recording as an example.

### Recording

The primary goal of this library was to enable high-performance recording from
multiple audio channels concurrently. While other interfaces (`Adc.h`, `Timer.h`)
were kept fairly generic, the `recording` module provides a simple interface for
rapidly ingesting audio data to files on the SD card.

Similar to the `adc` module,
an initialization function must be called prior to recording. The [`init`](https://jbourds.github.io/chrispy/namespacerecording.html#a127b7f12a32c210317b0cb5696eeb2d7)
function configures the set of channels to be used, as well as a pointer to the
`SD` object for accessing the board's micro SD card.

The only other function is [`record`](https://jbourds.github.io/chrispy/namespacerecording.html#ac3a362134397d1b0fe8c9ef1568138f7)
which takes the list of filenames for each channel and all the necessary
parameters for configuring the ADC. This function configures the ADC, creates
all the files for recording, ingests data from the ADC into each file, truncates
files down to a common size, and then closes all files and deactivates the ADC.

The `single_channel_recording` and `multi_channel_recording` examples
demonstrate how to use this API for recording to SD card files.
