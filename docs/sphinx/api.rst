Public API
==========

Doxygen builds current public include trees and excludes private SoC storage,
vendor SDKs, tests and archived APIs. Contracts specify context, absolute
deadlines, retained storage and failure state. Generated bindings belong to
one resolved assembly.

The index includes current typed I/O, optional backend adapters and explicitly
selected components. Optional public contracts include ``threadx.h``,
``usb_device.h``, ``ethernet.h``, ``mdio.h``, ``timer_capture.h``,
``advanced_pwm.h``, ``user_io_broker.h`` and ``dp83848.h``. Their presence does not
authorize an unsupported Board route, userspace transport or physical claim.

Factory lookup returns an existing selected object. Default compact IDs,
optional class-specific references and read-only capability records do not
allocate objects or initialize devices. Backend-specific task controls expose
their selected kernel storage contract; device faces keep vendor storage
private. Product tasks, protocol stacks, recovery and station policy remain
external to this API.

.. doxygenindex::
   :project: nexus
