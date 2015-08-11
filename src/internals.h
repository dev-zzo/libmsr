#ifndef LIBMSR_INTERNALS_H
#define LIBMSR_INTERNALS_H

typedef struct {
    HANDLE PortHandle;
    DCB PortSettings;
} MSRCONTEXT, *LPMSRCONTEXT;

#define ESC 0x1B

#endif /* LIBMSR_INTERNALS_H */
