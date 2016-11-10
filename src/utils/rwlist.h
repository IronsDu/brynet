#ifndef _RWLIST_H_INCLUDED_
#define _RWLIST_H_INCLUDED_

#ifdef  __cplusplus
extern "C" {
#endif

struct rwlist_s;
struct stack_s;

struct rwlist_s* ox_rwlist_new(int num, int element_size, int max_pengding_num);
void ox_rwlist_delete(struct rwlist_s* self);
void ox_rwlist_push(struct rwlist_s* self, const void* data);

char*   ox_rwlist_front(struct rwlist_s* self, int timeout);
char*   ox_rwlist_pop(struct rwlist_s* self, int timeout);

void    ox_rwlist_flush(struct rwlist_s* self);
void    ox_rwlist_force_flush(struct rwlist_s* self);

bool    ox_rwlist_allempty(struct rwlist_s* self);

#ifdef  __cplusplus
}
#endif

#endif
