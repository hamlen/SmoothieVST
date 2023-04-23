# SmoothieVST

*Smoothie* is a VST3 for smoothing automation parameter changes within a DAW. It's useful for doing short fade-ins or fade-outs during a live performance.

To do smoothing, it exports triads of automation parameters:
- **InParam:** Change this parameter to set the new destination of the associated **OutParam**.
- **OutParam:** *Smoothie* gradually moves this parameter until it matches **InParam**.
- **Slowness:** Set this parameter to indicate how quickly **OutParam** should move. Setting **Slowness** to 0.0 makes **OutParam** move instantly (no smoothing). Setting it to 0.5 (the default) causes **OutParam** to take 2 seconds to move from 0.0 to 1.0. Setting it to 1.0 makes **OutParam** move infinitely slowly (it never changes). In general, if you want **OutParam** to take *n* seconds to move from 0.0 to 1.0, then set **Slowness** to *n* / (*n* + 2).

A typical usage is to bind **OutParam** to Gain in your DAW, and then bind one of your controller buttons to alternatingly send 1.0 and 0.0 to **InParam**. Pressing the bound button will then have the effect of fading your Gain in and out at the rate defined by **Slowness**.

*Smoothie* changes **OutParam** in response to changes in **InParam** with sample-accuracy, even when **InParam** undergoes continuous change (e.g., according to an automation curve). As **InParam** changes, **OutParam** chases it without exceeding the speed limit defined by **Slowness**. However, changes to **OutParam** and **Slowness** are subject to the host's block processing speed, and *Smoothie* does not attempt to smooth them. This allows you to instantly jump *OutParam* to a desired value (whereupon it will resume chasing **InParam**).

As **OutParam** slides to its destination, *Smoothie* also outputs MIDI CC messages (on channel 1, controller number 90) to approximate its movement. However, these values are not as smooth as reading **OutParam** (because CC values are restricted to integers from 0 to 127), so should only be used to communicate with devices that don't understand automation parameters. CC messages sent to *Smoothie* are interpreted as changes to **InParam**.

By default, *Smoothie* exports 8 triads of the above parameters, allowing you to smooth 8 independent parameters per VST instance. MIDI CC numbers 90-97 (channel 1) reflect each parameter. To smooth more parameters, just load multiple instances of *Smoothie*.
