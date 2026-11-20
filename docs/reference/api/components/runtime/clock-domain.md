---
id: reference.api.components.runtime.clock-domain
type: reference
audience: consumer
source_language: en
---

(reference.api.components.runtime.clock-domain)=

# Clock-domain ownership and coordination

A clock-domain owner coordinates admitted consumers and explicit transitions.
Clock identity, epochs, retained consumers and actual hardware outcomes remain
part of the selected contract.

Trusted static composition includes `clock_domain_owner.h`. Ordinary callers
keep the opaque interface; cold helpers and span validation remain private.
Storage fields are implementation state after construction, not application
controls.

## Public declarations

### clock_domain.h

```{doxygenfile} components/runtime/clock_domain/include/nexus/components/clock_domain.h
:project: nexus
```

### clock_domain_owner.h

```{doxygenfile} components/runtime/clock_domain/include/nexus/components/clock_domain_owner.h
:project: nexus
```
