---
id: reference.api.io.media
type: reference
audience: consumer
source_language: en
---

(reference.api.io.media)=

# Audio, image capture and display faces

Frames, audio samples, image operations and display commands retain their
declared geometry and storage ownership. A completed memory transfer is not by
itself a complete frame, wire transfer or safe display-buffer release.

## Public declarations

### audio.h

```{doxygenfile} io/include/nexus/io/audio.h
:project: nexus
```

### capture.h

```{doxygenfile} io/include/nexus/io/capture.h
:project: nexus
```

### display.h

```{doxygenfile} io/include/nexus/io/display.h
:project: nexus
```

### image.h

```{doxygenfile} io/include/nexus/io/image.h
:project: nexus
```
