# Typed I/O

GPIO, UART, SPI, I2C, Flash, watchdog, EXTI, PWM and ADC expose C11 interfaces.
Each selected instance has an immutable face with two pointers: shared read-only
methods and caller-owned provider state. Generated faces live in read-only
storage. A 32-bit target spends eight bytes of Flash per face; mutable state and
exact-capacity buffers are separate. Method pointers are shared per provider and
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

Default paths need no OS locks or hidden tasks. Requests and exact-capacity
buffers are caller or assembly owned. UART tracks physical TC separately from
loaded bytes and request release; error-only RX events never invent data. SPI
and I2C preserve their bounded polling contracts, and controller recovery never
replays a failed transaction. Flash exposes physical geometry; products provide
explicit restricted regions.

The Native model owns independent state per instance. Its explicit default
fixtures support focused tests; generated assemblies use separate state and
buffers. Native behavior, production-register fault models and ARM link fixtures
establish software evidence. Board facts authorize reviewed physical routes;
software-only fixtures do not establish PCB connectivity or physical timing.
