
#ifndef COMMON_HEADER_H
#define COMMON_HEADER_H

struct event {
	__u64 ts;
	__u64 flowid;
	__u64 counter;
};

#endif // COMMON_HEADER_H
