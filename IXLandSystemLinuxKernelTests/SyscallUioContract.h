#ifndef SYSCALL_UIO_CONTRACT_H
#define SYSCALL_UIO_CONTRACT_H

#ifdef __cplusplus
extern "C" {
#endif

int syscall_uio_contract_readv_writev_round_trip(void);
int syscall_uio_contract_rejects_invalid_iov_count(void);

#ifdef __cplusplus
}
#endif

#endif
