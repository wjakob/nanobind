#include <nanobind/nb_defs.h>
#include <memory>

#if defined(SHARED_BUILD)
#  define EXPORT_SHARED NB_EXPORT
#else
#  define EXPORT_SHARED NB_IMPORT
#endif

struct EXPORT_SHARED Shared {
    int value;
};

enum class EXPORT_SHARED SharedEnum { One = 1, Two = 2 };

extern EXPORT_SHARED Shared create_shared();
extern EXPORT_SHARED std::shared_ptr<Shared> create_shared_sp();
extern EXPORT_SHARED std::unique_ptr<Shared> create_shared_up();
extern EXPORT_SHARED bool check_shared(const Shared &shared);
extern EXPORT_SHARED bool check_shared_sp(std::shared_ptr<Shared> shared);
extern EXPORT_SHARED bool check_shared_up(std::unique_ptr<Shared> shared);

extern EXPORT_SHARED SharedEnum create_enum();
extern EXPORT_SHARED bool check_enum(SharedEnum e);

extern EXPORT_SHARED void throw_shared();
