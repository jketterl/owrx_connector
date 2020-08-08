#ifndef SHIMS_H
#define SHIMS_H

#ifdef __APPLE__
#define MSG_NOSIGNAL 0x2000 /* don't raise SIGPIPE */
#endif	// __APPLE__

#endif
