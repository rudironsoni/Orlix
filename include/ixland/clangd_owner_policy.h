#ifndef IXLAND_CLANGD_OWNER_POLICY_H
#define IXLAND_CLANGD_OWNER_POLICY_H

#ifdef IXLAND_LINUX_OWNER_FILE

#pragma GCC poison dlsym
#pragma GCC poison RTLD_NEXT
#pragma GCC poison syscall
#pragma GCC poison pthread_t
#pragma GCC poison pthread_mutex_t
#pragma GCC poison pthread_cond_t
#pragma GCC poison NSFileManager
#pragma GCC poison NSURL
#pragma GCC poison Foundation
#pragma GCC poison UIKit
#pragma GCC poison NSObject
#pragma GCC poison NSString
#pragma GCC poison NSArray
#pragma GCC poison NSDictionary
#pragma GCC poison os_log

#endif /* IXLAND_LINUX_OWNER_FILE */

#endif /* IXLAND_CLANGD_OWNER_POLICY_H */
