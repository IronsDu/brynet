#include <stdlib.h>
#include <string.h>

#include "event.h"

/*  事件结处理器定义    */
struct handler_data_s
{
    fpn_event_handler callback;   /*  事件处理函数  */
    void* data;                   /*  事件参数     */
};

struct handler_node_s
{
    struct double_link_node_s   node;
    struct handler_data_s       data;
};

struct observer_node_s
{
    struct double_link_node_s   node;
    struct event_object_s*            obj;
};

struct handle_list_s
{
    struct double_link_node_s   node;
    int type;
    struct double_link_s    handlers;
};

struct observer_list_s
{
    struct double_link_node_s   node;
    int type;
    struct double_link_s    observers;
};

struct event_object_s* 
event_object_create(void)
{
    struct event_object_s* ret = (struct event_object_s*)malloc(sizeof(struct event_object_s));
    if(NULL != ret)
    {
        double_link_init(&ret->handlers);
        double_link_init(&ret->observers);
    }
    
    return ret;
}

void
event_object_init(struct event_object_s* self)
{
    double_link_init(&self->handlers);
    double_link_init(&self->observers);
}

void
event_object_removeall(struct event_object_s* self)
{
    struct handle_list_s* handlers = NULL;
    struct double_link_node_s* begin = double_link_begin(&self->handlers);
    struct double_link_node_s* end = double_link_end(&self->handlers);

    /*  查找已经存在的链表   */
    for(; begin != end;)
    {
        struct handle_list_s* temp = (struct handle_list_s*)begin;

        struct double_link_node_s* begin_handle = double_link_begin((struct double_link_s*)&temp->handlers);
        struct double_link_node_s* end_handle = double_link_end((struct double_link_s*)&temp->handlers);

        for(; begin_handle != end_handle;)
        {
            struct handler_node_s* node = (struct handler_node_s*)begin_handle;
            begin_handle = begin_handle->next;
            free(node);
        }

        begin = begin->next;
        free(temp);
    }

    double_link_init(&self->handlers);
    double_link_init(&self->observers);
}

static struct handle_list_s*
find_handle_list(struct event_object_s* root, int event_id)
{
    struct handle_list_s* handlers = NULL;
    struct double_link_node_s* begin = double_link_begin(&root->handlers);
    struct double_link_node_s* end = double_link_end(&root->handlers);

    /*  查找已经存在的链表   */
    for(; begin != end;)
    {
        struct handle_list_s* temp = (struct handle_list_s*)begin;
        if(temp->type == event_id)
        {
            handlers = temp;
            break;
        }

        begin = begin->next;
    }

    return handlers;
}

static struct observer_list_s*
find_observer_list(struct event_object_s* root, int event_id)
{
    struct observer_list_s* observers = NULL;
    struct double_link_node_s* begin = double_link_begin(&root->observers);
    struct double_link_node_s* end = double_link_end(&root->observers);

    /*  查找已经存在的链表   */
    for(; begin != end;)
    {
        struct observer_list_s* temp = (struct observer_list_s*)begin;
        if(temp->type == event_id)
        {
            observers = temp;
            break;
        }

        begin = begin->next;
    }

    return observers;
}

void 
event_object_insert_observer(struct event_object_s* root, int event_id, struct event_object_s* observer_obj)
{
    struct observer_list_s* observers = find_observer_list(root, event_id);
    struct observer_node_s* node = NULL;

    if(observers == NULL)
    {
        /*  不存在则创建此event类型的链表   */
        observers = (struct observer_list_s*)malloc(sizeof(*observers));
        observers->node.next = NULL;
        observers->node.prior = NULL;
        observers->type = event_id;

        double_link_init(&observers->observers);
        double_link_push_back((struct double_link_s*)&root->observers, (struct double_link_node_s*)observers);
    }

    /*  创建新的节点  */
    node = (struct observer_node_s*)malloc(sizeof(*node));
    node->node.next = NULL;
    node->node.prior = NULL;
    node->obj = observer_obj;

    double_link_push_back(&observers->observers, (struct double_link_node_s*)node);
}

