osal_status_t osal_event_create(osal_event_handle_t* handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle=NULL;
    osal_enter_critical();
    for(unsigned i=0;i<OSAL_MAX_EVENTS;++i){
        bare_event_t*e=&s_events[i];if(e->used)continue;
        void*token=bare_token(OSAL_TYPE_EVENT);if(!token)break;
        *e=(bare_event_t){.used=true,.token=token};*handle=token;BARE_COUNT_INC(event);
        osal_exit_critical();return OSAL_OK;
    }
    osal_exit_critical();return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_event_delete(osal_event_handle_t h){BARE_TASK_ONLY();if(!h)return OSAL_ERROR_NULL_POINTER;osal_enter_critical();bare_event_t*e=bare_event(h);if(!e){osal_exit_critical();return OSAL_ERROR_INVALID_PARAM;}e->used=false;--s_stats.event_count;osal_exit_critical();return OSAL_OK;}
static osal_status_t bare_event_update(osal_event_handle_t h,uint32_t bits,bool set){if(!h)return OSAL_ERROR_NULL_POINTER;if(!bits||(bits&~OSAL_EVENT_BITS_MASK))return OSAL_ERROR_INVALID_PARAM;osal_enter_critical();bare_event_t*e=bare_event(h);if(!e){osal_exit_critical();return OSAL_ERROR_INVALID_PARAM;}e->bits=set?e->bits|bits:e->bits&~bits;osal_exit_critical();return OSAL_OK;}
osal_status_t osal_event_set(osal_event_handle_t h,uint32_t bits){BARE_TASK_ONLY();return bare_event_update(h,bits,true);}
osal_status_t osal_event_clear(osal_event_handle_t h,uint32_t bits){BARE_TASK_ONLY();return bare_event_update(h,bits,false);}
osal_status_t osal_event_set_from_isr(osal_event_handle_t h,uint32_t bits){return bare_event_update(h,bits,true);}
osal_status_t osal_event_clear_from_isr(osal_event_handle_t h,uint32_t bits){return bare_event_update(h,bits,false);}
osal_event_bits_t osal_event_get(osal_event_handle_t h){osal_enter_critical();bare_event_t*e=bare_event(h);uint32_t bits=e?e->bits:0;osal_exit_critical();return bits;}
static osal_status_t bare_event_wait(osal_event_handle_t h,uint32_t set,uint32_t bits,const osal_event_wait_options_t*opts,uint32_t*out){
    BARE_TASK_ONLY();if(out)*out=0;if(!h||!opts)return OSAL_ERROR_NULL_POINTER;
    if(!bits||((bits|set)&~OSAL_EVENT_BITS_MASK)||(opts->mode!=OSAL_EVENT_WAIT_ALL&&opts->mode!=OSAL_EVENT_WAIT_ANY))return OSAL_ERROR_INVALID_PARAM;
    uint32_t begin;osal_status_t setup=bare_wait_setup(opts->timeout_ms,&begin);if(setup!=OSAL_OK)return setup;
    bool seen=false;
    for(;;){
        osal_enter_critical();bare_event_t*e=bare_event(h);
        if(!e){osal_exit_critical();return seen?OSAL_ERROR_CANCELLED:OSAL_ERROR_INVALID_PARAM;}
        seen=true;e->bits|=set;set=0;uint32_t match=e->bits&bits;
        if(opts->mode==OSAL_EVENT_WAIT_ALL?match==bits:match!=0){if(out)*out=match;if(opts->auto_clear)e->bits&=~match;osal_exit_critical();return OSAL_OK;}
        if(out){*out=match;}osal_exit_critical();
        if(bare_expired(opts->timeout_ms,begin))return OSAL_ERROR_TIMEOUT;
        osal_platform_delay_us(1);
    }
}
osal_status_t osal_event_wait(osal_event_handle_t h,uint32_t bits,const osal_event_wait_options_t*opts,uint32_t*out){return bare_event_wait(h,0,bits,opts,out);}
osal_status_t osal_event_sync(osal_event_handle_t h,uint32_t set,uint32_t bits,const osal_event_wait_options_t*opts,uint32_t*out){if(!h||!opts)return OSAL_ERROR_NULL_POINTER;if(!set||(opts&&(opts->mode!=OSAL_EVENT_WAIT_ALL||!opts->auto_clear)))return OSAL_ERROR_INVALID_PARAM;return bare_event_wait(h,set,bits,opts,out);}

/* A baremetal product owns its event loop. Task and timer services require the
 * FreeRTOS backend or a separately implemented product scheduler. */
osal_status_t osal_task_create(const osal_task_config_t*c,osal_task_handle_t*h){if(!c||!h)return OSAL_ERROR_NULL_POINTER;*h=NULL;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_task_delete(osal_task_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_task_request_stop(osal_task_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
bool osal_task_should_stop(void){return false;}
osal_status_t osal_task_join(osal_task_handle_t h,uint32_t ms){(void)h;(void)ms;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_task_suspend(osal_task_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_task_resume(osal_task_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_task_delay(uint32_t ms){BARE_TASK_ONLY();if(ms==OSAL_WAIT_FOREVER)return OSAL_ERROR_INVALID_PARAM;uint32_t begin;osal_status_t setup=bare_wait_setup(ms,&begin);if(setup!=OSAL_OK)return setup;while(!bare_expired(ms,begin))osal_platform_delay_us(1);return OSAL_OK;}
osal_status_t osal_task_yield(void){BARE_TASK_ONLY();osal_platform_delay_us(1);return OSAL_OK;}
osal_task_handle_t osal_task_get_current(void){return NULL;}
const char* osal_task_get_name(osal_task_handle_t h){(void)h;return NULL;}
uint8_t osal_task_get_priority(osal_task_handle_t h){(void)h;return 0;}
osal_status_t osal_task_set_priority(osal_task_handle_t h,uint8_t p){(void)h;(void)p;return OSAL_ERROR_NOT_SUPPORTED;}
size_t osal_task_get_stack_watermark(osal_task_handle_t h){(void)h;return 0;}
osal_task_state_t osal_task_get_state(osal_task_handle_t h){(void)h;return OSAL_TASK_STATE_DELETED;}
osal_status_t osal_timer_create(const osal_timer_config_t*c,osal_timer_handle_t*h){if(!c||!h)return OSAL_ERROR_NULL_POINTER;*h=NULL;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_timer_delete(osal_timer_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_timer_start(osal_timer_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_timer_stop(osal_timer_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_timer_reset(osal_timer_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_timer_set_period(osal_timer_handle_t h,uint32_t ms){(void)h;(void)ms;return OSAL_ERROR_NOT_SUPPORTED;}
bool osal_timer_is_active(osal_timer_handle_t h){(void)h;return false;}
osal_status_t osal_timer_start_from_isr(osal_timer_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_timer_stop_from_isr(osal_timer_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_timer_reset_from_isr(osal_timer_handle_t h){(void)h;return OSAL_ERROR_NOT_SUPPORTED;}
uint32_t osal_timer_get_remaining(osal_timer_handle_t h){(void)h;return 0;}
uint32_t osal_timer_get_period(osal_timer_handle_t h){(void)h;return 0;}
osal_status_t osal_timer_set_callback(osal_timer_handle_t h,osal_timer_callback_t cb,void*a){(void)h;(void)cb;(void)a;return OSAL_ERROR_NOT_SUPPORTED;}
/* Control-domain resources are static. This backend provides no heap. */
osal_status_t osal_mem_seal(void){return OSAL_ERROR_NOT_SUPPORTED;}
bool osal_mem_is_sealed(void){return false;}
void* osal_mem_alloc(size_t size){(void)size;return NULL;}
void* osal_mem_calloc(size_t count,size_t size){(void)count;(void)size;return NULL;}
void* osal_mem_realloc(void*p,size_t size){(void)p;(void)size;return NULL;}
void* osal_mem_alloc_aligned(size_t alignment,size_t size){(void)alignment;(void)size;return NULL;}
void osal_mem_free(void*p){if(p)osal_report_error(OSAL_ERROR_NOT_SUPPORTED,__FILE__,__LINE__);}
void osal_mem_free_aligned(void*p){osal_mem_free(p);}
osal_status_t osal_mem_get_stats(osal_mem_stats_t*s){if(!s)return OSAL_ERROR_NULL_POINTER;*s=(osal_mem_stats_t){0};return OSAL_ERROR_NOT_SUPPORTED;}
size_t osal_mem_get_free_size(void){return 0;}
size_t osal_mem_get_min_free_size(void){return 0;}
size_t osal_mem_get_allocation_count(void){return 0;}
osal_status_t osal_mem_check_integrity(void){return OSAL_ERROR_NOT_SUPPORTED;}
osal_status_t osal_get_stats(osal_stats_t*s){if(!s)return OSAL_ERROR_NULL_POINTER;osal_enter_critical();*s=s_stats;osal_exit_critical();return OSAL_OK;}
osal_status_t osal_reset_stats(void){osal_enter_critical();s_stats.mutex_watermark=s_stats.mutex_count;s_stats.sem_watermark=s_stats.sem_count;s_stats.queue_watermark=s_stats.queue_count;s_stats.event_watermark=s_stats.event_count;osal_exit_critical();return OSAL_OK;}
osal_status_t osal_set_error_callback(osal_error_callback_t cb){osal_enter_critical();s_error_callback=cb;osal_exit_critical();return OSAL_OK;}
osal_error_callback_t osal_get_error_callback(void){osal_enter_critical();osal_error_callback_t cb=s_error_callback;osal_exit_critical();return cb;}
void osal_report_error(osal_status_t e,const char*f,uint32_t l){osal_error_callback_t cb=osal_get_error_callback();if(cb&&e!=OSAL_OK)cb(e,f,l);}
void osal_assert_failed(const char*f,uint32_t l){osal_report_error(OSAL_ERROR,f,l);}
