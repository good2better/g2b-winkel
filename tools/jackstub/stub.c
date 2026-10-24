/* mini libjack stub: alle functies die DPF JACK-standalone nodig heeft */
#include <stdint.h>
#include <stdlib.h>
typedef void* jack_client_t;
typedef void* jack_port_t;
typedef uint32_t jack_nframes_t;
typedef int (*JackProcessCallback)(jack_nframes_t,void*);
typedef void (*JackShutdownCallback)(void*);
typedef float jack_default_audio_sample_t;
jack_client_t jack_client_open(const char* n, int o, int* s){ return malloc(1); }
int jack_client_close(jack_client_t c){ free(c); return 0; }
const char* jack_client_name(){ return "g2b-stub"; }
jack_port_t jack_port_register(jack_client_t c,const char* n,const char* t,unsigned long f,unsigned long b){ return malloc(1); }
int jack_port_unregister(jack_client_t c,jack_port_t p){ free(p); return 0; }
void* jack_port_get_buffer(jack_port_t p,jack_nframes_t n){ return calloc(n>0?n:2048,sizeof(float)); }
int jack_set_process_callback(jack_client_t c,JackProcessCallback cb,void* a){ return 0; }
int jack_set_sample_rate_callback(jack_client_t c,int cb,void* a){ return 0; }
int jack_set_buffer_size_callback(jack_client_t c,int cb,void* a){ return 0; }
void jack_on_shutdown(jack_client_t c,JackShutdownCallback cb,void* a){}
int jack_activate(jack_client_t c){ return 0; }
int jack_deactivate(jack_client_t c){ return 0; }
jack_nframes_t jack_get_sample_rate(jack_client_t c){ return 44100; }
jack_nframes_t jack_get_buffer_size(jack_client_t c){ return 512; }
int jack_connect(jack_client_t c,const char* s,const char* d){ return 0; }
const char** jack_get_ports(jack_client_t c,const char* p,const char* t,unsigned long f){ return 0; }
void jack_free(void* p){}
jack_nframes_t jack_frame_time(jack_client_t c){ return 0; }
jack_nframes_t jack_last_frame_time(jack_client_t c){ return 0; }
float jack_cpu_load(jack_client_t c){ return 0.f; }
int jack_set_freewheel(jack_client_t c,int on){ return 0; }
void* jack_set_error_function(void* f){ return 0; }
void* jack_set_info_function(void* f){ return 0; }