void
event_object_remove_observer(struct event_object_s* root, int event_id, struct event_object_s* observer)
{
    struct observer_list_s* observers = find_observer_list(root, event_id);

    if(observers != NULL)
    {
        struct double_link_node_s* begin_observer = double_link_begin((struct double_link_s*)&observers->observers);
        struct double_link_node_s* end_observer = double_link_end((struct double_link_s*)&observers->observers);

        for(; begin_observer != end_observer;)
        {
            struct observer_node_s* node = (struct observer_node_s*)begin_observer;

            if(node->obj == observer)
            {
                double_link_erase((struct double_link_s*)&observers->observers, (struct double_link_node_s*)node);
                free(node);
                break;
            }

            begin_observer = begin_observer->next;
        }
    }
}

void 
event_object_insert_handler(struct event_object_s* root, int event_id, fpn_event_handler callback, void* event_data)
{
    struct handle_list_s* handlers = find_handle_list(root, event_id);
    struct handler_node_s* node = NULL;

    if(handlers == NULL)
    {
        /*  不存在则创建此event类型的链表   */
        handlers = (struct handle_list_s*)malloc(sizeof(*handlers));
        handlers->node.next = NULL;
        handlers->node.prior = NULL;
        handlers->type = event_id;

        double_link_init(&handlers->handlers);
        double_link_push_back((struct double_link_s*)&root->handlers, (struct double_link_node_s*)handlers);
    }

    /*  创建新的节点  */
    node = (struct handler_node_s*)malloc(sizeof(*node));
    node->node.next = NULL;
    node->node.prior = NULL;
    node->data.callback = callback;
    node->data.data = event_data;

    double_link_push_back(&handlers->handlers, (struct double_link_node_s*)node);
}

void
event_object_remove_handler(struct event_object_s* root, int event_id, fpn_event_handler callback, void* event_data)
{
    struct handle_list_s* handlers = find_handle_list(root, event_id);
    struct handler_node_s* node = NULL;

    if(handlers != NULL)
    {
        struct double_link_node_s* begin_handle = double_link_begin((struct double_link_s*)&handlers->handlers);
        struct double_link_node_s* end_handle = double_link_end((struct double_link_s*)&handlers->handlers);

        for(; begin_handle != end_handle;)
        {
            struct handler_node_s* node = (struct handler_node_s*)begin_handle;
            if(node->data.callback == callback && node->data.data == event_data)
            {
                double_link_erase((struct double_link_s*)&handlers->handlers, (struct double_link_node_s*)node);
                free(node);
                break;
            }

            begin_handle = begin_handle->next;
        }
    }
}

void 
event_object_handler(struct event_object_s* root, struct event_object_s* source, int event_id, const void* arg)
{
    struct handle_list_s* handlers = find_handle_list(root, event_id);

    if(handlers != NULL)
    {
        struct double_link_node_s* begin_handle = double_link_begin((struct double_link_s*)&handlers->handlers);
        struct double_link_node_s* end_handle = double_link_end((struct double_link_s*)&handlers->handlers);

        for(; begin_handle != end_handle;)
        {
            struct handler_node_s* node = (struct handler_node_s*)begin_handle;
            (node->data.callback)(root, source, node->data.data, arg);

            begin_handle = begin_handle->next;
        }
    }
}

void 
event_object_nofify(struct event_object_s* root, int event_id, void* arg)
{
    struct observer_list_s* observers = find_observer_list(root, event_id);

    if(observers != NULL)
    {
        struct double_link_node_s* begin_observer = double_link_begin((struct double_link_s*)&observers->observers);
        struct double_link_node_s* end_observer = double_link_end((struct double_link_s*)&observers->observers);

        for(; begin_observer != end_observer;)
        {
            struct observer_node_s* node = (struct observer_node_s*)begin_observer;
            
            event_object_handler(node->obj, root, event_id, arg);
            begin_observer = begin_observer->next;
        }
    }
}
