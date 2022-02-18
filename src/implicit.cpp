#include <nanobind/trampoline.h>
#include "internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

void implicitly_convertible(const std::type_info *src,
                            const std::type_info *dst) noexcept {
    internals &internals = internals_get();

    auto it1 = internals.type_c2p.find(std::type_index(*src));
    auto it2 = internals.type_c2p.find(std::type_index(*dst));

    bool src_unknown = it1 == internals.type_c2p.end(),
         dst_unknown = it2 == internals.type_c2p.end();

    if (src_unknown || dst_unknown) {
        char *src_name = type_name(src),
             *dst_name = type_name(dst);

        const char *message =
            src_unknown
                ? (dst_unknown ? "both types unknown" : "source type unknown")
                : "destination type unknown";

        fail("nanobind::detail::implicitly_convertible(src=%s, dst=%s): %s!",
             src_name, dst_name, message);
    }

}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
