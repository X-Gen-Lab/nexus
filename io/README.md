# Typed I/O

GPIO, UART, SPI, I2C, Flash, watchdog, EXTI, PWM and ADC expose C11 interfaces.
The optional [GPIO pin-state contract](../docs/explanation/gpio-pin-states.md)
adds an explicit selected face for authored DEFAULT/SLEEP images. Its generated
instance ID and runtime selection enum remain separate; lookup never acquires
pad ownership or starts a power transition.
Each selected instance has an immutable face with two pointers: shared read-only
methods and a pointer to private provider state. Generated faces live in read-only
storage; their state and selected buffers are private static objects in the
generated translation unit. Manually assembled providers use storage supplied
by their explicit constructor owner. A 32-bit target spends eight bytes of Flash
per face; mutable state and exact-capacity buffers are separate. Method pointers are shared per provider and
mode rather than repeated in every mutable instance.

Public operations check the face and dispatch its exact context. Different
provider tables can coexist in one image without redefining public symbols.
A missing optional method reports `NX_ERROR_UNSUPPORTED`; invalid faces reject
before provider access. A method must preserve the public operation's execution,
deadline, buffer ownership and failure contract. Provider headers, register
layouts and SDK types stay private.

Generated typed factory IDs select instances and distinguish SPI/I2C controllers
from their child endpoints. Lookup returns a static identity. Platform start,
request admission, service, recovery and stop remain explicit; lookup performs
no hardware initialization, allocation, name search or locking. Faces remain
valid while provider storage exists, so stopping requires caller and IRQ
quiescence rather than promising that a pointer has been revoked.

Stream reserve, publish, acquire, release, abort and stop access constant
metadata independent of block count. Stop checks an outstanding-block count;
filling, ready and borrowed blocks remain counted until proved abort or matching
release. It never scans the block array or revokes a loan under the interrupt
mask. Storage overlap validation remains a cold assembly operation whose work
grows with block count. This complexity contract is not physical IRQ latency or
cycle qualification.

Default paths need no OS locks or hidden tasks. Requests and transfer buffers
remain caller-owned; generated provider buffers are assembly-owned and separate.
UART tracks physical TC separately from loaded bytes and request release; error-only RX events never invent data. SPI
and I2C preserve their bounded polling contracts, and controller recovery never
replays a failed transaction. Flash exposes physical geometry; products provide
explicit restricted regions.

The Native model owns independent state per instance. Its explicit default
fixtures support focused tests; generated assemblies use separate state and
buffers. Native behavior, production-register fault models and ARM link fixtures
establish software evidence. Board facts authorize reviewed physical routes;
software-only fixtures do not establish PCB connectivity or physical timing.
