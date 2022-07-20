#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stddef.h>

typedef struct Hoard* hoard_t;

hoard_t hoard_init();
int     hoard_put(hoard_t h, int64_t* src, size_t count, size_t offset);
int     hoard_get(hoard_t h, int64_t* dest, size_t count, size_t offset);
void    hoard_finalize(hoard_t h);
#ifdef __cplusplus
}
#endif
