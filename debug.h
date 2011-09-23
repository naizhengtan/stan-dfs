#ifndef debug_h
#define debug_h

#define DEBUG
#ifdef DEBUG
#define dprintf(x...) printf(x)
#else
#define dprintf(x...)
#endif

#endif
