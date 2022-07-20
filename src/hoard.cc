#include "hoard.hpp"
#include "hoard-c.h"

hoard_t hoard_init()
{
    Hoard* h = new (Hoard);
    return h;
}
int hoard_put(hoard_t h, int64_t* src, size_t count, size_t offset)
{
    return (h->put(src, count, offset));
}
int hoard_get(hoard_t h, int64_t* dest, size_t count, size_t offset)
{
    return h->get(dest, count, offset);
}
void hoard_finalize(hoard_t h) { delete h; }
