#ifndef __LEAKTRACE_L_h_included__
#define __LEAKTRACE_L_h_included__

#ifndef TRACE
#ifdef LOGGER
#define TRACE(arg) fprintf arg
#else
#define TRACE(arg)
#endif
#endif

#define LEAKTRACER_VERSION "3.0.0"

#endif /* __LEAKTRACE_L_h_included__ */
