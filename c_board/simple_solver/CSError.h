#ifndef CSERROR_H
#define CSERROR_H

typedef enum {NO_FAILURE = 0,
              FAILURE    = 1,
              FAIL_ALLOC = 2,
              FAIL_PARAM = 3} CSError;

#define NOFAIL(x) assert(NO_FAILURE == (x))

#endif // CSERROR_H
