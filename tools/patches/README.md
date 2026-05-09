# Submodule patches

Patches that must be applied to vendored submodules whose upstreams we don't
control. Apply after `git submodule update --init --recursive`.

## embeddedproto-cstring.patch

Adds `#include <cstring>` to `WriteBufferFixedSize.h`. Upstream
(Embedded-AMS/EmbeddedProto) uses `memcpy` without including the header,
which fails to compile under `arm-none-eabi-g++`.

Apply:

```sh
git -C Middlewares/Third_Party/EmbeddedProto apply ../../../tools/patches/embeddedproto-cstring.patch
```

To check whether it's already applied:

```sh
git -C Middlewares/Third_Party/EmbeddedProto diff --quiet src/WriteBufferFixedSize.h && echo "not applied" || echo "applied"
```
