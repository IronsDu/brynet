#ifndef _EVENT_H_INCLUDED_
#define _EVENT_H_INCLUDED_

#ifdef  __cplusplus
extern "C" {
#endif

    /*  事件模块    */
    /*  TODO::优化内部内存分配    */

    #include "double_link.h"

    struct event_object_s
    {
        struct double_link_s    handlers;
        struct double_link_s    observers;
    };

    struct event_handler_s;

    typedef void (*fpn_event_handler)(struct event_object_s* root, struct event_object_s* source, void* data, const void* arg);

    struct event_object_s* event_object_create(void);

    void event_object_init(struct event_object_s* self);

    void event_object_removeall(struct event_object_s* self);

    void event_object_insert_observer(struct event_object_s* root, int event_id, struct event_object_s* observer);
    void event_object_remove_observer(struct event_object_s* root, int event_id, struct event_object_s* observer);

    void event_object_insert_handler(struct event_object_s* root, int event_id, fpn_event_handler callback, void* event_data);
    void event_object_remove_handler(struct event_object_s* root, int event_id, fpn_event_handler callback, void* event_data);

    void event_object_handler(struct event_object_s* root, struct event_object_s* source, int event_id, const void* arg);
    void event_object_nofify(struct event_object_s* root, int event_id, void* arg);

#ifdef  __cplusplus
}
#endif

#endif
